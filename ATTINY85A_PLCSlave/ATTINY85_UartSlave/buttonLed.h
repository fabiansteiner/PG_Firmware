/*
 * buttonLed.h
 *
 * Created: 29.12.2020 22:18:12
 *  Author: Gus
 */ 


#define LED_MOVEVALVE 1
#define LED_REGISTEREDORRESET 2
#define LED_STARTUP 3
#define LED_GLOW 4
#define LED_OFF 5
#define LED_FASTBLINK 6
#define LED_SHORTBLINK 7

#define BUTTON_NORMALPRESS 1
#define BUTTON_5SEC_PRESS 2
#define BUTTON_5XPRESS 3
#define BUTTON_MANUALWATERING 10

#ifndef BUTTONLED_H_
#define BUTTONLED_H_


/**
 * Initializes Pin Change interrupt to detect, if the Button has been pressed
 */
void enableButtonDetection();

void disableButtonDetection();


/**
 * Detects, if a defined Button action has been completed
 * @return 0, if nothing was pressed, and the codes for the Button presses (BUTTON_......) if something has been pressed
 */
uint8_t detectButtonAction();

/**
 * Act out chosen LED animation
 */
void animateLed();

/**
 * Pick LED animation
 */
void pickAnimation(uint8_t ledAnimation);


#endif /* BUTTONLED_H_ */