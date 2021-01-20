
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
TaskHandle_t plcHandle = NULL;

plant getNextPlant();
uint8_t getCheckSum(uint8_t* message, int beginAt);

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
    uint8_t messageToSend[8] = {0};
    uint8_t receivedMessage[BUF_SIZE] = {0};

    messageToSend[0] = 0b01010110;
    messageToSend[1] = ~(0b01010110);
    messageToSend[2] = 30;


    plant chosenPlant;
    

    while (1) {

        //Write Data to PLC Line
        chosenPlant = getNextPlant();
        if(chosenPlant.address != 0){
            memset(messageToSend, 0, sizeof(messageToSend));
            messageToSend[0] = chosenPlant.address;
            messageToSend[1] = ~chosenPlant.address;
            messageToSend[2] = READSTATUS;
            messageToSend[7] = getCheckSum(messageToSend, 0);
            uart_write_bytes(UART1_PORT_NUM, (const char *) messageToSend, 8);
            //ESP_LOGI(TAG, "Sent Request to PLC Valve with Address: %u\n\r", chosenPlant.address);
        }

        //Wait for PLC Uart to transmit data + Wait for answer from slaves
        vTaskDelay( xDelay );

        
        // Read data from PLC Line
        int len = uart_read_bytes(UART1_PORT_NUM, receivedMessage, BUF_SIZE, 0);
        //ESP_LOGI(TAG," %u Bit received ... check, if answer is correct....\r\n", len);
        if(len != 0 && len != -1){
            if(len <= 8){
                ESP_LOGI(TAG,"NO Answer received!\n");
            }else if (len == 16){
                if(receivedMessage[8] == chosenPlant.address && receivedMessage[9] == messageToSend[1] && receivedMessage[15] == getCheckSum(receivedMessage, 8)){
                    ESP_LOGI(TAG,"CORRECT ANSWER!\n");
                    int waterContent = getWaterContent(receivedMessage[10], receivedMessage[11]);
                    chosenPlant.soilMoisture = waterContent;
                    ESP_LOGI(TAG,"Water Content of PLC Valve %u is: %i", chosenPlant.address, waterContent);
                }else{
                    ESP_LOGI(TAG,"WRONG ANSWER!\n");
                }
            }else{
                ESP_LOGI(TAG,"ANSWER WITH INCORRECT LENGHT\n");
            }
            memset(receivedMessage, 0, sizeof(receivedMessage));    //Reset buffer
        }else if(len == -1){
            ESP_LOGI(TAG,"Receive Error Occured\n");
        }

        changePlant(chosenPlant, CHANGE_PLCVALVEVALUES);
    }
}



uint8_t getCheckSum(uint8_t* message, int beginAt){
    uint16_t sum = 0;
    for(int i = beginAt; i<(beginAt+7); i++){
        sum += message[i];
    }
    sum = sum % 255;
    return sum;
}


plant getNextPlant(){
    plant nextPlant = {0};  //Initialize empty plant, with all values at 0
    int incrementCounter = 0;

    while(plantBufferReference[plantListPointer].address == 0){

        plantListPointer++;
         if(plantListPointer >= PLANTSIZE-1){
             plantListPointer = 0;
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

void initializePLCTask(){

    plantBufferReference = getVariablePool();
    xTaskCreate(plc_task, "plc_task", PLC_TASK_STACK_SIZE, NULL, PLC_TASK_PRIORITY, plcHandle);
}



