/*
 * buttonLed.c
 *
 * Created: 29.12.2020 22:18:00
 *  Author: Gus
 */ 

 #include <avr/io.h>
 #include <avr/interrupt.h>
 #include <stdbool.h>

 #include "buttonLed.h"

 uint8_t buttonPressCounter = 0;
 uint8_t timeOutCounter = 0;
 uint16_t buttonDownTime = 0;

 uint8_t pickedButtonPress = 0;
 uint8_t returnButtonPress = 0;


 bool debounceState = false;
 uint8_t debounceCounter = 0;


 uint8_t currentLedAnimation = LED_OFF;
 uint16_t animationProgress = 0;
 uint16_t animationTime = 0;

 /**
 * Initializes Pin Change interrupt to detect, if the Button has been pressed
 */
void enableButtonDetection(){
	GIMSK  |= (1<<PCIF0);				//Enables PinChange interrupt for PCINT0-PCINT7
	PCMSK0 |= (1<<PCINT6);			//Makes a Mask for PCINT6 --> Only changes on PCINT6 (PA6) are captured
}

void disableButtonDetection(){
	//GIMSK  &= ~(1<<PCIF0);				//Enables PinChange interrupt for PCINT0-PCINT7
	PCMSK0 &= ~(1<<PCINT6);
}

void resetButtonCounters(){
	buttonPressCounter = 0;
	timeOutCounter = 0;
	buttonDownTime = 0;
}


/**
 * Detects, if a defined Button action has been completed --> called every 10ms
 * @return 0, if nothing was pressed, and the codes for the Button presses (BUTTON_......) if something has been pressed
 */
uint8_t detectButtonAction(){
	
	//Disable debouncing after 20ms
	if(debounceState == true){
		debounceCounter++;
		if (debounceCounter >= 2){
			debounceState = false;
		}
	}

	if(buttonPressCounter > 0){
		if((PINA & (1<<PINA6))==0){	
			buttonDownTime++;
			if(buttonDownTime >= 500){
				pickedButtonPress = BUTTON_5SEC_PRESS;
				resetButtonCounters();
			}
		}else{
			timeOutCounter++;
			if(timeOutCounter >= 100){
				pickedButtonPress = BUTTON_NORMALPRESS;
				resetButtonCounters();
			}
		}
		
	}

	uint8_t returnButtonPress = pickedButtonPress;
	pickedButtonPress = 0;

	return returnButtonPress;
}

/**
 * Pick LED animation
 */
void pickAnimation(uint8_t ledAnimation){
	currentLedAnimation = ledAnimation;
	animationProgress = 0;
	animationTime = 0;
	if(ledAnimation == LED_GLOW || ledAnimation == LED_STARTUP || LED_SHORTBLINK){
		PORTB |= (1<<PORTB0);
	}
	
}

/**
 * Act out chosen LED animation
 */
void animateLed(){
	//called every 10ms
	if(currentLedAnimation == LED_MOVEVALVE){
		animationProgress++;
		if(animationProgress >= 80){
			PORTB ^= (1<<PORTB0);
			animationProgress = 0;
		}
	}

	if(currentLedAnimation == LED_STARTUP){
		animationProgress++;
		if(animationProgress >= 300){
			pickAnimation(LED_OFF);
		}
	}

	if(currentLedAnimation == LED_SHORTBLINK){
		animationProgress++;
		if(animationProgress >= 10){
			pickAnimation(LED_OFF);
		}
	}

	if(currentLedAnimation == LED_FASTBLINK){
		animationProgress++;
		if(animationProgress >= 20){
			PORTB ^= (1<<PORTB0);
			animationProgress = 0;
		}
	}

	if(currentLedAnimation == LED_REGISTEREDORRESET){
		animationProgress++;
		if(animationProgress >= 100){
			PORTB ^= (1<<PORTB0);
			animationProgress = 0;
			animationTime++;
			if(animationTime >= 5){
				pickAnimation(LED_OFF);
			}
		}
	}


	if(currentLedAnimation == LED_OFF && (PINA & (1<<PINA6)) != 0){
		PORTB &= ~(1<<PORTB0);
	}
	

}

void startDebouncing(){
	debounceState = true;
	debounceCounter = 0;
}


ISR(PCINT0_vect){
	if(!debounceState){
		if((PINA & (1<<PINA6))==0){		//Button has been pressed
			PORTB |= (1<<PORTB0);
			buttonPressCounter++;
			timeOutCounter = 0;
			startDebouncing();
		}else{							//Button has been released
			PORTB &= ~(1<<PORTB0);
			buttonDownTime = 0;
			startDebouncing();
		}
	}
	
	
}