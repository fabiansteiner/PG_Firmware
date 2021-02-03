#include "PLCUART.h"

#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 8000000UL

#ifndef TIMER_DIVIDER
#define TIMER_DIVIDER 64
#endif

#ifndef BAUD
#define BAUD 2400
#endif

#ifndef RXBUFFERSIZE
#define RXBUFFERSIZE 8
#endif

#define RECEIVE 0
#define TRANSMIT 1
#define IDLE 2
#define CHECKSTARTBIT 3
#define WAITFOR_OTHERBYTES 4
#define SEND 5

#if 255 < (F_CPU/TIMER_DIVIDER)/BAUD + ((F_CPU/TIMER_DIVIDER)/BAUD)/2
#error USIUART.c: Required ticks overflows OCR0A, try increasing TIMER_DIVIDER
#endif

#define TIMER_TICK ((float) (F_CPU/TIMER_DIVIDER)/BAUD)			//Wie viele Timer counts pro übertragenes Bit, Bei 2400 Baud und Timer Divider von 64 ergibt das 52,08 Ticks pro Bit == 416uS
static char rxBuffer[RXBUFFERSIZE];
static volatile uint8_t state = IDLE;

int8_t bitCounter = 0;
uint8_t byteCounter = 0;
uint8_t received8Bit = 0;
uint8_t starbedingungCounter = 0;
uint8_t startBitValid = false;
bool bufferedCommand = false;
static volatile uint8_t tempTxChar;
static volatile char* currentAddress;

void initializeTimer(){
	
	//Carrier Signal generation
	DDRA |= (1<<DDA5);
	TCCR1A |= (1<<COM1B0);	//Toggle OC1B (PA5) on Compare Match
	TCCR1B |= (1<<WGM12) | (1<<CS10);	//CTC Mode
	OCR1A = 1;
}

void uart_init()
{
	initializeTimer();

	//PB0 and PB1 as input for RX, Comperator + and -
	DDRA &= ~_BV(DDA1);
	DDRA &= ~_BV(DDA2);

	//PA4 to rebuild RX
	//DDRA |= _BV(DDA4);
	//PORTA |= (1<<PORTA4);

	//PA4 as TX output, Set high because calm TX is high
	DDRA |= _BV(DDA4);
	PORTA |= (1<<PORTA4);

	//Enable Analog Comperator Interrupt
	ACSR |= (1<<ACIE);
	/*
	 * Set Timer settings
	 */
	TCCR0A |= _BV(WGM01);		//Enable CTC Mode
#if	TIMER_DIVIDER == 1
	TCCR0B |= _BV(CS00);
#elif TIMER_DIVIDER == 8
	TCCR0B |= _BV(CS01);
#elif TIMER_DIVIDER == 64
	TCCR0B |= _BV(CS01) | _BV(CS00);
#elif TIMER_DIVIDER == 256
	TCCR0B |= _BV(CS02);
#elif TIMER_DIVIDER == 1024
	TCCR0B |= _BV(CS02) | _BV(CS00);
#else
#error USIUART.c: Unsupported timer divider
#endif

}


void resetRXBufferAndIdle(){
	TIMSK0 &= ~(1<<OCIE0A);
	state = IDLE;
	memset(rxBuffer, 0, sizeof(rxBuffer));
	bitCounter = 0;
	byteCounter = 0;
	startBitValid = false;
}

bool usiuart_getCommand(char* dst)
{
	if(bufferedCommand == false) 
		return false;

	int i;
	for(i = 0; i<RXBUFFERSIZE; i++){
		dst[i] = rxBuffer[i];
	}
	bufferedCommand = false;
	resetRXBufferAndIdle();					//Clear after command is in main
	
	return true;
}


bool usiuart_printStr(char* string){
	
	if(state == SEND) return false;		//Currently sending

	//Disable Analog Comperator Interrupt (== Sending is priority, receiving gets interrupted if something is currently received)
	ACSR &= ~(1<<ACIE);
	resetRXBufferAndIdle();

	bitCounter = -1;
	currentAddress = string;
	TCNT0 = 0;
	OCR0A = TIMER_TICK-1;
	state = SEND;
	TIMSK0 |= (1<<OCIE0A); //Enable Timer Interrupt

	return true;
}



