#include <stdlib.h>
#include "string.h"
#include "esp_log.h"

#include "jsonParser.h"
#include "cJSON.h"

#define ACTION_CHANGESETTINGS 1
#define ACTION_DELETEPLANT 2
#define ACTION_WATERPLANT 3
#define ACTION_CHANGEWIFICREDENTIALS 4
#define ACTION_CHANGNAME 5

static const char *TAG = "JSONparser";

char sendBuffer[512];



uint8_t buildJsonString(plant p, errorStates err, bool isPlant){
    memset(sendBuffer, 0, 512);
    if(isPlant){

        //static int counter = 0;
        char *out = NULL;
        char *buf = NULL;
        size_t len = 0;

        cJSON *plantJson,*progressJson;
	    plantJson=cJSON_CreateObject();
        cJSON_AddNumberToObject(plantJson,"address",		p.address);
	    cJSON_AddItemToObject(plantJson, "name", cJSON_CreateString(p.name));
        cJSON_AddNumberToObject(plantJson,"waterAmount",		p.waterAmount);
        cJSON_AddNumberToObject(plantJson,"fertilizerAmount",		p.fertilizerAmount);
        cJSON_AddNumberToObject(plantJson,"soilMoisture",		p.soilMoisture);
        cJSON_AddNumberToObject(plantJson,"threshold",		p.threshold);
        cJSON_AddNumberToObject(plantJson,"wateringStatus",		p.wateringStatus);
        cJSON_AddNumberToObject(plantJson,"valveStatus",		p.valveStatus);
        cJSON_AddNumberToObject(plantJson,"autoWatering",		p.autoWatering);
        
	    cJSON_AddItemToObject(plantJson, "progress", progressJson=cJSON_CreateObject());
        cJSON_AddNumberToObject(progressJson,"water",		            p.progress.water);
        cJSON_AddNumberToObject(progressJson,"waterProgress",		    p.progress.waterProgress);
        cJSON_AddNumberToObject(progressJson,"fertilizerPerLiter",		p.progress.fertilizerPerLiter);
        cJSON_AddNumberToObject(progressJson,"fertilizerProgress",		p.progress.fertilizerProgress);
        
        cJSON_AddNumberToObject(plantJson,"unsuccessfulRequests",		p.unsuccessfulRequests);
        cJSON_AddNumberToObject(plantJson,"safetyTimeActive",		p.safetyTimeActive);
        cJSON_AddNumberToObject(plantJson,"safetyMinutesLeft",		p.safetyMinutesLeft);
        cJSON_AddNumberToObject(plantJson,"type",		p.type);
        cJSON_AddNumberToObject(plantJson,"waitTime",		p.waitTime);

        //formatted print */

        //counter++;
        out = cJSON_Print(plantJson);
        //ESP_LOGI(TAG,"out of json parsing: %s\n Lenght: %i", out, strlen(out));
        //    free(out);
            //strcpy(sendBuffer, out);
        
        
        //create buffer to succeed */
        // the extra 5 bytes are because of inaccuracies when reserving memory */
        //
        
        len = strlen(out) + 5;
        buf = (char*)malloc(len);
        if (buf == NULL)
        {
            ESP_LOGI(TAG,"Failed to allocate memory.\n");
            cJSON_Delete(plantJson);
            free(out);
            free(buf);
            return 1;
        }
        //Print to buffer 
        if (!cJSON_PrintPreallocated(plantJson, buf, (int)len, 1)) {
            ESP_LOGI(TAG,"cJSON_PrintPreallocated failed!");
            if (strcmp(out, buf) != 0) {
                ESP_LOGI(TAG,"cJSON_PrintPreallocated not the same as cJSON_Print!");
                ESP_LOGI(TAG,"cJSON_Print result:\n%s", out);
                ESP_LOGI(TAG,"cJSON_PrintPreallocated result:\n%s", buf);
            }
            cJSON_Delete(plantJson);
            free(out);
            free(buf);
            return 1;
        }

        // success 
        strcpy(sendBuffer, buf);
        cJSON_Delete(plantJson);
        free(out);
        free(buf);
        
        
    }else{
        char *out = NULL;
        char *buf = NULL;
        size_t len = 0;

        cJSON *errState;
	    errState=cJSON_CreateObject();
        if(err.waterPressureHigh){
            cJSON_AddTrueToObject(errState, "waterPressureHigh");
        }else{
            cJSON_AddFalseToObject(errState, "waterPressureHigh");
        }
        if(err.notEnoughWaterFlow){
            cJSON_AddTrueToObject(errState, "notEnoughWaterFlow");
        }else{
            cJSON_AddFalseToObject(errState, "notEnoughWaterFlow");
        }
        if(err.oneOrMoreValvesNotClosed){
            cJSON_AddTrueToObject(errState, "oneOrMoreValvesNotClosed");
        }else{
            cJSON_AddFalseToObject(errState, "oneOrMoreValvesNotClosed");
        }
        if(err.oneOrMoreValveErrors){
            cJSON_AddTrueToObject(errState, "oneOrMoreValveErrors");
        }else{
            cJSON_AddFalseToObject(errState, "oneOrMoreValveErrors");
        }
        if(err.oneOrMoreValvesOffline){
            cJSON_AddTrueToObject(errState, "oneOrMoreValvesOffline");
        }else{
            cJSON_AddFalseToObject(errState, "oneOrMoreValvesOffline");
        }

         /* formatted print */
        out = cJSON_Print(errState);

        /* create buffer to succeed */
        /* the extra 5 bytes are because of inaccuracies when reserving memory */
        len = strlen(out) + 5;
        buf = (char*)malloc(len);
        if (buf == NULL)
        {
            ESP_LOGI(TAG,"Failed to allocate memory.");
            cJSON_Delete(errState);
            free(out);
            free(buf);
            return 1;
        }
         /* Print to buffer */
        if (!cJSON_PrintPreallocated(errState, buf, (int)len, 1)) {
            ESP_LOGI(TAG,"cJSON_PrintPreallocated failed!");
            if (strcmp(out, buf) != 0) {
                ESP_LOGI(TAG,"cJSON_PrintPreallocated not the same as cJSON_Print!");
                ESP_LOGI(TAG,"cJSON_Print result:\n%s", out);
                ESP_LOGI(TAG,"cJSON_PrintPreallocated result:\n%s", buf);
            }
            cJSON_Delete(errState);
            free(out);
            free(buf);
            return 1;
        }

        /* success */
        strcpy(sendBuffer, buf);

        cJSON_Delete(errState);
        free(out);
        free(buf);
    }

    return 0;
    
}

