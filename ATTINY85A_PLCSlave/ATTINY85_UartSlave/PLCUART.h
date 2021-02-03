/*
 * EEPROM.h
 *
 * Library for 2400 Baud Software Serial
 *
 * Created: 07.12.2020 19:03:56
 *  Author: Gus
 */ 

#ifndef PLCUART_H_
#define PLCUART_H_

#include <stdint.h>
#include <stdbool.h>

#define LOCKCOMMAND 10
#define READSTATUS 20 //+Unlock
#define READSTATUSWITHOUTUNLOCK 25
#define OPENVALVE 30
#define CLOSEVALVE 50
#define RECEIVEDVALVEADDRESS 80

/**
 * Initialize PLCUART
 */
void uart_init(void);

/**
 * Get next command, if its exists
 * @param dst Pointer to the char array
 * @return False = no command, True = Next command received
 */
bool usiuart_getCommand(char* dst);

/**
 * Transmit a string.
 * Interrupts every ongoing RX.
 * @param string Null terminated string
 * @return True = Success, False = currently Sending
 */
bool usiuart_printStr(char* string);

#endif /* PLCUART_H_ */
