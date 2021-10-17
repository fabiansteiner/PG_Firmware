/*
 * ADC.c
 *
 * Created: 04.12.2020 22:35:10
 *  Author: Gus
 */ 
#define F_CPU 8000000

#include <avr/io.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "ADC.h"
#include "EEPROM.h"
#include "buttonLed.h"

#define READSOILANDTEMP 1
#define OPENV 2
#define CLOSEV 3

#define STROMSENSOR1 0
#define STROMSENSOR2 7
#define SOILMOISTURESENSOR 3

#define DISCARDMOTOR 20				//80 = 60 Measurments per Second, 20 = 240 Measurements per second
#define DISCRADSOILSENSOR 14423		//Measurement every 3 seconds
#define CURRBUFFSIZE 40

//#define SIXTYMA 480		//ADC value at 65mA flowing current, // Miliamp Calculation = ((125/1023)*analogValue);

uint16_t motorTreshold = 500;
bool stopButton = false;

uint8_t valveState = CLOSED;
uint8_t adcflag = READSOILANDTEMP;

uint16_t discardcounter = 0;
uint16_t adcresult = 0;

volatile uint16_t currentRingBuffer1[CURRBUFFSIZE] = {0};
uint8_t currBuffcount1 = 0;
bool currBuff1FULL =false;
uint16_t averageCurrent1FirstHalf = 0;
uint16_t averageCurrent1SecondHalf = 0;
uint16_t averageCurrent1 = 0;

volatile uint16_t currentRingBuffer2[CURRBUFFSIZE] = {0};
uint8_t currBuffcount2 = 0;
bool currBuff2FULL =false;
uint16_t averageCurrent2FirstHalf = 0;
uint16_t averageCurrent2SecondHalf = 0;
uint16_t averageCurrent2 = 0;

int16_t difference;
uint16_t compareValue = 0;

uint16_t soiMoistureRingBuffer[10] = {0};
uint8_t soilBuffCount = 0;
bool soilBuffFULL =false;
uint16_t averageSoil = 0;

void selectADCChannnel(uint8_t channel);
void addRingBufferValueAndCalculateAverage(uint8_t type);
void open();
void close();
void resetBuffers();
void testOpen();
void testClose();

void changeValveState(uint8_t newState){
	if(valveState != newState){
		bool worthWriting = true;
		if(newState == LOCKED || (newState == CLOSED && valveState == LOCKED) || newState == CURRSENSEERROR || newState == CALIBRATING){
			worthWriting = false;
		}
		valveState = newState;
		if(worthWriting) writeMotorStatus(valveState);
	}
	
}

void adc_init(void){

	//Motor Pins as outputs
	DDRB |= (1<<DDB1) | (1<<DDB2);
	
	valveState = readMotorStatus();
	motorTreshold = readMotorStrength();
	
	ADCSRA|= (1<<ADPS0) | (1<<ADPS1) | (1<<ADPS2);			//Prescaler 128 = 62500 HZ = 62,5kHz ADC Clock, 1 Takt = 16uS, 1xADC Convertion in free running mode = 13 Cylces = 208uS x 2 for both current Sensors = 416uS for 1 Current Measurement cycle
	selectADCChannnel(SOILMOISTURESENSOR);						//standard ADC channel = 0 = STROMSENSOR1	
	adcflag = READSOILANDTEMP;
	ADCSRA|= (1<<ADEN) | (1<<ADATE) | (1<<ADIE);			//Enable ADC, Standard Voltage Reference = Vin = 5V, Enable Auto Triggering, standard = free running mode, enable adc interrupt	
	ADCSRA|= (1<<ADSC);										//Start first conversion16
	
	

}

void goToDefinedPosition(){
	//openValveManually();
	
	if(valveState == OPENING || valveState == CLOSING || valveState == CLOSINGMANUALLY || valveState == OPEN){
		closeValve();
	}else if(valveState == OPENINGMANUALLY){
		openValveManually();
	}
	
}

void selectADCChannnel(uint8_t channel){
	switch(channel){
		case STROMSENSOR1: ADMUX = 0b00000000;
			break;
		case STROMSENSOR2: ADMUX = 0b00000111;
			break;
		case SOILMOISTURESENSOR: ADMUX = 0b00000011;
			break;
	}
}

void lockValve(bool lock){
	if(lock == true){
		if(valveState==CLOSED){
			changeValveState(LOCKED);
			disableButtonDetection();
		 }
	}else{
		changeValveState(CLOSED);
		enableButtonDetection();
	}
	
}

uint8_t getValveState(){
	return valveState;
}

void stopButtonPressed(){
	stopButton = true;
}

void openValve(){
	if(valveState != OPEN && valveState != OPENING){
		changeValveState(OPENING);
		open();
	}
	
}

void openValveManually(){
	changeValveState(OPENINGMANUALLY);
	open();
}

