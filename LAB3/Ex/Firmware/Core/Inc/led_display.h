/*
 * led_display.h
 *
 *  Created on: Oct 27, 2026
 *      Author: ASUS
 */


#ifndef INC_LED_DISPLAY_H_
#define INC_LED_DISPLAY_H_

#include "main.h"

void update_led_buffer_mode(int val1, int val2);
void scan_7seg_display(void);
void blink_traffic_leds(int mode);


void reset_normal_traffic(void);
void run_normal_traffic(void);

#endif /* INC_LED_DISPLAY_H_ */