uint8_t parseIncomingString(char *incomingString){
    plant p = {0};
    p.address = UNREGISTEREDADDRESS;
    bool validJsonArrived = true;

    
    const cJSON *requestItems = NULL;
    const cJSON *address = NULL;
    const cJSON *name = NULL;
    const cJSON *waterAmount = NULL;
    const cJSON *fertilizerAmount = NULL;
    const cJSON *threshold = NULL;
    const cJSON *autoWatering = NULL;
    const cJSON *type = NULL;
    const cJSON *waitTime = NULL;
    const cJSON *action = NULL;


    //Parse Json and check if incoming string is JSON
    cJSON *json = cJSON_Parse(incomingString);
    if (json == NULL)
    {
        ESP_LOGI(TAG, "No JSON Data arrived");
        cJSON_Delete(json);
        return 1;
    }

    //See if Address and Action is included in JSON
    requestItems = cJSON_GetObjectItem(json , "requestItems");
    if(cJSON_IsTrue(requestItems)){
        return 2;
    }


    //See if Address and Action is included in JSON
    address = cJSON_GetObjectItem(json , "address");
    if (!cJSON_IsNumber(address)){
         validJsonArrived = false;
    }else{
        if(address->valueint < 0 && address->valueint>=100){
            validJsonArrived = false;
        }
    }
    action = cJSON_GetObjectItem(json , "action");
    if (!cJSON_IsNumber(action)){
         validJsonArrived = false;
    }

    if(validJsonArrived == false){
        ESP_LOGI(TAG, "Json with invalid parameters arrived");
        return 1;
    }

    //Check which action and prepare plants
    if(action->valueint == ACTION_CHANGESETTINGS){
        ESP_LOGI(TAG, "Trying to act out changesettings");
        waterAmount = cJSON_GetObjectItem(json , "waterAmount");
        if (!cJSON_IsNumber(waterAmount)){
            validJsonArrived = false;
        }
        fertilizerAmount = cJSON_GetObjectItem(json , "fertilizerAmount");
        if (!cJSON_IsNumber(fertilizerAmount)){
            validJsonArrived = false;
        }
        threshold = cJSON_GetObjectItem(json , "threshold");
        if (!cJSON_IsNumber(threshold)){
            validJsonArrived = false;
        }
        autoWatering = cJSON_GetObjectItem(json , "autoWatering");
        if (!cJSON_IsNumber(autoWatering)){
            validJsonArrived = false;
        }
        type = cJSON_GetObjectItem(json , "type");
        if (!cJSON_IsNumber(type)){
            validJsonArrived = false;
        }
        waitTime = cJSON_GetObjectItem(json , "waitTime");
        if (!cJSON_IsNumber(waitTime)){
            validJsonArrived = false;
        }

        if(validJsonArrived == false){
            ESP_LOGI(TAG, "Json with invalid parameters arrived");
            return 1;
        }

        p.address = address->valueint;
        p.waterAmount = waterAmount->valueint;
        p.fertilizerAmount = fertilizerAmount->valueint;
        p.threshold = threshold->valueint;
        p.autoWatering = autoWatering->valueint;
        p.waitTime = waitTime->valueint;
        p.type = type->valueint;

    }else if (action->valueint == ACTION_CHANGNAME){
        name = cJSON_GetObjectItem(json , "name");
        if (!cJSON_IsString(name)){
            validJsonArrived = false;
        }

         if(validJsonArrived == false){
            ESP_LOGI(TAG, "Json with invalid parameters arrived");
            return 1;
        }
        p.address = address->valueint;
        strcpy(p.name, name->valuestring);
    }else if (action->valueint == ACTION_DELETEPLANT){
        p.address = address->valueint;
    }else if (action->valueint == ACTION_WATERPLANT){
        waterAmount = cJSON_GetObjectItem(json , "waterAmount");
        if (!cJSON_IsNumber(waterAmount)){
            validJsonArrived = false;
        }
        fertilizerAmount = cJSON_GetObjectItem(json , "fertilizerAmount");
        if (!cJSON_IsNumber(fertilizerAmount)){
            validJsonArrived = false;
        }

        if(validJsonArrived == false){
            ESP_LOGI(TAG, "Json with invalid parameters arrived");
            return 1;
        }

        p.address = address->valueint;
        p.waterAmount = waterAmount->valueint;
        p.fertilizerAmount = fertilizerAmount->valueint;
    }else{
        ESP_LOGI(TAG, "Invalid Action");
        return 1;
    }

    //Act out changes
    switch (action->valueint)
    {
    case ACTION_CHANGESETTINGS: changePlant(p, CHANGE_SETTINGS);
        break;
    case ACTION_CHANGNAME: changePlant(p, CHANGE_NAME);
        break;
    case ACTION_DELETEPLANT: changePlant(p,CHANGE_REMOVE);
        break;
    case ACTION_WATERPLANT: changePlant(p, CHANGE_QUEUEFORWATERING);
        break;
    default:
        break;
    }
    

    cJSON_Delete(json);
    return 0;
}

char * getSendBuffer(){
    return sendBuffer;
}
