#include "variablepool.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "esp_log.h"

static const char *TAG = "VARIABLEPOOL_TASK";


#define VARIABLE_POOL_TASK_STACK_SIZE    4096
#define VARIABLE_POOL_TASK_PRIORITY    9

TaskHandle_t variablePoolHandle = NULL;
QueueHandle_t plantChangeQueue;


plant plantList[PLANTSIZE] = {0};
int plantListCounter = 0;

void changePlantInternally(plantChange changePlant);

void variablePoolTask(void * pvParameters){
    //InitializingShizzle

    plantChange pc;

    while(1){
        if(plantChangeQueue != 0){
            if(xQueueReceive(plantChangeQueue, &pc, (TickType_t) 20) == pdTRUE){
                changePlantInternally(pc);
            }
        }else{
            vTaskDelay(10/ portTICK_PERIOD_MS);
        }
    }
}

/**
 * Initializes the main Variables, reads Data from "EEPROM", if it exists
 * Start RTOS Task
 */
void initializeVariablePool(){
    //Read data from EEPROM
    //Dummy Data 
    plant dummyPlant1 = {0b01010110, "Plant1", 1000, 5, 70.0f, 50, 1, 0, {0}};
    //plant dummyPlant2 = {0b11001100, "Plant2", 1000, 5, 70.0f, 50, 1, 0};

    plantList[0] = dummyPlant1;
    //plantList[1] = dummyPlant2;
    
    plantChangeQueue = xQueueCreate(32, sizeof(plantChange));
    xTaskCreate(variablePoolTask, "variable_pool_task", VARIABLE_POOL_TASK_STACK_SIZE, NULL, VARIABLE_POOL_TASK_PRIORITY, variablePoolHandle);

    /*

    plantChange pc1 = {dummyPlant1, CHANGE_ADD};
    plantChange pc2 = {dummyPlant2, CHANGE_ADD};

    changePlant(pc1);
    changePlant(pc2);
    */

   

}

/**
 * Adds change request for plants to the RTOS Queue
 * Adding and Removing is also happening here
 * This should be used as the ONLY interface to make changes to the plantList
 * Due to the Queue, this will prevent concurrent writing of changes to the List
 */
void changePlant(plant plantToChange, uint8_t parameterType){
    plantChange pc = {plantToChange, parameterType};
    xQueueSend(plantChangeQueue, &pc, (TickType_t) 20);
    ESP_LOGI(TAG, "Sent Plant Change Request to Queue\n");
}

/**
 * Get the whole pointer to the variable Pool
 */
plant * getVariablePool(){
    return plantList;
}


int getPlantIndex(uint8_t plantAddress){
    int index = 255;
    for(int i = 0; i<PLANTSIZE; i++){
        if(plantList[i].address == plantAddress){
            index = i;
            break;
        }
    }
    return index;
}

void changePlantInternally(plantChange changePlant){
    ESP_LOGI(TAG, "Entered changePlantInternally...\n");
    int i = 0;
    int index = 0;
    switch (changePlant.parameterType)
    {
    case CHANGE_ADD:
       
        while(plantList[i].address != 0){
            i++;
        }
        plantList[i] = changePlant.plantToChange;
        break;
    case CHANGE_PLCVALVEVALUES:
        index = getPlantIndex(changePlant.plantToChange.address);
        if(index!=255){
            ESP_LOGI(TAG, "Old Soil Moisture: %i, New Soil Moiture: %i\n", plantList[index].soilMoisture, changePlant.plantToChange.soilMoisture);
            plantList[index].progress = changePlant.plantToChange.progress;
            plantList[index].soilMoisture = changePlant.plantToChange.soilMoisture;
            plantList[index].status = changePlant.plantToChange.status;
        }
        
        break;
    case CHANGE_SETTINGS:
        index = getPlantIndex(changePlant.plantToChange.address);
        if(index!=255){
            plantList[index].autoWatering = changePlant.plantToChange.autoWatering;
            plantList[index].waterAmount = changePlant.plantToChange.waterAmount;
            plantList[index].fertilizerAmount = changePlant.plantToChange.fertilizerAmount;
            strcpy(plantList[index].name, changePlant.plantToChange.name);
            plantList[index].threshold= changePlant.plantToChange.threshold;
        }
        break;
    
    default:
        break;
    }
}
