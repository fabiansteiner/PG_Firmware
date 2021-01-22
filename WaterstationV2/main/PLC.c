
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

#include "PLC.h"
#include "variablepool.h"
#include "calculateVolumetricContent.h"
static const char *TAG = "PLC_TASK";
TaskHandle_t plcHandle = NULL;
QueueHandle_t wateringQueue;
uint8_t watering_STATE = STATE_NOWATERING;

#define PLC_TXD 17
#define PLC_RXD 16
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define UART1_PORT_NUM      1
#define UART1_BAUD_RATE     2400
#define PLC_TASK_STACK_SIZE    4096
#define PLC_TASK_PRIORITY    10

#define BUF_SIZE (1024)

plant * plantBufferReference;
int plantListPointer = 0;
bool plantListPointerOverflow = false;

uint8_t messageToSend[8] = {0};
uint8_t receivedMessage[BUF_SIZE] = {0};


plant getNextPlant();
uint8_t getCheckSum(uint8_t* message, int beginAt);
void writeToPLCLine(plant p, uint8_t command);
char readFromPLCLine(plant p, uint8_t command);
uint8_t getNextFreePlantListAddress();


static void plc_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */

     uart_config_t uart1_config = {
        .baud_rate = UART1_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(UART1_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART1_PORT_NUM, &uart1_config));
    ESP_ERROR_CHECK(uart_set_pin(UART1_PORT_NUM, PLC_TXD, PLC_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    const TickType_t xDelay = 2000 / portTICK_PERIOD_MS;

    plant chosenPlant;
    wateringJob wJob;
    uint8_t command = READSTATUS;

    while (1) {

        if(wateringQueue!=0){
            if(xQueueReceive(wateringQueue, &wJob, (TickType_t) 20) == pdTRUE){
                watering_STATE = STATE_LOCKINGVALVES;
                plantListPointer = 0;
                plantListPointerOverflow = false;
            }
        }
        
        if(watering_STATE == STATE_NOWATERING){
            if(plantListPointerOverflow == true){ //If Overflow
                plantListPointerOverflow = false;
                if(getNextFreePlantListAddress() != PLANTSIZE){ //Try to register new PLCValves - only if maximum Plantsize not reached yet
                    chosenPlant.address = UNREGISTEREDADDRESS;
                    command = BROADCASTVALVEADDRESS;
                    writeToPLCLine(chosenPlant, command);
                    ESP_LOGI(TAG,"Broadcast for new PLCValves!");
                }
            }else{  //Make the normal request rounds
                chosenPlant = getNextPlant();
                command = READSTATUS;
                writeToPLCLine(chosenPlant, command);
                ESP_LOGI(TAG,"Requesting PLCValve: %u for new State!", chosenPlant.address);
            }
        }else if (watering_STATE == STATE_LOCKINGVALVES){

        }else if (watering_STATE == STATE_OPENINGVALVE){

        }else if (watering_STATE == STATE_WATERING){

        } else if (watering_STATE == STATE_CLOSINGVAVLE){

        }
        
        //Wait for PLC Uart to transmit data + Wait for answer from slaves
        vTaskDelay( xDelay );

        char receiveResult = readFromPLCLine(chosenPlant, command);
        // Read data from PLC Line
        
    }
}

void writeToPLCLine(plant p, uint8_t command){
     if(p.address != NONEXISTINGADDRESS){
        memset(messageToSend, 0, sizeof(messageToSend));
        messageToSend[0] = p.address;
        messageToSend[1] = ~p.address;
        messageToSend[2] = command;
        if(command == BROADCASTVALVEADDRESS){
            messageToSend[3] = getNextFreePlantListAddress();
        }
        messageToSend[7] = getCheckSum(messageToSend, 0);
        
        uart_write_bytes(UART1_PORT_NUM, (const char *) messageToSend, 8);
        //ESP_LOGI(TAG, "Sent Request to PLC Valve with Address: %u\n\r", chosenPlant.address);
     }
}

char readFromPLCLine(plant p, uint8_t command){
    char acknoledge = NOANSWER;
    uint8_t invertedAddress;

    int len = uart_read_bytes(UART1_PORT_NUM, receivedMessage, BUF_SIZE, 0);
    if(len != 0 && len != -1){
        if(len <= 8){
            ESP_LOGI(TAG,"NO Answer received!\n");
        }else if (len == 16){
            //Build address to compare with
            if(command == BROADCASTVALVEADDRESS){
                p.address = receivedMessage[3];
            }
            invertedAddress = ~p.address;

            //If Address is correct, and checksum is correct --> Message is VALID!
            if(receivedMessage[8] == p.address && receivedMessage[9] == invertedAddress && receivedMessage[15] == getCheckSum(receivedMessage, 8)){
                if(command == BROADCASTVALVEADDRESS){
                    changePlant(p, CHANGE_ADD);
                }
                acknoledge = receivedMessage[13];
                if(acknoledge == RES){
                    changePlant(p, CHANGE_REMOVE);
                }
                ESP_LOGI(TAG,"CORRECT ANSWER!");
                int waterContent = getWaterContent(receivedMessage[10], receivedMessage[11]);
                p.soilMoisture = waterContent;
                p.valveStatus = receivedMessage[12];
                ESP_LOGI(TAG,"Water Content of PLC Valve %u is: %i\n", p.address, waterContent);
            }else{
                ESP_LOGI(TAG,"WRONG ANSWER!");
            }
        }else{
            ESP_LOGI(TAG,"ANSWER WITH INCORRECT LENGHT");
        }
        memset(receivedMessage, 0, sizeof(receivedMessage));    //Reset buffer
    }else if(len == -1 || len == 0){
        ESP_LOGI(TAG,"Receive Error Occured");
    }
    if(acknoledge!=NOANSWER)
        changePlant(p, CHANGE_PLCVALVEVALUES);

    return acknoledge;
}



uint8_t getCheckSum(uint8_t* message, int beginAt){
    uint16_t sum = 0;
    for(int i = beginAt; i<(beginAt+7); i++){
        sum += message[i];
    }
    sum = sum % 255;
    return sum;
}

uint8_t getNextFreePlantListAddress(){

    int nextFreePlantListAddress = 0;

    while(plantBufferReference[nextFreePlantListAddress].address != 255){
        nextFreePlantListAddress++;
        if(nextFreePlantListAddress >= PLANTSIZE){
            break;
        }
    }

    return nextFreePlantListAddress;
}


plant getNextPlant(){
    plant nextPlant = {0};  //Initialize empty plant, with all values at 0
    nextPlant.address = NONEXISTINGADDRESS;
    int incrementCounter = 0;

    while(plantBufferReference[plantListPointer].address == UNREGISTEREDADDRESS){

        plantListPointer++;
         if(plantListPointer >= PLANTSIZE){
             plantListPointer = 0;
             plantListPointerOverflow = true;
         }

         incrementCounter++;
         if(incrementCounter >= PLANTSIZE){
             break;
         }
    }

    if(incrementCounter < 100){
        nextPlant = plantBufferReference[plantListPointer];
        plantListPointer++;
    }else{
        plantListPointer = 0;
    }

    return nextPlant;
    
}

void addWateringJob(uint8_t plantAddress, uint16_t waterAmount, uint8_t fertilizerAmount){
    wateringJob job = {plantAddress, waterAmount, fertilizerAmount};
    xQueueSend(wateringQueue, &job, (TickType_t) 20);
}

void initializePLCTask(){

    plantBufferReference = getVariablePool();
    xTaskCreate(plc_task, "plc_task", PLC_TASK_STACK_SIZE, NULL, PLC_TASK_PRIORITY, plcHandle);
    wateringQueue = xQueueCreate(15, sizeof(wateringJob));
}



