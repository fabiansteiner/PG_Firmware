#include <stdio.h>
#include <stdbool.h>

#ifndef VARIABLEPOOL_H
#define VARIABLEPOOL_H


#define PLANTSIZE 100
#define UNREGISTEREDADDRESS 255
#define NONEXISTINGADDRESS 254

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
#define CHANGE_WATERINGSTATUS 12
#define CHANGE_SETSAFETYTIME 13
#define UPDATE_SAFETYMINUTES 14

#define ERRCHANGE_OVERPRESSURE 1
#define ERRCHANGE_NOTENOUGHWATERFLOW 2

#define STATUS_NOTHINSCHEDULED 2
#define STATUS_IN_QUEUE 3
#define STATUS_WATERING 4

#define OPEN 20
#define CLOSED 40
#define OPENING 60
#define CLOSING 80
#define UNKNOWN 100
#define CURRSENSEERROR 120
#define MANUALOPEN 140
#define OPENINGMANUALLY 160
#define CLOSINGMANUALLY 180
#define LOCKED 200
#define OFFLINE 220

#define UNKNOWNSOILMOISTURE 80

//In minutes
#define CLASSICSAFETYWAIT 180

typedef struct errorStates{
    bool waterPressureHigh;
    bool notEnoughWaterFlow;
    bool oneOrMoreValvesNotClosed;
    bool oneOrMoreValveErrors;
    bool oneOrMoreValvesOffline;
}errorStates;

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
    uint8_t wateringStatus;
    uint8_t valveStatus;
    uint8_t autoWatering;
    wateringProgress progress;
    uint8_t unsuccessfulRequests;
    uint8_t safetyTimeActive;
    uint16_t safetyMinutesLeft;
}plant;


typedef struct plantDataChange{
    plant plantToChange;
    uint8_t parameterType;
}plantChange;



typedef struct errorChange{
    uint8_t errType;
    bool newState;
}errorChange;


/**
 * Initializes the main Variables, reads Data from FAT Partition, if it exists
 * Start RTOS Task
 */
void initializeVariablePool();

/**
 * Adds change request for plants to the RTOS Queue: plantChangeQueue
 * Adding and Removing is also happening here
 * This should be used as the ONLY interface to make changes to the plantList
 * The plantChangeQueue prevents concurrent writing of changes to the List
 * @param plantToChange - copy of the plant, which should be changed
 * @param parameterType - defines, what exactly should be changed: CHANGE_* Constants are used here
 */
void changePlant(plant plantToChange, uint8_t parameterType);

void changeErrorState(uint8_t errType, bool newState);


/**
 * Get the whole plant list
 * @return Pointer to plant list
 */
plant * getVariablePool();


/**
 * Gets new plant with standard init values
 * @return copy of new plant object
 */
plant getNewPlant();


/**
 * Adds Error message
 * @param plantAddress - Address of plant
 * @param errorType - Error Type, to know how the error message will look like
 */
void addErrorMessage(uint8_t plantAddress, uint8_t errorType);



#endif