ISR(TIM0_COMPA_vect)
{
	if(state == CHECKSTARTBIT){
		starbedingungCounter++;
		if(startBitValid == false){
			if((ACSR & (1<<ACO))==0){					//If the signal stays low, a counter goes up.
				if (starbedingungCounter >= 23){		//Bleibt der Pegel lange genug unten ((TIMER_TICK/2)-3), wird ein gültiges Starbit erkannt
					startBitValid = true;
				}
			}else{										//Bleibt der Pegel nicht lange genug unten, wird kein Startbit erkannt und er geht zurück zum IDLE State und löscht alles
				resetRXBufferAndIdle();
			}
		}else{
			if(starbedingungCounter >= 26){				//Nachdem ein gültiges Startbit erkannt wurde, wechsle nach genau einer Bit Time (420us) in den RECEIVE status
				state = RECEIVE;
				OCR0A = (TIMER_TICK/2)-1; 
				startBitValid = false;
			}
		}
		
	}else if(state == RECEIVE){
		if(bitCounter > 7){								//Wenn ein ganzes Byte empfangen wurde
			TIMSK0 &= ~(1<<OCIE0A);	 
			bitCounter = 0;
			if((ACSR & (1<<ACO))!=0){					//Wenn das Stopp Bit erkannt wird, speichere empfangens Byte in Buffer
				rxBuffer[byteCounter] = received8Bit;
				received8Bit = 0;
				if(byteCounter == 7){					//Wenn 8 Bytes empfangen wurden, commando zum lesen freigeben
					bufferedCommand = true;
					state = IDLE;
					bitCounter = 0;
					byteCounter = 0;
				}else{									//Wenn noch keine 8 Bytes empfangen wurden wechle in the "Wartemodus" - dort wird 4 Bit times (1680us) auf das nächste Startbit gewartet
					state = WAITFOR_OTHERBYTES;
					OCR0A = TIMER_TICK*4;
					TIMSK0 |= (1<<OCIE0A);
				}
				byteCounter++;
			}else{										//Wenn das stopp bit nicht erkannt wird, alles verwerfen und zurück in den IDLE State
				resetRXBufferAndIdle();
			}
		}else{
			//Safe received bit
			if((ACSR & (1<<ACO))!=0){
				received8Bit |= (1<<bitCounter);
			}else{
				received8Bit &= ~(1<<bitCounter);
			}
			bitCounter++;
			OCR0A = TIMER_TICK-1; //Reload
		}

	}else if(state == WAITFOR_OTHERBYTES){			//TIMEOUT - Kommt das nächste Startbit nicht rechtzeitig, verwerfe alles und gehe zurück in den IDLE State
		resetRXBufferAndIdle();

	}else if(state == SEND){

		if(bitCounter == -1){
			tempTxChar = *currentAddress++;		//Get char at currentAddress and then increment Current address
			PORTA &= ~(1<<PORTA4);								//Initialize Startbit
			bitCounter++;
		}else if (bitCounter>=0 && bitCounter <= 7){
			//Send those bits
			if(!(tempTxChar & (1<<bitCounter))){
				PORTA &= ~(1<<PORTA4);
			}else{
				PORTA |= (1<<PORTA4);
			}
			bitCounter++;
		}else if (bitCounter == 8){
			//Send stop bit
			PORTA |= (1<<PORTA4);
			bitCounter=-1;
			byteCounter++;
			if(byteCounter == 8){	//if 8 Bytes were sent, return to Idle State
				resetRXBufferAndIdle();
				ACSR |= (1<<ACIE); //Re-Enable Analog Comperator Interrupt, receiving is active again
			}
		}
		
	}
}

ISR(ANA_COMP_vect){
	if (state == IDLE || state == WAITFOR_OTHERBYTES){
		//Initiate checking, if it was a Start Bit
		if ((ACSR & (1<<ACO))==0){
			state = CHECKSTARTBIT;
			starbedingungCounter = 0;
			TCNT0 = 0;
			OCR0A = 1;	//Ganzes Bit dauert 52 Timer Counts, um eine Startbedingung zu erkennen, muss das Signal minimum 40 Ticks, maximum 64 Ticks unten bleiben = 20 Overflows, 1 Tick = 8uS, ISR wird also alle 16us aufgerufen
			TIMSK0 |= (1<<OCIE0A); //Enable Timer Interrupt
		}
	}
	/*
	*No Resync, could cause Problems when Comperator Toggles, rely only on the SYNC of the Startbit should be enough for 8 Bits at only 2400Baud
	if (state == RECEIVE){
		TIFR = _BV(OCF0A);
		TCNT0 = 0;
		OCR0A = (TIMER_TICK/2)-1;
	}*/

	/*
	
	uint8_t output = (ACSR & (1<<ACO));
	if(output != 0){
		PORTA |= (1<<PORTA4);
	}else{
		PORTA &= ~(1<<PORTA4);
	}
	*/
	

}