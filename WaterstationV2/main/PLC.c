
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>


#include "PLC.h"
#include "calculateVolumetricContent.h"
#include "watering.h"

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
#define PLC_TASK_STACK_SIZE    8192
#define PLC_TASK_PRIORITY    9

#define BUF_SIZE (1024)

plant * plantBufferReference;
int plantListPointer = 0;
bool plantListPointerOverflow = false;
bool awaitAnswer = false;

uint8_t messageToSend[8] = {0};
uint8_t receivedMessage[BUF_SIZE] = {0};

void resetGetNextPlantParameters();
plant getNextPlant();
uint8_t getCheckSum(uint8_t* message, int beginAt);
void writeToPLCLine(plant p, uint8_t command);
char readFromPLCLine(plant p, uint8_t command);
uint8_t getNextFreePlantListAddress();
void abortWatering(plant p);


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

    const TickType_t xDelay = 200 / portTICK_PERIOD_MS;

    plant chosenPlant;
    wateringJob wJob;
    uint8_t command = READSTATUS;
    char plcAnswer = NOANSWER;

    while (1) {

        awaitAnswer = false;

        if(wateringQueue!=0 && watering_STATE == STATE_NOWATERING && xQueuePeek(wateringQueue, &wJob, (TickType_t) 0)==pdTRUE){//If there is something in the watering queue + Not currenctly watering 
            //Pre-check if all registered PLC Valves are in CLOSED state
            int i = 0;
            while((plantBufferReference[i].address != UNREGISTEREDADDRESS && (plantBufferReference[i].valveStatus == CLOSED || plantBufferReference[i].valveStatus == LOCKED)) || plantBufferReference[i].address == UNREGISTEREDADDRESS){
                i++;
                if(i == PLANTSIZE){
                    break;
                }
            }
            if(i == PLANTSIZE){   //If yes: initialize Watering
                if(xQueueReceive(wateringQueue, &wJob, 0) == pdTRUE){
                    watering_STATE = STATE_LOCKINGVALVES;
                    resetGetNextPlantParameters();
                    //Needed for getting the first plant properly
                    plcAnswer = ACK; 
                }
            }else{          //If no: Add Error Message, that one PLC Valve is not closed and delete the watering Job
                xQueueReceive(wateringQueue, &wJob, (TickType_t) 0);
                abortWatering(wJob.plantToWater);
            }
        }
        

        if(watering_STATE == STATE_NOWATERING){
            if(plantListPointerOverflow == true){ //If Overflow
                plantListPointerOverflow = false;
                if(getNextFreePlantListAddress() != PLANTSIZE){ //Try to register new PLCValves - only if maximum Plantsize not reached yet
                    chosenPlant = getNewPlant();
                    chosenPlant.address = UNREGISTEREDADDRESS;
                    command = BROADCASTVALVEADDRESS;
                    writeToPLCLine(chosenPlant, command);
                }else{
                    awaitAnswer = false;
                }
            }else{  //Make the normal request rounds
                chosenPlant = getNextPlant();
                command = READSTATUS;
                writeToPLCLine(chosenPlant, command);
            }




        }else if (watering_STATE == STATE_LOCKINGVALVES){  

            if(plcAnswer == ACK || plcAnswer == RES){
                chosenPlant = getNextPlant();
                if(plantListPointerOverflow != true){
                    command = LOCKCOMMAND; writeToPLCLine(chosenPlant, command);
                }else{
                    ESP_LOGI(TAG,"Successfully locked all valves, continue with opening...");
                    watering_STATE = STATE_OPENINGVALVE;
                    while(plantBufferReference[wJob.plantToWater.address].valveStatus != LOCKED){vTaskDelay(1 / portTICK_PERIOD_MS);}
                    chosenPlant = wJob.plantToWater;
                    resetGetNextPlantParameters();
                }  
            }else if(plcAnswer == NOANSWER){
                command = LOCKCOMMAND; writeToPLCLine(chosenPlant, command);
            }else if (plcAnswer == NACK){
                resetGetNextPlantParameters();
                watering_STATE = STATE_NOWATERING;
                abortWatering(chosenPlant);
                ESP_LOGI(TAG,"A Valve could not be locked... abort watering.");
            }

            




        }else if (watering_STATE == STATE_OPENINGVALVE){

            if(plantBufferReference[chosenPlant.address].valveStatus == LOCKED){
                command = OPENVALVE;
                writeToPLCLine(chosenPlant, command);
                vTaskDelay(700/ portTICK_PERIOD_MS);            //Quickfix: Extra wait is needed when opening for whatever reason... if not--> Valve puts TX down mid send and lets it there (maybe has to do something with lock (no it has not, idk then))
            }else if(plantBufferReference[chosenPlant.address].valveStatus == OPENING){
                command = READSTATUSWITHOUTUNLOCK;
                writeToPLCLine(chosenPlant, command);
            }else if(plantBufferReference[chosenPlant.address].valveStatus == OPEN){
                watering_STATE = STATE_WATERING;
                chosenPlant.wateringStatus = STATUS_WATERING;
                changePlant(chosenPlant, CHANGE_WATERINGSTATUS);
                while(plantBufferReference[chosenPlant.address].wateringStatus != STATUS_WATERING){vTaskDelay(1 / portTICK_PERIOD_MS);}
                startWateringTask(wJob);
            }else{
                ESP_LOGI(TAG,"Something went wrong, watering aborted. Valve state was: %u", plantBufferReference[chosenPlant.address].valveStatus);
                resetGetNextPlantParameters();
                watering_STATE = STATE_NOWATERING;
                abortWatering(chosenPlant);
            }
            
            
        }else if (watering_STATE == STATE_WATERING){
            //Make Watering shizzle - when done, continue with closing valve
            if(plantBufferReference[chosenPlant.address].wateringStatus == STATUS_WATERING){
                command = READSTATUS; writeToPLCLine(chosenPlant, command);
            }else{
                watering_STATE = STATE_CLOSINGVALVE;
            }


        
        }else if (watering_STATE == STATE_CLOSINGVALVE){
             if(plantBufferReference[chosenPlant.address].valveStatus == OPEN){
                command = CLOSEVALVE;
                writeToPLCLine(chosenPlant, command);
                vTaskDelay(700/ portTICK_PERIOD_MS);
            }else if(plantBufferReference[chosenPlant.address].valveStatus == CLOSING){
                command = READSTATUSWITHOUTUNLOCK;
                writeToPLCLine(chosenPlant, command);
            }else if(plantBufferReference[chosenPlant.address].valveStatus == CLOSED){
                watering_STATE = STATE_NOWATERING;
            }else{
                ESP_LOGI(TAG,"Something went wrong, watering aborted. Valve state was: %u", plantBufferReference[chosenPlant.address].valveStatus);
                resetGetNextPlantParameters();
                watering_STATE = STATE_NOWATERING;
                abortWatering(chosenPlant);
            }
        }

        plcAnswer = NOANSWER;
        
        if(awaitAnswer == true){
            //Wait for PLC Uart to transmit data + Wait for answer from slaves
            vTaskDelay( xDelay );
            
            // Read data from PLC Line
            plcAnswer = readFromPLCLine(chosenPlant, command);

            if(plcAnswer == NOANSWER){
                changePlant(chosenPlant, CHANGE_INCREASEUNSUCCESSFULREQUESTS);
                if(watering_STATE != STATE_NOWATERING && plantBufferReference[chosenPlant.address].valveStatus == OFFLINE){
                    watering_STATE = STATE_NOWATERING;
                    resetGetNextPlantParameters();
                    abortWatering(chosenPlant);
                }
                
            }

        }
        vTaskDelay(10/ portTICK_PERIOD_MS);
        
        
    }
}

