/*
 * EEPROM.c
 *
 * Created: 07.12.2020 19:03:43
 *  Author: Gus
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

#include "EEPROM.h"
#include "ADC.h"

#define ADDRESSPOINTER 0
#define VALVESTATEPOINTER 1
#define NONEXISTINGADDRESS 254


void EEPROMwriteByte(uint8_t address, uint8_t data);
uint8_t EEPROMreadByte(uint8_t address);

/**
 * See if magic byte is written. 
 * --> if not, set everything to a well known state (Address = 255, valveState = MANUALOPEN)
 */
void eeprom_init(){
	uint8_t magicByte = EEPROMreadByte(MAGICBYTEADDRESS);
	if(magicByte != MAGICBYTENUMBER){
		writeAddress(UNREGISTEREDSLAVEADDRESS);
		EEPROMwriteByte(MAGICBYTEADDRESS, MAGICBYTENUMBER);
		writeMotorStatus(MANUALOPEN);
	}
}

 void clearEEPROM(){
	for(int i = 0; i < 256; i++){
		EEPROMwriteByte(i, 0);
	}
 }

 void clearAddress(){
	EEPROMwriteByte(ADDRESSPOINTER, 255);
 }

 void writeAddress(uint8_t address){
	EEPROMwriteByte(ADDRESSPOINTER, address);
 }

 /**
 * read Slave Address of Power Line Communication from specific EEPROM Address
 * @return PLCAddress
 */
uint8_t readAddress(){
	return EEPROMreadByte(ADDRESSPOINTER);
}

void writeMotorStatus(uint8_t state){
	EEPROMwriteByte(VALVESTATEPOINTER, state);
}

uint8_t readMotorStatus(){
	return EEPROMreadByte(VALVESTATEPOINTER);
}

void EEPROMwriteByte(uint8_t address, uint8_t data){
	
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE));
	/* Set Programming mode */
	EECR = (0<<EEPM1)|(0<<EEPM0);
	/* Set up address and data registers */
	EEARH = 0;
	EEARL = address;
	EEDR = data;
	cli();
	/* Write logical one to EEMPE */
	EECR |= (1<<EEMPE);
	/* Start eeprom write by setting EEPE */
	EECR |= (1<<EEPE);
	sei();
}

uint8_t EEPROMreadByte(uint8_t address){
	/* Wait for completion of previous write */
	while(EECR & (1<<EEPE));
	/* Set up address register */
	EEARH = 0;
	EEARL = address;
	/* Start eeprom read by writing EERE */
	cli();
	EECR |= (1<<EERE);
	sei();
	/* Return data from data register */
	return EEDR;
}