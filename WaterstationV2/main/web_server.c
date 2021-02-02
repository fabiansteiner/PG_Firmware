#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include <esp_http_server.h>

#include "wifi.h"
#include "variablepool.h"
#include "jsonParser.h"


static const char *TAG = "WebSocket Server";
SemaphoreHandle_t xMutexTriggerAsync = NULL;
static httpd_handle_t server = NULL;
plant * plantBufferReference;

struct async_resp_arg asyncResponseConnection;
bool connectedWithSomething = false;

static esp_err_t trigger_async_send_bro(char *message);

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    char jsonToSend[512];
};
    

esp_err_t plantChangedNotification(plant p){
    esp_err_t esperr = ESP_OK;
    if(connectedWithSomething){
        if(xSemaphoreTake( xMutexTriggerAsync, portMAX_DELAY) == pdTRUE){
            errorStates er = {0};
            uint8_t fail = buildJsonString(p, er, true);
            if(fail == 0){
                esperr = trigger_async_send_bro(getSendBuffer());
            }
            xSemaphoreGive(xMutexTriggerAsync);
        }
    }
    return esperr;
}

esp_err_t errorStateChangedNotification(errorStates errStates){
    esp_err_t esperr = ESP_OK;
    if(connectedWithSomething){
        if(xSemaphoreTake( xMutexTriggerAsync, portMAX_DELAY) == pdTRUE){
            plant p = {0};
            uint8_t fail = buildJsonString(p, errStates, false);
            if(fail == 0){
                //esperr = trigger_async_send_bro(getSendBuffer());
            }
            xSemaphoreGive(xMutexTriggerAsync);
        }
    }
    return esperr;
}

esp_err_t sendPlantsAndErrorState(){
    esp_err_t esperr = ESP_OK;
    for(int i = 0; i<PLANTSIZE; i++){
        if(plantBufferReference[i].address != UNREGISTEREDADDRESS){
            esperr = plantChangedNotification(plantBufferReference[i]);
            if(esperr != ESP_OK)
                return esperr;
        }
    }
    //esperr = errorStateChangedNotification(getErrorStates());

    return esperr;
}


/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)resp_arg->jsonToSend;
    ws_pkt.len = strlen(resp_arg->jsonToSend);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    //ESP_LOGI(TAG, "Sending json data....");
    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send_bro(char *message)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = asyncResponseConnection.hd;
    resp_arg->fd = asyncResponseConnection.fd;
    strcpy(resp_arg->jsonToSend, message);
    //ESP_LOGI(TAG, "FD: %i", resp_arg->fd);
    return httpd_queue_work(asyncResponseConnection.hd, ws_async_send, resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    strcpy(resp_arg->jsonToSend, "Ok :)");
    //ESP_LOGI(TAG, "FD: %i", resp_arg->fd);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t echo_handler(httpd_req_t *req)
{
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
            connectedWithSomething = true;
            ESP_LOGI(TAG, "Target Socket set");
            return sendPlantsAndErrorState();
        }else{
            uint8_t res = parseIncomingString((char *)ws_pkt.payload);
            if(res == 2)
                return sendPlantsAndErrorState();
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


static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
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
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

void initWebSocketServer(){
    xMutexTriggerAsync = xSemaphoreCreateMutex();
    plantBufferReference = getVariablePool();
    

    //Done by wifi.c
    //ESP_ERROR_CHECK(nvs_flash_init());
    //ESP_ERROR_CHECK(esp_netif_init());
    //ESP_ERROR_CHECK(esp_event_loop_create_default());

    initWifi();

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

}