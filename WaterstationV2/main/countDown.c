#include <stdio.h>
#include "countDown.h"
#include "variablepool.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "esp_log.h"

TaskHandle_t countDownHandle;
static const char *TAG = "COUNTDOWN_TASK";

#define COUNT_DOWN_TASK_STACK_SIZE    2048
#define COUNT_DOWN_TASK_PRIORITY    20

void countDownTask(void * pvParameters){
    plant dummyPlant = getNewPlant();
    while(1){
        //Wait exactly one Minute
        vTaskDelay(60000/ portTICK_PERIOD_MS);
        changePlant(dummyPlant, UPDATE_SAFETYMINUTES);
        ESP_LOGI(TAG, "Update of safety minutes triggered");
    }
}


void initializeCountDown(){
    xTaskCreate(countDownTask, "countDownTask", COUNT_DOWN_TASK_STACK_SIZE, NULL, COUNT_DOWN_TASK_PRIORITY, countDownHandle);
}