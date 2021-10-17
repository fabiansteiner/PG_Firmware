/*
 * EEPROM.h
 *
 * Library for the 512 Byte Onboard-EEPROM
 *
 * Created: 07.12.2020 19:03:56
 *  Author: Gus
 */ 

#include <stdio.h>

#define MAGICBYTEADDRESS 100
#define MAGICBYTEADDRESSADCVALUE 101
#define MAGICBYTENUMBER 170 //0b10101010
#define UNREGISTEREDSLAVEADDRESS 255

#ifndef EEPROM_H_
#define EEPROM_H_

/**
 * See if magic byte is written. 
 * --> if not, set everything to a well known state (Address = 255, valveState = MANUALOPEN)
 */
void eeprom_init();

/**
 * Clear EEPROM (write 0 to every Address in the EEPROM)
 */
void clearEEPROM();

/**
 * Clears PLC Slave Address
 */
void clearAddress();

/**
 * write Slave Address of Power Line Communication to EEPROM to specific Address
 * @param address Pointer to the char array
 */
void writeAddress(uint8_t address);

/**
 * read Slave Address of Power Line Communication from specific EEPROM Address
 * @return PLCAddress
 */
uint8_t readAddress();

/**
 * write State of Valve Motor
 * @param state Pointer to the char array
 */
void writeMotorStatus(uint8_t state);

/**
 * read Motor State
 * @return Motor State
 */
uint8_t readMotorStatus();

/**
 * write ADC Strength of closing and Opening Mechanism
 * @param strength - strength as ADC Value
 */
void writeMotorStrength(uint16_t strength);

/**
 * read Motor Strength
 * @return Motor Strength
 */
uint16_t readMotorStrength();



#endif /* EEPROM_H_ */