
/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#include "PLC.h"
#include "variablepool.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <string.h>

#define DEBUG_TXD 4
#define DEBUG_RXD 5
#define PLC_TXD 17
#define PLC_RXD 16
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define UART0_PORT_NUM      0
#define UART1_PORT_NUM      1
#define UART0_BAUD_RATE     9600
#define UART1_BAUD_RATE     2400
#define PLC_TASK_STACK_SIZE    2048
#define PLC_TASK_PRIORITY    10

#define BUF_SIZE (1024)

plant * plantBufferReference;
int plantListPointer = 0;
TaskHandle_t plcHandle = NULL;

plant getNextPlant();


static void plc_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart0_config = {
        .baud_rate = UART0_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

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

    ESP_ERROR_CHECK(uart_driver_install(UART0_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART0_PORT_NUM, &uart0_config));
    ESP_ERROR_CHECK(uart_set_pin(UART0_PORT_NUM, DEBUG_TXD, DEBUG_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));
    ESP_ERROR_CHECK(uart_driver_install(UART1_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART1_PORT_NUM, &uart1_config));
    ESP_ERROR_CHECK(uart_set_pin(UART1_PORT_NUM, PLC_TXD, PLC_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Configure a temporary buffer for the incoming data
    //uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

    const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
    char* hello = "Motherfucker\r\n";
    char messageToSend[8] = {0};
    uint8_t receivedMessage[BUF_SIZE] = {0};

    messageToSend[0] = 0b01010110;
    messageToSend[1] = ~(0b01010110);
    messageToSend[2] = 30;

    int writeState = 1;

    while (1) {
       

        if(writeState == 1){
            plant chosenPlant = getNextPlant();
            if(chosenPlant.address != 0){
                messageToSend[0] = chosenPlant.address;
                messageToSend[1] = ~chosenPlant.address;
                messageToSend[2] = 30;

                uart_write_bytes(UART0_PORT_NUM, (const char *) hello, 14);
                uart_write_bytes(UART1_PORT_NUM, (const char *) messageToSend, 8);

                writeState = 0;
            }
        }else{
            writeState = 1;
             // Read data from the UART
            int len = uart_read_bytes(UART1_PORT_NUM, receivedMessage, BUF_SIZE, 20 / portTICK_RATE_MS);
            if(len != 0 && len != -1){
                uart_write_bytes(UART0_PORT_NUM, (const char *) receivedMessage, len);
                memset(receivedMessage, 0, sizeof(receivedMessage));    //Reset buffer
            }
        }
        vTaskDelay( xDelay );

        
    }
}

void initializePLCTask(){

    plantBufferReference = getVariablePool();
    xTaskCreate(plc_task, "plc_task", PLC_TASK_STACK_SIZE, NULL, PLC_TASK_PRIORITY, plcHandle);
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