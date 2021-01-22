#include <stdio.h>
#include "calculateVolumetricContent.h"
#include "PLC.h"
#include "esp_log.h"
#include "math.h"

static const char* TAG = "calculateWaterContent";
int linearInterpolation (int turnedAdcValue, float k, float d, int x1);

uint8_t getWaterContent(uint8_t highByte, uint8_t lowByte){
    int adc_value = 1023-(lowByte + (highByte << 8));
    ESP_LOGI(TAG, "HighByte: %u , LowByte: %u\n\r", highByte, lowByte);
    ESP_LOGI(TAG, "Turned ADC value: %i\n", adc_value);

    if(adc_value < ADC_P1 || adc_value == 1023){
        return 0;
    }else if (adc_value >= ADC_P1 && adc_value < ADC_P2){
        return linearInterpolation(adc_value, INTER_K_P1_P2, INTER_D_P1_P2, INTER_X1_P1_P2);
    }else if (adc_value >= ADC_P2 && adc_value < ADC_P3){
        return linearInterpolation(adc_value, INTER_K_P2_P3, INTER_D_P2_P3, INTER_X1_P2_P3);
    }else if (adc_value >= ADC_P3 && adc_value <= ADC_P4){
        return linearInterpolation(adc_value, INTER_K_P3_P4, INTER_D_P3_P4, INTER_X1_P3_P4);
    }else if (adc_value > ADC_P4){
        return 50;
    }
    return 0;
}


int linearInterpolation (int turnedAdcValue, float k, float d, int x1){

    // f(x) = k(x-x1) + d 
    int x = turnedAdcValue - x1;
    float result = (k*x)+d;
    float roundedResult = roundf(result);

    ESP_LOGI(TAG, "x = %i , result: %f , roundedResult: %f\n", x ,result, roundedResult);

    return roundedResult;
}