void open(){
	//First four lines are needed if sudden switch from close to open occurs
	adcflag = READSOILANDTEMP;
	resetBuffers();
	PORTB &= ~(1<<PORTB1);
	PORTB &= ~(1<<PORTB2);

	pickAnimation(LED_MOVEVALVE);
	PORTB |= (1<<PORTB1);
	_delay_ms(300);
	adcflag = OPENV;
	selectADCChannnel(STROMSENSOR1);
	discardcounter = 0;
}

void testOpen(){
	//First four lines are needed if sudden switch from close to open occurs
	_delay_ms(4000);
	if(valveState == OPENING) changeValveState(OPEN);
	if(valveState == OPENINGMANUALLY) changeValveState(MANUALOPEN);
	
}

void closeValve(){
	if(valveState != CLOSED){
		changeValveState(CLOSING);
		close();
		pickAnimation(LED_MOVEVALVE);
	}
}

void closeValveManually(){
	changeValveState(CLOSINGMANUALLY);
	close();
	pickAnimation(LED_MOVEVALVE);
}

void calibrationClosing(){
	changeValveState(CALIBRATING);
	close();
	pickAnimation(LED_FASTBLINK);
}

void close(){
	//First four lines are needed if sudden switch from open to close occurs
	adcflag = READSOILANDTEMP;
	resetBuffers();
	PORTB &= ~(1<<PORTB1);
	PORTB &= ~(1<<PORTB2);

	
	PORTB |= (1<<PORTB2);
	//_delay_ms(110);
	adcflag = CLOSEV;
	selectADCChannnel(STROMSENSOR1);
	discardcounter = 0;
}

void testClose(){
	//First four lines are needed if sudden switch from close to open occurs
	_delay_ms(4000);
	changeValveState(CLOSED);
	
}

uint16_t getSoilMoisture(){
	//Calculate Average Soilmoisture over the last 3 seconds
	if(soilBuffFULL == true){
		averageSoil = 0;
		for(int j = 0; j < 10; j++){
			averageSoil += soiMoistureRingBuffer[j];
		}
		averageSoil = averageSoil/10;
	}
	return averageSoil;
}

void calcAverages(uint8_t sensorOne){
	
	uint16_t *averageCurrentFirstHalf;
	uint16_t *averageCurrentSecondHalf;
	uint16_t *averageCurrent;
	volatile uint16_t *currBuff;
	uint8_t index;
	
	if(sensorOne){
		averageCurrentFirstHalf = &averageCurrent1FirstHalf;
		averageCurrentSecondHalf = &averageCurrent1SecondHalf;
		averageCurrent = &averageCurrent1;
		currBuff = currentRingBuffer1;
		index = currBuffcount1;
	}else{
		averageCurrentFirstHalf = &averageCurrent2FirstHalf;
		averageCurrentSecondHalf = &averageCurrent2SecondHalf;
		averageCurrent = &averageCurrent2;
		currBuff = currentRingBuffer2;
		index = currBuffcount2;
	}

	*averageCurrentFirstHalf = 0;
	*averageCurrentSecondHalf = 0;
	*averageCurrent = 0;
	
	cli();


	for(int j = 0; j < (CURRBUFFSIZE/2); j++){
		if(index >= CURRBUFFSIZE){
			index = 0;
		}
		*averageCurrentFirstHalf += currBuff[index];
		index++;
	}

	for(int j = 0; j < (CURRBUFFSIZE/2); j++){
		if(index >= CURRBUFFSIZE){
			index = 0;
		}
		*averageCurrentSecondHalf += currBuff[index];
		index++;
	}

	
	sei();
	*averageCurrentFirstHalf /= (CURRBUFFSIZE/2);
	*averageCurrentSecondHalf /= (CURRBUFFSIZE/2);
	*averageCurrent = (*averageCurrentFirstHalf + *averageCurrentSecondHalf)/2; 
}