void abortWatering(plant p){
    p.wateringStatus = STATUS_NOTHINSCHEDULED;
    changePlant(p, CHANGE_WATERINGSTATUS);
}

void writeToPLCLine(plant p, uint8_t command){
    if(p.address != NONEXISTINGADDRESS){
        awaitAnswer = true;

        switch (command){
        case READSTATUS: ESP_LOGI(TAG,"Requesting PLCValve: %u for new State!", p.address);
            break;
        case BROADCASTVALVEADDRESS: ESP_LOGI(TAG,"Broadcast for new PLCValves!");
            break;
        case LOCKCOMMAND: ESP_LOGI(TAG,"Trying to lock PLCValve: %u", p.address);
            break;
        case OPENVALVE: ESP_LOGI(TAG,"Trying to open Valve %u", p.address);
            break;
        default: ESP_LOGI(TAG,"Writing something to PLC Valves!");
            break;
        }

    
        memset(messageToSend, 0, sizeof(messageToSend));
        messageToSend[0] = p.address;
        messageToSend[1] = ~p.address;
        messageToSend[2] = command;
        if(command == BROADCASTVALVEADDRESS){
            messageToSend[3] = getNextFreePlantListAddress();
        }
        messageToSend[7] = getCheckSum(messageToSend, 0);
        
        uart_write_bytes(UART1_PORT_NUM, (const char *) messageToSend, 8);
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
            invertedAddress = 255-p.address;

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
                //ESP_LOGI(TAG,"Water Content of PLC Valve %u is: %i\n", p.address, waterContent);
                ESP_LOGI(TAG,"Valve State of PLC Valve %u is: %u\n", p.address, p.valveStatus);
            }else{
                ESP_LOGI(TAG,"WRONG ANSWER!");
                ESP_LOGI(TAG,"%u" ,receivedMessage[8]);
                ESP_LOGI(TAG,"%u" ,receivedMessage[9]);
                ESP_LOGI(TAG,"%u" ,receivedMessage[10]);
                ESP_LOGI(TAG,"%u" ,receivedMessage[15]);
                ESP_LOGI(TAG,"%u" ,getCheckSum(receivedMessage, 8));
                
            }
        }else{
            ESP_LOGI(TAG,"ANSWER WITH INCORRECT LENGHT");
        }
        memset(receivedMessage, 0, sizeof(receivedMessage));    //Reset buffer
    }else if(len == -1 || len == 0){
        ESP_LOGI(TAG,"Receive Error Occured with len: %i", len);
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

void resetGetNextPlantParameters(){
    plantListPointer = 0;
    plantListPointerOverflow = false;
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


void addWateringJob(plant plantToWater, uint16_t waterAmount, uint8_t fertilizerAmount){
    plantToWater.wateringStatus = STATUS_IN_QUEUE;
    changePlant(plantToWater, CHANGE_WATERINGSTATUS);
    wateringJob job = {plantToWater, waterAmount, fertilizerAmount};
    xQueueSend(wateringQueue, &job, (TickType_t) 20);
}

void initializePLCTask(){
    initializeWateringComponents(false);

    plantBufferReference = getVariablePool();
    xTaskCreate(plc_task, "plc_task", PLC_TASK_STACK_SIZE, NULL, PLC_TASK_PRIORITY, plcHandle);
    wateringQueue = xQueueCreate(15, sizeof(wateringJob));
    //vTaskDelay(10000 / portTICK_PERIOD_MS);
    //ESP_LOGI(TAG, "ADDING WATERING JOB BROOOOOOOOOOOOOOOOOOOOOOOOO");
    //addWateringJob(plantBufferReference[0],500, 5);
}



