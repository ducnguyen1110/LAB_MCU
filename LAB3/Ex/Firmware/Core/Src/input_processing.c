/*
 * input_processing.c
 *
 *  Created on: Oct 26, 2026
 *      Author: ASUS
 */

/*
 * input_processing.c
 */

#include "main.h"
#include "input_reading.h"
#include "input_processing.h"
#include "led_display.h"

enum SystemMode {
    MODE_NORMAL = 1,
    MODE_MODIFY_RED = 2,
    MODE_MODIFY_AMBER = 3,
    MODE_MODIFY_GREEN = 4
};


enum ButtonState {
    BUTTON_RELEASED,
    BUTTON_PRESSED,
    BUTTON_PRESSED_MORE_THAN_1_SECOND
};

int currentMode = MODE_NORMAL;

int red_duration = 5;
int amber_duration = 2;
int green_duration = 3;
static int temp_duration = 5;

static enum ButtonState btn1State = BUTTON_RELEASED;
static enum ButtonState btn2State = BUTTON_RELEASED;
static enum ButtonState btn3State = BUTTON_RELEASED;


static int counter_500ms = 0;
static uint8_t flag_500ms = 0;


void auto_increase_tick_10ms(void) {
    if (btn2State == BUTTON_PRESSED_MORE_THAN_1_SECOND) {
        counter_500ms++;
        if (counter_500ms >= 50) {
            counter_500ms = 0;
            flag_500ms = 1;
        }
    } else {
        counter_500ms = 0;
        flag_500ms = 0;
    }
}

static void increase_temp_duration(void) {
    if (currentMode != MODE_NORMAL) {
        temp_duration++;
        if (temp_duration > 99) {
            temp_duration = 1;
        }
        update_led_buffer_mode(currentMode, temp_duration);
    }
}

void fsm_for_input_processing(void) {

	switch (btn1State) {
	        case BUTTON_RELEASED:
	            if (is_button_pressed(0)) {
	                btn1State = BUTTON_PRESSED;
	                currentMode++;
	                if (currentMode > MODE_MODIFY_GREEN) {
	                    currentMode = MODE_NORMAL;
	                }

	                if (currentMode == MODE_NORMAL) {
	                    reset_normal_traffic();
	                } else if (currentMode == MODE_MODIFY_RED) {
	                    temp_duration = red_duration;
	                    update_led_buffer_mode(currentMode, temp_duration);
	                } else if (currentMode == MODE_MODIFY_AMBER) {
	                    temp_duration = amber_duration;
	                    update_led_buffer_mode(currentMode, temp_duration);
	                } else if (currentMode == MODE_MODIFY_GREEN) {
	                    temp_duration = green_duration;
	                    update_led_buffer_mode(currentMode, temp_duration);
	                }
	            }
	            break;

        case BUTTON_PRESSED:
            if (!is_button_pressed(0)) {
                btn1State = BUTTON_RELEASED;
            }
            break;

        default:
            btn1State = BUTTON_RELEASED;
            break;
    }


    switch (btn2State) {
        case BUTTON_RELEASED:
            if (is_button_pressed(1)) {
                btn2State = BUTTON_PRESSED;

                increase_temp_duration();
            }
            break;

        case BUTTON_PRESSED:
            if (!is_button_pressed(1)) {
                btn2State = BUTTON_RELEASED;
            } else {

                if (is_button_pressed_1s(1)) {
                    btn2State = BUTTON_PRESSED_MORE_THAN_1_SECOND;
                }
            }
            break;

        case BUTTON_PRESSED_MORE_THAN_1_SECOND:
            if (!is_button_pressed(1)) {
                btn2State = BUTTON_RELEASED;
            } else {

                if (flag_500ms == 1) {
                    flag_500ms = 0;
                    increase_temp_duration();
                }
            }
            break;
    }


    switch (btn3State) {
        case BUTTON_RELEASED:
            if (is_button_pressed(2)) {
                btn3State = BUTTON_PRESSED;
                if (currentMode == MODE_MODIFY_RED) {
                    red_duration = temp_duration;
                } else if (currentMode == MODE_MODIFY_AMBER) {
                    amber_duration = temp_duration;
                } else if (currentMode == MODE_MODIFY_GREEN) {
                    green_duration = temp_duration;
                }
            }
            break;

        case BUTTON_PRESSED:
            if (!is_button_pressed(2)) {
                btn3State = BUTTON_RELEASED;
            }
            break;

        default:
            btn3State = BUTTON_RELEASED;
            break;
    }
}
