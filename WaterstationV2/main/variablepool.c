#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "esp_log.h"

#include "variablepool.h"
#include "FAT_storage.h"
#include "PLC.h"
#include "countDown.h"
#include "web_server.h"


static const char *TAG = "VARIABLEPOOL_TASK";


#define VARIABLE_POOL_TASK_STACK_SIZE    4096
#define VARIABLE_POOL_TASK_PRIORITY    10

TaskHandle_t variablePoolHandle = NULL;
QueueHandle_t plantChangeQueue;
QueueHandle_t errStateChangeQueue;

plant plantList[PLANTSIZE] = {0};
int plantListCounter = 0;

char errorMessage[200] = {0};
errorStates errStates = {0};

void changePlantInternally(plantChange changePlant);
void changeErrorStateInternally(errorChange errChange);

void variablePoolTask(void * pvParameters){
    //InitializingShizzle

    plantChange pc;
    errorChange ec;

    while(1){
        if(xQueueReceive(plantChangeQueue, &pc, (TickType_t) 20) == pdTRUE){
            changePlantInternally(pc);
        }
        if(xQueueReceive(errStateChangeQueue, &ec, (TickType_t) 20) == pdTRUE){
            changeErrorStateInternally(ec);
        }

        vTaskDelay(5/ portTICK_PERIOD_MS);
        
    }
}

/**
 * Initializes the main Variables, reads Data from FAT Partition, if it exists
 * Start RTOS Task
 */
void initializeVariablePool(){
    
    //Initialize PlantList
    char plantName[10];
    
    for(uint8_t i=0; i< PLANTSIZE; i++){
        sprintf(plantName, "plant%u", i);
        plant initPlant = getNewPlant();
        strcpy(initPlant.name, plantName);
        plantList[i] = initPlant;
    }

    errorStates initStates;
    initStates.notEnoughWaterFlow = false;
    initStates.waterPressureHigh = false;
    initStates.oneOrMoreValveErrors = false;
    initStates.oneOrMoreValvesNotClosed = false;
    initStates.oneOrMoreValvesOffline = false;

    errStates = initStates;
    


    //Dummy Data 
    //plant dummyPlant1 = {0b01010110, "Plant86", 300, 0, 0, 50, STATUS_OK, UNKNOWN ,0, {0}};
    //plant dummyPlant2 = {0b11001100, "Plant2", 1000, 5, 70.0f, 50, 1, 0};

    //plantList[86] = dummyPlant1;
    //plantList[1] = dummyPlant2;
    
    plantChangeQueue = xQueueCreate(32, sizeof(plantChange));
    errStateChangeQueue = xQueueCreate(15, sizeof(errorChange));
    xTaskCreate(variablePoolTask, "variable_pool_task", VARIABLE_POOL_TASK_STACK_SIZE, NULL, VARIABLE_POOL_TASK_PRIORITY, variablePoolHandle);

    //Init Fat Storage an read all stored Plants into ram
    initFATStorage();

    initializeCountDown();

}


void changePlant(plant plantToChange, uint8_t parameterType){
    plantChange pc = {plantToChange, parameterType};
    xQueueSend(plantChangeQueue, &pc, (TickType_t) 0);
    //ESP_LOGI(TAG, "Sent Plant Change Request to Queue\n");
}


void changeErrorState(uint8_t errType, bool newState){
   errorChange ec = {errType, newState};
   xQueueSend(errStateChangeQueue, &ec, (TickType_t) 0);
   ESP_LOGI(TAG, "Sent Error Change request to queue Request to Queue\n");
}


plant * getVariablePool(){
    return plantList;
}

errorStates getErrorStates(){
    return errStates;
}

errorStates * getErrorStatesPointer(){
    return &errStates;
}


plant getNewPlant(){
        
    plant p = {0};
    
    p.address = UNREGISTEREDADDRESS;
    p.waterAmount = 300;
    p.fertilizerAmount = 0;
    p.threshold = 25;
    p.valveStatus = UNKNOWN;
    p.soilMoisture = UNKNOWNSOILMOISTURE;
    p.autoWatering = 0;
    p.wateringStatus = STATUS_NOTHINSCHEDULED;
    p.unsuccessfulRequests = 0;
    p.safetyMinutesLeft = 0;
    p.safetyTimeActive = 0;
    p.type = 7;
    p.waitTime = CLASSICSAFETYWAIT;

    return p;
}


