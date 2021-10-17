/*
 * ATTINY85_SoftwareUart.c
 *
 * Created: 02.10.2020 17:02:57
 * Author : Gus
 */ 

#define F_CPU 8000000UL

#define ACK 'A'
#define NACK 'N'
#define RES 'R'


#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>

#include "PLCUART.h"
#include "ADC.h"
#include "EEPROM.h"
#include "buttonLed.h"


char acknowledge = ACK;
bool resetflag = false;
uint16_t resetTimeout = 0;

uint8_t state;
char receivedMessage[8] = {0};
char answer[8] = {0};
uint8_t myAddress =  254;	//Initialise with non existing address

bool registered = false;
bool readyToReceiveAddress = false;

//uint16_t buttonPressCount = 0;

bool checkRequest();
void processRequest();
void sendAnswer();
void actOutButtonPress(uint8_t buttonAction);
void reset();

int main(void)
{
	sei();
	uart_init();
	//clearEEPROM();
	eeprom_init();
	adc_init();
	
	//LED
	DDRB |= (1<<DDB0);
	//Button Pull-up
	PORTA |= (1<<PORTA6);

	enableButtonDetection();

	//read Address

	//reset();
	//myAddress = 0b01010110;
	//myAddress = 255;
	myAddress = readAddress();
	if(myAddress != 255){
		registered = true;
	}

	pickAnimation(LED_STARTUP);
	goToDefinedPosition();	//Drive valve to a defined position, when power was off during movement (ManualOPEN or CLOSED)


    while (1) 
    {
		if(usiuart_getCommand(receivedMessage) == true){
			if((registered || readyToReceiveAddress)){
				bool requestIsValid = checkRequest();
				if(requestIsValid){
					processRequest();
					_delay_ms(3);
					sendAnswer();
				}
			}
			memset(receivedMessage, 0, sizeof(receivedMessage));		//Reset char array.
		}
		uint8_t buttonPress = detectButtonAction();

		//buttonPressCount++;
		//if(buttonPressCount == 6000){
		//	buttonPress = BUTTON_NORMALPRESS;
		//	buttonPressCount=0;
		//}
		if(buttonPress){
			actOutButtonPress(buttonPress);
		}
		

		_delay_ms(10);	//Essential for timing the Led animations correct
		calculateAverageCurrentSensorValues();
		animateLed();

		
		

		if(resetflag == true){
			resetTimeout++;
			if(resetTimeout >= 600){	//If 6 Seconds have passed, without being requested from the master, the reset takes place anyways
				reset();
			}
		}
		
    }
}


uint8_t calculateChecksum(char * checkMessage){
	//Calculate checksum of received message
	uint16_t checksum = 0;
	for(int i = 0; i<7; i++){
		checksum += checkMessage[i];
	}
	checksum = checksum % 255;

	return checksum;
}

/**
 * - check, if the Address is my address
 * - calculate checksum with divider of 255
 */
bool checkRequest(){

	//Get checksum
	uint8_t checksum = calculateChecksum(receivedMessage);
	//Comparing with ~myAddress does not work for whatever reason, making this variable works
	char reversedAddress = ~myAddress;
	
	//Check, if the received address matches my address + matches with checksum
	if(receivedMessage[0] == myAddress && receivedMessage[1] == reversedAddress && receivedMessage[7] == checksum){
		//pickAnimation(LED_SHORTBLINK);
		return true;
	}
	return false;
}

/**
 * check what master wants to do and take action
 */
void processRequest(){

	acknowledge = ACK;

	switch(receivedMessage[2]){
		case LOCKCOMMAND: if (getValveState() == CLOSED || getValveState()==LOCKED){ lockValve(true);}else{acknowledge=NACK;}
			break;
		case READSTATUS: if(getValveState() == LOCKED) lockValve(false);
			break;
		case READSTATUSWITHOUTUNLOCK://do nothing
			break;
		case OPENVALVE:if(getValveState()==LOCKED) openValve();
			break;
		case CLOSEVALVE:closeValve();
			break;
		case RECEIVEDVALVEADDRESS: writeAddress(receivedMessage[3]); myAddress = receivedMessage[3]; registered = true; readyToReceiveAddress = false; pickAnimation(LED_REGISTEREDORRESET);
			break;
	}	

	
}

void reset(){
	clearAddress();
	myAddress = 255;

	if(getValveState() != OPEN && getValveState() != MANUALOPEN){
		openValveManually();
	}

	resetflag = false;
	resetTimeout = 0;
	registered = false;
}

void sendAnswer(){

	if(resetflag == true){
		acknowledge = RES;
	}

	uint16_t adc_soil = getSoilMoisture();

	answer[0] = myAddress;
	answer[1] = ~myAddress;
	answer[2] = (uint8_t)(adc_soil>>8);		//Higher Byte
	answer[3] = (uint8_t)adc_soil;			//Lower Byte
	answer[4] = getValveState();
	answer[5] = acknowledge;						
	answer[6] = 0;							//reserved for more information later
	answer[7] = calculateChecksum(answer);	//checksum

	usiuart_printStr(answer);

	if(resetflag == true){
		_delay_ms(200);						//Wait until reset message has been sent completely
		reset();	
	}

}


void actOutButtonPress(uint8_t buttonAction){
	if(buttonAction == BUTTON_NORMALPRESS){
		if(!readyToReceiveAddress){
			if(getValveState() == MANUALOPEN || getValveState() == OPENINGMANUALLY){
				closeValveManually();
			}else if(getValveState() == CLOSED || getValveState() == CLOSINGMANUALLY){
				openValveManually();
			}
		}
		
	}
	if(buttonAction == BUTTON_5SEC_PRESS && (getValveState() == CLOSED || getValveState() == MANUALOPEN)){
		if(registered){
			pickAnimation(LED_REGISTEREDORRESET);
			resetflag = true;
		}else{
			readyToReceiveAddress = true;
			pickAnimation(LED_FASTBLINK);
		}
	}
	if(buttonAction == BUTTON_5XPRESS && getValveState() == MANUALOPEN){
		calibrationClosing();
	}
}


/*

USEFUL SHIT

if(strncmp(toCompare, string, 8)==0){
memset(string, 0, sizeof(string));		//Reset char array.
*/

