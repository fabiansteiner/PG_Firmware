#include <stdio.h>

#ifndef CALCULATEVOLUMETRICCONTENT_H
#define CALCULATEVOLUMETRICCONTENT_H


/*
*	Interpolation between the following points
*			x		y
*	P1		233		0%
*	P2		417		16%
*	P3		540		37%
*	P4		569		49%
*/

#define ADC_P1 233
#define ADC_P2 417
#define ADC_P3 540
#define ADC_P4 569

#define INTER_K_P1_P2 0.0852515f
#define INTER_K_P2_P3 0.1726569f
#define INTER_K_P3_P4 0.4244032f

#define INTER_D_P1_P2 0.0f
#define INTER_D_P2_P3 15.6862745f
#define INTER_D_P3_P4 36.9230769f

#define INTER_X1_P1_P2 233
#define INTER_X1_P2_P3 417
#define INTER_X1_P3_P4 540

uint8_t getWaterContent(uint8_t highByte, uint8_t lowByte);

#endif