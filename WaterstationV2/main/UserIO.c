#include <stdio.h>
#include "variablepool.h"
#include "UserIO.h"
#include "ws2812_control.h"

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
#define SW_1 27

#define LED_ANIMATION_ERROR 1
#define LED_ANIMATION_WATERING 2
#define LED_ANIMATION_CONNECTED 3

void animateLED();
void errorAnimation();
void connectedStateAnimation();
void wateringAnimation();
void changeLedColor(uint32_t color);



bool buttonPressed = false;
errorStates * errStatesReference;
bool watering = false;
bool connected = false;

uint8_t currentAnimation;
uint8_t animationRefreshRate = 10;           // 10 = 100ms, 100 = 1000ms, etc...
uint8_t animationRefreshCounter = 0;

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
        vTaskDelay(10/portTICK_PERIOD_MS);

        
        animationRefreshCounter++;
        if(animationRefreshCounter >= animationRefreshRate){
            animateLED();
            animationRefreshCounter = 0;
        }

        
    }
}

void changeUserIOState(uint8_t subject, bool state){
    switch (subject)
    {
    case SUBJECT_CONNECTED: connected = state;
        break;
    case SUBJECT_WATERING: watering = state;
        break;
    
    default:
        break;
    }
}

void animateLED(){
    //Check states and pick animation, for every animation a new method

    //Check if any error state is != false, if so, pick errorAnimation
    if(errStatesReference->notEnoughWaterFlow == true || errStatesReference->waterPressureHigh == true || 
    errStatesReference->oneOrMoreValveErrors == true ||
    errStatesReference->oneOrMoreValvesNotClosed == true  ||
    errStatesReference->oneOrMoreValvesOffline == true ){
        errorAnimation();
    } else if (watering == true){
        wateringAnimation();
    }else{
        connectedStateAnimation();
    }

    //Check if watering is true, if so, pick watering Animation

    //else pick connectedAnimation
    
}

void errorAnimation(){
    //ESP_LOGI(TAG,"Making Error Animation");
    static uint8_t animationCounter = 0;
    static bool on = false;
    bool animationLockedIn = false;
    if(currentAnimation != LED_ANIMATION_ERROR){
        currentAnimation = LED_ANIMATION_ERROR;
        animationRefreshRate = 200; //=2s
        //Reset helping Variables for this animation
        animationCounter = 0;
        on = false;
    }
    if(on == false){
        //Search for next parameter that is true
        uint8_t goingThroughCounter = 0;
        while(animationLockedIn == false){
            switch (animationCounter)
            {
            case 0: if(errStatesReference->waterPressureHigh == true){animationLockedIn = true;}else{animationCounter++;}
                break;
            case 1: if(errStatesReference->notEnoughWaterFlow == true){animationLockedIn = true;}else{animationCounter++;}
                break;
            case 2: if(errStatesReference->oneOrMoreValvesNotClosed == true){animationLockedIn = true;}else{animationCounter++;}
                break;
            case 3: if(errStatesReference->oneOrMoreValveErrors == true){animationLockedIn = true;}else{animationCounter++;}
                break;
            case 4: if(errStatesReference->oneOrMoreValvesOffline == true){animationLockedIn = true;}else{animationCounter++;}
                break;
            case 5: animationCounter = 0;
                break;
            }
            goingThroughCounter++;
            if(goingThroughCounter > 5) break;
        }

        if(animationLockedIn){
            //Turn on leds in specific colors, indicating the ERROR
            if(animationCounter == 0 || animationCounter == 1){ //If waterPressureHigh || notEnoughWaterFlow
                changeLedColor(RED);
            }else if(animationCounter == 2){    //If valves not closed
                changeLedColor(YELLOW);
            }else if(animationCounter == 3 || animationCounter == 4){     //if valve error or valves offline
                changeLedColor(ORANGE);
            }

            animationCounter ++;
            on = true;
        }
    }else{
        changeLedColor(OFF);
        on = false;
    }
}

void connectedStateAnimation(){
    static bool goinUp = true;
    static uint32_t displayColor = 0;
    if(currentAnimation != LED_ANIMATION_CONNECTED){
        currentAnimation = LED_ANIMATION_CONNECTED;
        goinUp = true;
        displayColor = 0;
    }
    if(connected == true){
        animationRefreshRate = 200;      //Every 2 Seconds
        changeLedColor(VERYDIMMGREEN);
    }else{
        animationRefreshRate = 4;      //Every 20 MiliSeconds
        
        if(goinUp){
            displayColor = displayColor + (1<<8) + (1<<16);
            if((displayColor>>8) >= 25000) goinUp = false;
        }else{
            displayColor = displayColor - ((1<<8) + (1<<16));
            if((displayColor>>8) <= 1000) goinUp = true;
        }

        changeLedColor(displayColor);
        
       
    }
    
}

void wateringAnimation(){
    static uint8_t ledCounter = 0;
    if(currentAnimation != LED_ANIMATION_WATERING){
        currentAnimation = LED_ANIMATION_WATERING;
        animationRefreshRate = 10;
        ledCounter = 0;
    }

    //Do shit

    //Set all LEDs to nothing
    struct led_state new_state;
    for(uint8_t i = 0; i < NUM_LEDS; i++){
        new_state.leds[i] = OFF;
    }

    uint32_t blue = BLUE;

    for(int i = 0; i <= 4; i++){
        int targetLed = ledCounter - i;

        if(targetLed < 0) targetLed = targetLed + NUM_LEDS;
        
        new_state.leds[targetLed] = blue;
        blue = blue - 50;
    }

    ws2812_write_leds(new_state);


    ledCounter++;

    if(ledCounter >= NUM_LEDS){
        ledCounter = 0;
    }

}


void changeLedColor(uint32_t color){
    //ESP_LOGI(TAG,"Changing LED Color to %i", color);
    struct led_state new_state;
    for(uint8_t i = 0; i < NUM_LEDS; i++){
        new_state.leds[i] = color;
    }
    ws2812_write_leds(new_state);
}

void switchButtonDetection(bool on){
    if(on){
        gpio_isr_handler_add(SW_1, button_isr_handler, (void*) 0);
    }else{
        gpio_isr_handler_remove(SW_1);
    }
}



void initializeUserIO(){
    //Setup Interrupts
    gpio_set_direction(SW_1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SW_1, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(SW_1, GPIO_INTR_NEGEDGE);//install gpio isr service
    //gpio_install_isr_service(0);    //Return a normal interrupt source of level 1 - 3
    gpio_isr_handler_add(SW_1, button_isr_handler, (void*) 0);

    errStatesReference = getErrorStatesPointer();


    //Initialize Neopixel LEDS
    ws2812_control_init();


    xTaskCreate(userioTask, "userioTask", USER_IO_TASK_STACK_SIZE, NULL, USER_IO_TASK_PRIORITY, userioHandle);

    
}