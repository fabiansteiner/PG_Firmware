#include <driver/adc.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"


#include <stdio.h>
#include "PLC.h"


#define RELAY 25
#define PUMP 18
#define EXTRAVALVE 19
#define STEP 14
#define DIR 12
#define PRESSURESENSOR 34
#define FLOWSENSOR 35

static const char *TAG = "WATERING_TASK";
TaskHandle_t wateringHandle = NULL;

#define WATERING_TASK_STACK_SIZE    4096
#define WATERING_TASK_PRIORITY    15

wateringJob wJob;

static void turn_on_pump(float duty_cycle);
static void turn_off_pump();

uint32_t flowSensorTick = 0;
uint32_t setPoint = 0;
bool testing = false;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    flowSensorTick ++;
}


void wateringTask(void * pvParameters){
    bool overPressure = false;
    bool waterFlowTooSlow = false;

    uint32_t wateringTimeCounter = 0;
    uint32_t lastTickAmount = 0;
    uint32_t ticksSinceLastUpdate = 0;
    float flowSpeed = 0.0f;

    int adc_val;
    float pressure;

    //Set Relay to on 
    gpio_set_level(RELAY, 1);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    gpio_isr_handler_add(FLOWSENSOR, gpio_isr_handler, (void*) 0);
    //Activate PWM on Pump
    turn_on_pump(75.0);
    

    while(flowSensorTick < setPoint){
        vTaskDelay(500/portTICK_PERIOD_MS);
        adc_val = adc1_get_raw(ADC1_CHANNEL_6);
        pressure = ((3.3f/4096*adc_val)-0.5)*4;

        ticksSinceLastUpdate = flowSensorTick - lastTickAmount;
        flowSpeed = 0.181818f * ticksSinceLastUpdate;
        ESP_LOGI(TAG,"Wateringprogress: %f mililitres, Flowspeed: %f l/min, Pressure: %f bar", 1.5f * flowSensorTick, flowSpeed, pressure);

        lastTickAmount = flowSensorTick;
        wateringTimeCounter++;

        if(pressure >= 6.0f){
            overPressure = true;
            break;
        }

        if(wateringTimeCounter > 10){//if the first 5 Seconds passed
            if(flowSpeed < 0.5f){
                waterFlowTooSlow = true;
                break;
            }

        }    
    }

    turn_off_pump();
    gpio_set_level(RELAY, 0);
    gpio_isr_handler_remove(FLOWSENSOR);

    if(overPressure == true){
        ESP_LOGI(TAG,"Watering aborted, Pressure too High");
        if(!testing)
            changeErrorState(ERRCHANGE_OVERPRESSURE, true);
    }else if (waterFlowTooSlow == true){
        ESP_LOGI(TAG,"Watering aborted, Waterflow is too slow");
        if(!testing)
            changeErrorState(ERRCHANGE_NOTENOUGHWATERFLOW, true);
    }

    
    


    wJob.plantToWater.wateringStatus = STATUS_NOTHINSCHEDULED;
    if(!testing){
        changePlant(wJob.plantToWater, CHANGE_SETSAFETYTIME);
        changePlant(wJob.plantToWater, CHANGE_WATERINGSTATUS);
    }
        

    vTaskDelete(NULL);

}



void startWateringTask(wateringJob job123){
    flowSensorTick = 0;
    wJob = job123;
    //1ml = 0.66 Ticks
    setPoint = 0.66f * job123.waterAmount;
    ESP_LOGI(TAG,"Setpoint set to %u, which are %u milliliters", setPoint, job123.waterAmount);
    xTaskCreate(wateringTask, "wateringTask", WATERING_TASK_STACK_SIZE, NULL, WATERING_TASK_PRIORITY, wateringHandle);
}


void initializeWateringComponents(bool test){
    testing = test;
    //TODO: Set GPIO inputs and outputs which are needed for watering
     //gpio_reset_pin(RELAY);
     //gpio_reset_pin(PUMP);
     //gpio_reset_pin(EXTRAVALVE);
     //gpio_reset_pin(STEP);
     //gpio_reset_pin(DIR);
     //gpio_reset_pin(PRESSURESENSOR);
     //gpio_reset_pin(FLOWSENSOR);
    /* Set the GPIOs direction */
    gpio_set_direction(RELAY, GPIO_MODE_OUTPUT);

    //Init Motor Control PWM on Pump pin
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PUMP);
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 22;    //frequency = 22Hz (2l/min),
    pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings

    gpio_set_direction(EXTRAVALVE, GPIO_MODE_OUTPUT);
    gpio_set_direction(STEP, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR, GPIO_MODE_OUTPUT);
    gpio_set_direction(PRESSURESENSOR, GPIO_MODE_INPUT);

    gpio_set_direction(FLOWSENSOR, GPIO_MODE_INPUT);
    gpio_set_intr_type(FLOWSENSOR, GPIO_INTR_POSEDGE);//install gpio isr service
    gpio_install_isr_service(0);    //Return a normal interrupt source of level 1 - 3

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6,ADC_ATTEN_DB_11);


}

/**
 * @brief motor moves in forward direction, with duty cycle = duty %
 */
static void turn_on_pump(float duty_cycle)
{
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, duty_cycle);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); //call this each time, if operator was previously in low/high state
}

/**
 * @brief motor stop
 */
static void turn_off_pump()
{
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
}

/**
 * 
 * Start watering (Relay schalten, Pumpe einschalten)
 * Interrupts aktivieren, dieser soll eine Nummer nach oben zählen, welche für den Wasserdurchfluss steht
 * main loop checkt die Nummer und sendet change Requests an den variable Pool
 * 
 *
 * 
 */