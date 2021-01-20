
#include <stdio.h>

#define PLANTSIZE 100

#define CHANGE_NAME 1
#define CHANGE_WATERAMOUNT 2
#define CHANGE_FERTILIZERAMOUNT 3
#define CHANGE_SOILMOISTURE 4
#define CHANGE_TRESHOLD 5
#define CHANGE_STATUS 6
#define CHANGE_AUTOWATERING 7
#define CHANGE_REMOVE 8
#define CHANGE_ADD 9
#define CHANGE_PLCVALVEVALUES 10
#define CHANGE_SETTINGS 11

#define STATUS_MOTORERROR 1
#define STATUS_OK 2
#define STATUS_IN_QUEUE 3
#define STATUS_WATERING 4

typedef struct wateringProgress{
    int water;
    int waterProgress;
    int fertilizerPerLiter;
    int fertilizerProgress;
}wateringProgress;

typedef struct plantData{
    uint8_t address;
	char name[50];
	int waterAmount;
    uint8_t fertilizerAmount;
    int soilMoisture;
    uint8_t threshold;
    uint8_t status;
    uint8_t autoWatering;
    wateringProgress progress;
}plant;

typedef struct plantDataChange{
    plant plantToChange;
    uint8_t parameterType;
}plantChange;


/**
 * Initializes the main Variables, reads Data from "EEPROM", if it exists
 * Start RTOS Task
 */
void initializeVariablePool();

/**
 * Adds change request for plants to the RTOS Queue
 * Adding and Removing is also happening here
 * This should be used as the ONLY interface to make changes to the plantList
 * Due to the Queue, this will prevent concurrent writing of changes to the List
 */
void changePlant(plant plantToChange, uint8_t parameterType);


/**
 * Get pointer to the Plant List
 */
plant * getVariablePool();



