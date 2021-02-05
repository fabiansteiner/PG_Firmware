/*
 * ADC.h
 *
 *	Library for getting the Soil Moisture
 *	+ Valve Control Library
 *	shared Resource = ADC
 *	other resources needed: Timer1 Overflow Interrupt (for Motor Movement Timeout), EEPROM for reading the Motor State in the beginning
 *
 * Created: 04.12.2020 22:35:31
 *  Author: Gus
 */ 

#include <stdio.h>
#include <stdbool.h>

#ifndef ADC_H_
#define ADC_H_

#define OPEN 20
#define CLOSED 40
#define OPENING 160
#define CLOSING 80
#define UNKNOWN 100
#define CURRSENSEERROR 120
#define MANUALOPEN 140
#define OPENINGMANUALLY 60
#define CLOSINGMANUALLY 180
#define LOCKED 200


/**
 * Initialize ADC
 */
void adc_init(void);

/**
 * locks or unlocks the valve for manual Interfacing
 * @param lock, true = lock valve for manual interfacing, false = unlock valve
 */
void lockValve(bool lock);

/**
 * opens Valve
 */
void openValve();

/**
 * opens Valve
 */
void openValveManually();

/**
 * closes Valve
 */
void closeValve();

/**
 * closes Valve
 */
void closeValveManually();

/**
 * Get the State of the Valve (Open/closed/opening/closing/unnknown/Failure Codes)
 * @return State of Valve
 */
uint8_t getValveState();

/**
 * Gets the Soil Moisture of the Plan in percent
 * @return Soil Moisture
 */
uint16_t getSoilMoisture();

/**
 * If a power down occurs while the valve is moving, or a CURRSENSERROR occurs, the motor will go to a defined state on power up again
 */
void goToDefinedPosition();

void calculateAverageCurrentSensorValues();



#endif /* ADC_H_ */