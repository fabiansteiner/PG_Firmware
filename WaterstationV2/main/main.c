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

#include "PLC.h"
#include "variablepool.h"

void app_main(void)
{


    //Set Log levels
    esp_log_level_set("calculateWaterContent", ESP_LOG_WARN);


    initializeVariablePool();
    //initializePLCTask();
    vTaskDelete(NULL);
}