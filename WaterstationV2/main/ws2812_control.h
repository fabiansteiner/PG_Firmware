#ifndef WS2812_CONTROL_H
#define WS2812_CONTROL_H
#include <stdint.h>
#include "sdkconfig.h"
#define NUM_LEDS 12

#define OFF 0x000000
#define RED   0x007700
#define GREEN 0xFF0000
#define BLUE  0x0000FF
#define YELLOW  0x555500
#define ORANGE 0x408800
#define VERYDIMMGREEN 0x170000

// This structure is used for indicating what the colors of each LED should be set to.
// There is a 32bit value for each LED. Only the lower 3 bytes are used and they hold the
// Red (byte 2), Green (byte 1), and Blue (byte 0) values to be set.
struct led_state {
    uint32_t leds[NUM_LEDS];
};

// Setup the hardware peripheral. Only call this once.
void ws2812_control_init(void);

// Update the LEDs to the new state. Call as needed.
// This function will block the current task until the RMT peripheral is finished sending 
// the entire sequence.
void ws2812_write_leds(struct led_state new_state);

#endif