void changeValveStatusErrorStates(){
    bool atLeastOneValveOffline = false;
    bool atLeastOneValveNotClosed = false;
    bool atLeastOneValveCurrSenseError = false;

    for(int i = 0; i<PLANTSIZE; i++){
        if(plantList[i].address != UNREGISTEREDADDRESS){
            //if(plantList[i].valveStatus != CLOSED && plantList[i].valveStatus != CURRSENSEERROR && plantList[i].valveStatus != OFFLINE && plantList[i].valveStatus != UNKNOWN){
            if(plantList[i].valveStatus == MANUALOPEN || plantList[i].valveStatus == OPENINGMANUALLY || plantList[i].valveStatus == CLOSINGMANUALLY){
                atLeastOneValveNotClosed = true;
            }else if(plantList[i].valveStatus == CURRSENSEERROR){
                atLeastOneValveCurrSenseError = true;
            }else if(plantList[i].valveStatus == OFFLINE){
                atLeastOneValveOffline = true;
            }
        }
    }
    if(errStates.oneOrMoreValveErrors != atLeastOneValveCurrSenseError || errStates.oneOrMoreValvesNotClosed != atLeastOneValveNotClosed || errStates.oneOrMoreValvesOffline != atLeastOneValveOffline){
        //If something changed
        errStates.oneOrMoreValveErrors = atLeastOneValveCurrSenseError;
        errStates.oneOrMoreValvesNotClosed = atLeastOneValveNotClosed;
        errStates.oneOrMoreValvesOffline = atLeastOneValveOffline;
        errorStateChangedNotification(errStates);
    }
    
}

bool areErrorStatesGood(){
    if(errStates.notEnoughWaterFlow || errStates.oneOrMoreValveErrors || errStates.oneOrMoreValvesNotClosed || errStates.oneOrMoreValvesOffline || errStates.waterPressureHigh){
        return false;
    }
    return true;
}

void decreaseSafetyMinutesByOne(){
    for(int i = 0; i<PLANTSIZE; i++){
        if(plantList[i].safetyTimeActive == 1){
            if(plantList[i].safetyMinutesLeft >=1)
                plantList[i].safetyMinutesLeft--;
            if(plantList[i].safetyMinutesLeft == 0){
                plantList[i].safetyTimeActive = 0;
                savePlantToStorage(plantList[i]);
            }
        }
    }
}

