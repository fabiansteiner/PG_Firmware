#include "variablepool.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


#define VARIABLE_POOL_TASK_STACK_SIZE    2048
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
    plant dummyPlant1 = {0b01010110, "Plant1", 1000, 5, 70.0f, 50, 1, 0};
    plant dummyPlant2 = {0b11001100, "Plant2", 1000, 5, 70.0f, 50, 1, 0};

    plantList[0] = dummyPlant1;
    plantList[1] = dummyPlant2;
    
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
void changePlant(plantChange changePlant){
    xQueueSend(plantChangeQueue, &changePlant, (TickType_t) 20);
}

/**
 * Get the whole pointer to the variable Pool
 */
plant * getVariablePool(){
    return plantList;
}

void changePlantInternally(plantChange changePlant){
    if(changePlant.parameterType == CHANGE_ADD){
        int i = 0;
        while(plantList[i].address != 0){
            i++;
        }
        plantList[i] = changePlant.plantToChange;
    }
}