void calculateAverageCurrentSensorValues(){
	uint16_t averageCurrentFirstHalf = 0;
	uint16_t averageCurrentSecondHalf = 0;
	bool rampUP = false;

	if(currBuff1FULL == true){
		calcAverages(1);
	}

	if(currBuff2FULL == true){
		calcAverages(0);

		averageCurrentFirstHalf = (averageCurrent1FirstHalf + averageCurrent2FirstHalf)/2;
		averageCurrentSecondHalf = (averageCurrent1SecondHalf + averageCurrent2SecondHalf)/2;

		if(averageCurrentSecondHalf > averageCurrentFirstHalf+40){
			rampUP = true;
		}else{
			rampUP = false;
		}
			
	}

	if(currBuff2FULL == true){
		difference = averageCurrent2-averageCurrent1;
		if(abs(difference) > 250){ //If the measurement gap is more than 250 (= ~30mA), something is wrong --> Motor Off + Error State
			PORTB &= ~(1<<DDB1);
			PORTB &= ~(1<<DDB2);
			changeValveState(CURRSENSEERROR);
			adcflag = READSOILANDTEMP;
			selectADCChannnel(SOILMOISTURESENSOR);
			pickAnimation(LED_FASTBLINK);
		}

		if(valveState == CLOSINGMANUALLY || valveState == CLOSING){
			compareValue = motorTreshold;
		}else if (valveState == OPENING || valveState == OPENINGMANUALLY){
			compareValue = motorTreshold / 2;
			if(compareValue < 400) compareValue = 400;
		}else if (valveState == CALIBRATING){
			compareValue = 1015;
		}

		uint16_t endAverage = (averageCurrent1 + averageCurrent2)/2;
		

		if(((rampUP && endAverage >= compareValue)) || averageCurrent1 > 1020 || averageCurrent2 > 1020){
			PORTB &= ~(1<<DDB1);
			PORTB &= ~(1<<DDB2);
			if(valveState == OPENING){changeValveState(OPEN);}
			else if (valveState == CLOSING || valveState == CLOSINGMANUALLY){changeValveState(CLOSED); enableButtonDetection(); }
			else if(valveState == OPENINGMANUALLY){changeValveState(MANUALOPEN);}
			else if(valveState == CALIBRATING) {changeValveState(CLOSED); writeMotorStrength(1015); motorTreshold = 1015;}
			resetBuffers();
			pickAnimation(LED_OFF);
			adcflag = READSOILANDTEMP;
			selectADCChannnel(SOILMOISTURESENSOR);
			return;
		}

		if(stopButton){
			stopButton = false;
			PORTB &= ~(1<<DDB1);
			PORTB &= ~(1<<DDB2);
			changeValveState(CLOSED); 
			if(endAverage > 500){
				motorTreshold = ((averageCurrent1 + averageCurrent2)/2)+50;
				if(motorTreshold > 1015){
					motorTreshold = 1015;
				}
			}else{
				motorTreshold = 500;
			}
			
			writeMotorStrength(motorTreshold);

			resetBuffers();
			pickAnimation(LED_OFF);
			adcflag = READSOILANDTEMP;
			selectADCChannnel(SOILMOISTURESENSOR);

		}
	}

}


void resetBuffers(){
	for (int i = 0; i<10; i++){
		currentRingBuffer1[i] = 0;
		currentRingBuffer2[i] = 0;
	}
	currBuff1FULL = false;
	currBuff2FULL = false;
	currBuffcount1 = 0;
	currBuffcount2 = 0;

	discardcounter = 0;
}


ISR(ADC_vect){
	//If Valve opens or Closes
	if(adcflag == OPENV || adcflag == CLOSEV){
		if(discardcounter==(DISCARDMOTOR/2)){
			//Measurement of first sensor
			//Happens every 16,6ms*40
			//Read ADCL first, and then ADCH, to ensure values are from the same conversion
			adcresult = ADCL;
			adcresult |= (ADCH<<8);
			selectADCChannnel(STROMSENSOR2);

			addRingBufferValueAndCalculateAverage(STROMSENSOR1);
			
			
		}
		if(discardcounter>=DISCARDMOTOR){
			//Measurement of second sensor

			discardcounter = 0;
			adcresult = ADCL;
			adcresult |= (ADCH<<8);
			selectADCChannnel(STROMSENSOR1);
			addRingBufferValueAndCalculateAverage(STROMSENSOR2);

		}
	}
	if(adcflag == READSOILANDTEMP){
		if(discardcounter>=DISCRADSOILSENSOR){
			discardcounter = 0;
			//Happens every 3 Seconds
			//Read ADCL first, and then ADCH, to ensure values are from the same conversion
			adcresult = ADCL;
			adcresult |= (ADCH<<8);

			//if(adcresult > 650)
			//	pickAnimation(LED_SHORTBLINK);

			addRingBufferValueAndCalculateAverage(SOILMOISTURESENSOR);
		}
	}
	discardcounter++;
}

void addRingBufferValueAndCalculateAverage(uint8_t type){
	switch(type){
		case STROMSENSOR1:
			currentRingBuffer1[currBuffcount1]=adcresult;
			currBuffcount1++;
			if(currBuffcount1 >=CURRBUFFSIZE){
				currBuffcount1 = 0;
				currBuff1FULL = true;
			}
			
			break;
		case STROMSENSOR2:
			currentRingBuffer2[currBuffcount2]=adcresult;
			currBuffcount2++;
			if(currBuffcount2 >=CURRBUFFSIZE){
				currBuffcount2 = 0;
				currBuff2FULL = true;
			}
			

			break;
		case SOILMOISTURESENSOR:
			soiMoistureRingBuffer[soilBuffCount]=adcresult;
			soilBuffCount++;
			if(soilBuffCount >=10){
				soilBuffCount = 0;
				soilBuffFULL = true;
			}
			break;
	}
}
