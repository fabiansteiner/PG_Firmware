/* UART Echo Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "string.h"

#include "PLC.h"
#include "watering.h"
#include "FAT_storage.h"
#include "web_server.h"
#include "UserIO.h"

void app_main(void)
{


    //Set Log levels
    esp_log_level_set("calculateWaterContent", ESP_LOG_WARN);
    esp_log_level_set("PLC_TASK", ESP_LOG_WARN);
    //esp_log_level_set("WebSocketServer", ESP_LOG_WARN);
    //esp_log_level_set("FAT_Storage", ESP_LOG_WARN);

    //Normal Operation:
    initWebSocketServer();
    initializeVariablePool();
    initializePLCTask();
    initializeUserIO(); //Has to be for whatever reason on the last position, if not --> neopixels does not work
    
    
    
    
    

    //Test Code for Watering
    /*
    initializeWateringComponents(true);
    plant p = getNewPlant();
    wateringJob wj;
    wj.plantToWater = p;
    wj.waterAmount = 2000;
    wj.fertilizerAmount = 10;
    startWateringTask(wj);
    */

    //Test Code for fat partition
    /*
    initFATStorage();

    plant p = getNewPlant();
    p.address = 123;
    strcpy(p.name, "HelloMyFreshBubu");
    savePlantToStorage(p);
    getPlantFromStorage(123, "", true);

    strcpy(p.name, "lul");
    savePlantToStorage(p);
    getPlantFromStorage(123, "", true);

    removePlantFromStorage(p);
    */
    /*
    while(1){
        vTaskDelay(100/portTICK_PERIOD_MS);
        plant p = getNewPlant();
        plantChangedNotification(p);
    }
    */
    

    vTaskDelete(NULL);
}