void changePlantInternally(plantChange changePlant){
    //ESP_LOGI(TAG, "Entered changePlantInternally...\n");
    int index = changePlant.plantToChange.address;
    bool triggerUpdate = true;
    switch (changePlant.parameterType)
    {
    case CHANGE_ADD:
        ESP_LOGI(TAG, "Adding new Plant");
        char plantName[10];
        sprintf(plantName, "plant%u", changePlant.plantToChange.address);
        strcpy(changePlant.plantToChange.name, plantName);
        plantList[index] = changePlant.plantToChange;
        changeValveStatusErrorStates();
        savePlantToStorage(plantList[index]);
        break;

    case CHANGE_ADDFROMFAT:
        ESP_LOGI(TAG, "Getting plant from FAT Storage");
        plantList[index].address = changePlant.plantToChange.address;
        plantList[index].autoWatering = changePlant.plantToChange.autoWatering;
        plantList[index].waterAmount = changePlant.plantToChange.waterAmount;
        plantList[index].fertilizerAmount = changePlant.plantToChange.fertilizerAmount;
        strcpy(plantList[index].name, changePlant.plantToChange.name);
        plantList[index].threshold = changePlant.plantToChange.threshold;
        plantList[index].type = changePlant.plantToChange.type;
        plantList[index].waitTime = changePlant.plantToChange.waitTime;
        plantList[index].safetyTimeActive = changePlant.plantToChange.safetyTimeActive;
        plantList[index].safetyMinutesLeft = changePlant.plantToChange.safetyMinutesLeft;
        plantList[index].valveStatus = OFFLINE;
        //If i directly set it up to Offline, everything crashes
        
        changeValveStatusErrorStates();

        //savePlantToStorage(changePlant.plantToChange);
        break;

    case CHANGE_REMOVE:
        ESP_LOGI(TAG, "Removing Plant");
        removePlantFromStorage(changePlant.plantToChange);
        plantList[index].address = UNREGISTEREDADDRESS;
        changeValveStatusErrorStates();
        break;

    case CHANGE_INCREASEUNSUCCESSFULREQUESTS:
        if(index!=UNREGISTEREDADDRESS && plantList[index].address != UNREGISTEREDADDRESS){
            if(plantList[index].unsuccessfulRequests < PLANTREACHABLETRHESHOLD){
                plantList[index].unsuccessfulRequests++;
            }else{
                plantList[index].valveStatus = OFFLINE;
                changeValveStatusErrorStates();
            }
        }else{triggerUpdate = false;}
            
        
        break;

    case CHANGE_PLCVALVEVALUES:
        if(index!=UNREGISTEREDADDRESS && plantList[index].address != UNREGISTEREDADDRESS){
            //ESP_LOGI(TAG, "Old Soil Moisture: %i, New Soil Moiture: %i", plantList[index].soilMoisture, changePlant.plantToChange.soilMoisture);
            plantList[index].soilMoisture = changePlant.plantToChange.soilMoisture;
            plantList[index].unsuccessfulRequests = 0;
            if(plantList[index].autoWatering == 1){
                if(plantList[index].soilMoisture < plantList[index].threshold && plantList[index].safetyTimeActive == 0){
                    if(plantList[index].wateringStatus == STATUS_NOTHINSCHEDULED && areErrorStatesGood()){
                        plantList[index].wateringStatus = STATUS_IN_QUEUE;
                        addWateringJob(plantList[index], plantList[index].waterAmount, plantList[index].fertilizerAmount);
                    }
                    
                }
            }
            if(plantList[index].valveStatus != changePlant.plantToChange.valveStatus){
                plantList[index].valveStatus = changePlant.plantToChange.valveStatus;
                changeValveStatusErrorStates();
            }
            
            
        }else{triggerUpdate = false;}
        break;
    
    case CHANGE_QUEUEFORWATERING:
        if(index!=UNREGISTEREDADDRESS && plantList[index].address != UNREGISTEREDADDRESS){
            if(plantList[index].wateringStatus == STATUS_NOTHINSCHEDULED && areErrorStatesGood()){
                plantList[index].wateringStatus = STATUS_IN_QUEUE;
                addWateringJob(plantList[index], changePlant.plantToChange.waterAmount, changePlant.plantToChange.fertilizerAmount);
            }
        }
        break;

    case CHANGE_NAME:
        if(index!=UNREGISTEREDADDRESS){
            strcpy(plantList[index].name, changePlant.plantToChange.name);
            savePlantToStorage(plantList[index]);
        }
        break;

    case CHANGE_SETTINGS:
        if(index!=UNREGISTEREDADDRESS){
            plantList[index].autoWatering = changePlant.plantToChange.autoWatering;
            plantList[index].waterAmount = changePlant.plantToChange.waterAmount;
            plantList[index].fertilizerAmount = changePlant.plantToChange.fertilizerAmount;
            plantList[index].threshold= changePlant.plantToChange.threshold;
            plantList[index].type = changePlant.plantToChange.type;
            plantList[index].waitTime = changePlant.plantToChange.waitTime;
            savePlantToStorage(plantList[index]);
        }
        break;

    case CHANGE_WATERINGSTATUS:
        if(index!=UNREGISTEREDADDRESS){
            //ESP_LOGI(TAG, "Old Soil Moisture: %i, New Soil Moiture: %i", plantList[index].soilMoisture, changePlant.plantToChange.soilMoisture);
            plantList[index].wateringStatus = changePlant.plantToChange.wateringStatus;
        }
        break;
        
        case CHANGE_SETSAFETYTIME:
        if(index!=UNREGISTEREDADDRESS && plantList[index].waitTime > 0){
            //ESP_LOGI(TAG, "Old Soil Moisture: %i, New Soil Moiture: %i", plantList[index].soilMoisture, changePlant.plantToChange.soilMoisture);
            plantList[index].safetyTimeActive = 1;
            plantList[index].safetyMinutesLeft = plantList[index].waitTime;
            savePlantToStorage(plantList[index]);

        }
        break;

        case UPDATE_SAFETYMINUTES: decreaseSafetyMinutesByOne(); triggerUpdate = false;
        break;
    }

    if(triggerUpdate && index != UNREGISTEREDADDRESS){
        if(changePlant.parameterType != CHANGE_REMOVE){
            plantChangedNotification(plantList[index], false);
        }else
        {
            changePlant.plantToChange.valveStatus = DELETED;
            plantChangedNotification(changePlant.plantToChange, false);
        }
        
        
    }
        
    //ESP_LOGI(TAG, "Changed Plant State");
    
}

void changeErrorStateInternally(errorChange errChange){
    switch (errChange.errType){
        case ERRCHANGE_OVERPRESSURE: errStates.waterPressureHigh = errChange.newState;
            break;
        case ERRCHANGE_NOTENOUGHWATERFLOW: errStates.notEnoughWaterFlow = errChange.newState;
            break;
        case ERRCHANGE_PROBLEMRESOLVEDMANUALLY: errStates.notEnoughWaterFlow = false; errStates.waterPressureHigh = false;
            break;
    }
    errorStateChangedNotification(errStates);
    ESP_LOGI(TAG, "Changed Error State");
}
