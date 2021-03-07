#include <stdio.h>
#include "variablepool.h"
#include "UserIO.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "esp_log.h"
#include "driver/gpio.h"

TaskHandle_t userioHandle;
static const char *TAG = "USER_IO_TASK";

#define USER_IO_TASK_STACK_SIZE    2048
#define USER_IO_TASK_PRIORITY    20

#define SW_1 26

bool buttonPressed = false;

static void IRAM_ATTR button_isr_handler(void* arg)
{
    buttonPressed = true;
}

void userioTask(void * pvParameters){
    
    while(1){
        if(buttonPressed == true){
            changeErrorState(ERRCHANGE_PROBLEMRESOLVEDMANUALLY, false);
            vTaskDelay(500/portTICK_PERIOD_MS);
            buttonPressed = false;
            ESP_LOGI(TAG,"Button pressed, error state changed to normal.");
        }
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}


void initializeUserIO(){
    //Setup Interrupts
    gpio_set_direction(SW_1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SW_1, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(SW_1, GPIO_INTR_NEGEDGE);//install gpio isr service
    //gpio_install_isr_service(0);    //Return a normal interrupt source of level 1 - 3

    gpio_isr_handler_add(SW_1, button_isr_handler, (void*) 0);

    
    xTaskCreate(userioTask, "userioTask", USER_IO_TASK_STACK_SIZE, NULL, USER_IO_TASK_PRIORITY, userioHandle);

    
}