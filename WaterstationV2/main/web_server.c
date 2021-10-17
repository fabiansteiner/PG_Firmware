//Possibly further improvement: track sessions with FD, so multiple clients can connect.
//Only possible if disconnect is tracked: idea here: https://www.esp32.com/viewtopic.php?t=16501&start=20

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include <esp_http_server.h>
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "wifi.h"
#include "variablepool.h"
#include "jsonParser.h"
#include "UserIO.h"


#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE 8192
#define SPIFFS_BASE_PATH  "/webapp"

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

char chunk[SCRATCH_BUFSIZE] = {0};

const uint8_t SENDERRORSTATES = 255;


static const char *TAG = "WebSocketServer";
static httpd_handle_t server = NULL;
plant * plantBufferReference;

struct async_resp_arg asyncResponseConnection;
bool connectedWithSomething = false;

TaskHandle_t webSocketTaskHandle = NULL;
QueueHandle_t wsSendQueue;
//Because if you send more asyncSends to the webserver at once, they will be ignored and not added to the http queue.
SemaphoreHandle_t xSemaphoreNotMoreThan6 = NULL;

uint8_t currentIndexPointer = 0;

static esp_err_t trigger_async_send_bro(uint8_t index);

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    uint8_t sendIndex;
};

void wsSendTask(void * pvParameters){
    //InitializingShizzle

    uint16_t countingSemaphorFullTimer = 0;

    uint8_t index = 0;

    while(1){
        if(uxSemaphoreGetCount(xSemaphoreNotMoreThan6)>0){
            if(xQueueReceive(wsSendQueue, &index, (TickType_t) 20) == pdTRUE){
            
                xSemaphoreTake(xSemaphoreNotMoreThan6, portMAX_DELAY);
                if (trigger_async_send_bro(index) != ESP_OK){
                    xSemaphoreGive(xSemaphoreNotMoreThan6);
                    ESP_LOGW(TAG, "Failed to queue to the http queue.");
                }
                
            }
            countingSemaphorFullTimer = 0;
        }else{
            countingSemaphorFullTimer++;
            if(countingSemaphorFullTimer > 500){//If it is full for 5 full seconds
                xSemaphoreGive(xSemaphoreNotMoreThan6);
                ESP_LOGW(TAG, "Safety Release of Counting semaphor was necessary!!");
            } 
        }
        
        
        

        vTaskDelay(10/ portTICK_PERIOD_MS);
    }
}
    

void plantChangedNotification(plant p, bool initialSend){

    if(connectedWithSomething || initialSend){
        plant * plantPointer = &(plantBufferReference[p.address]);
        
        xQueueSend(wsSendQueue, &(plantPointer->address), (TickType_t) 10);
    }
}

void errorStateChangedNotification(errorStates errStates, bool initialSend){
    if(connectedWithSomething || initialSend){
        xQueueSend(wsSendQueue, &SENDERRORSTATES, (TickType_t) 10);
    }
}

void sendPlantsAndErrorState(){

    errorStateChangedNotification(getErrorStates(), true);

    for(int i = 0; i<PLANTSIZE; i++){
        if(plantBufferReference[i].address != UNREGISTEREDADDRESS){
            plantChangedNotification(plantBufferReference[i], true);
        }
    }

    connectedWithSomething = true;
    
}


/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    uint8_t sendIndex = resp_arg->sendIndex;



    uint8_t fail;
    if(sendIndex != SENDERRORSTATES){
        fail = buildJsonString(sendIndex, true);
    }else{
        fail = buildJsonString(0, false);
    }

    
    if(fail == 0){
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

        ws_pkt.payload = (uint8_t*)getSendBuffer();
        ws_pkt.len = strlen(getSendBuffer());
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        ESP_LOGI(TAG, "Execute Sending json data....");
        httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    }

    
    free(resp_arg);
    xSemaphoreGive(xSemaphoreNotMoreThan6);
}

static esp_err_t trigger_async_send_bro(uint8_t index)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = asyncResponseConnection.hd;
    resp_arg->fd = asyncResponseConnection.fd;
    resp_arg->sendIndex = index;
    ESP_LOGI(TAG, "Queue HTTP Work with FD: %i and index: %i", resp_arg->fd, resp_arg->sendIndex);
    ESP_LOGI(TAG, "Current State of Counting Semaphor: %i", uxSemaphoreGetCount(xSemaphoreNotMoreThan6));
    ESP_LOGI(TAG, "Free Heap: %u", xPortGetFreeHeapSize());
    
    
    return httpd_queue_work(asyncResponseConnection.hd, ws_async_send, resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    //strcpy(resp_arg->jsonToSend, "Ok :)");
    //ESP_LOGI(TAG, "FD: %i", resp_arg->fd);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t echo_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "New echo_handler request with method: %i, Session Socket FD: %i", req->method, httpd_req_to_sockfd(req));
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        //connectedWithSomething = false;
        return ESP_OK;
    }

    uint8_t buf[512] = { 0 };
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 512);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        if(asyncResponseConnection.fd!=httpd_req_to_sockfd(req)){
            asyncResponseConnection.fd=httpd_req_to_sockfd(req);
            asyncResponseConnection.hd=req->handle;
            ESP_LOGI(TAG, "Target Socket set");
            sendPlantsAndErrorState();
            return ESP_OK;
        }else{
            uint8_t res = parseIncomingString((char *)ws_pkt.payload);
            if(res == 2){
                sendPlantsAndErrorState();
                return ESP_OK;
            }
        }

    }

    return ret;
}


static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = echo_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};


static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
        connectedWithSomething = false;
        xQueueReset(wsSendQueue);
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "image/svg+xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    strlcpy(filepath, SPIFFS_BASE_PATH, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/' || strcmp(req->uri, "/dashboard") == 0) {
        strlcat(filepath, "/index.html", sizeof(filepath));
    }else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}


 /* URI handler for getting web server files */
static const httpd_uri_t common_get_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = rest_common_get_handler,
    .user_ctx = NULL
};


static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;


    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &common_get_uri);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    connectedWithSomething = false;
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
    changeUserIOState(SUBJECT_CONNECTED, false);
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
    changeUserIOState(SUBJECT_CONNECTED, true);
}

esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE_PATH,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}



void initWebSocketServer(){
    initJsonParser();

    plantBufferReference = getVariablePool();


    wsSendQueue = xQueueCreate(30, sizeof(uint8_t));
    xTaskCreate(wsSendTask, "ws_Send_Task", 4096, NULL, 5, webSocketTaskHandle);
    xSemaphoreNotMoreThan6 = xSemaphoreCreateCounting(5,5);
    

    //Done by wifi.c
    //ESP_ERROR_CHECK(nvs_flash_init());
    //ESP_ERROR_CHECK(esp_netif_init());
    //ESP_ERROR_CHECK(esp_event_loop_create_default());

    initWifi();
    ESP_ERROR_CHECK(init_fs());


    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

}