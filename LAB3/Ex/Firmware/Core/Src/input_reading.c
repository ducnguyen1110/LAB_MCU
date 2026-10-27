/*
 * input_reading.c
 *
 *  Created on: Oct 26, 2026
 *      Author: ASUS
 */



#include "main.h"
#include "input_reading.h"


#define DURATION_FOR_AUTO_INCREASING 100
#define BUTTON_IS_PRESSED            GPIO_PIN_RESET
#define BUTTON_IS_RELEASED           GPIO_PIN_SET


static GPIO_PinState buttonBuffer[NO_OF_BUTTONS] = {
    BUTTON_IS_RELEASED, BUTTON_IS_RELEASED, BUTTON_IS_RELEASED
};

static GPIO_PinState debounceButtonBuffer1[NO_OF_BUTTONS] = {
    BUTTON_IS_RELEASED, BUTTON_IS_RELEASED, BUTTON_IS_RELEASED
};
static GPIO_PinState debounceButtonBuffer2[NO_OF_BUTTONS] = {
    BUTTON_IS_RELEASED, BUTTON_IS_RELEASED, BUTTON_IS_RELEASED
};


static uint8_t flagForButtonPress1s[NO_OF_BUTTONS] = {0, 0, 0};


static uint16_t counterForButtonPress1s[NO_OF_BUTTONS] = {0, 0, 0};


static uint16_t BUTTON_PINS[NO_OF_BUTTONS] = {
    Button1_Pin, Button2_Pin, Button3_Pin
};
static GPIO_TypeDef* BUTTON_PORTS[NO_OF_BUTTONS] = {
    Button1_GPIO_Port, Button2_GPIO_Port, Button3_GPIO_Port
};

void button_reading(void) {
    for (unsigned char i = 0; i < NO_OF_BUTTONS; i++) {
        debounceButtonBuffer2[i] = debounceButtonBuffer1[i];
        debounceButtonBuffer1[i] = HAL_GPIO_ReadPin(BUTTON_PORTS[i], BUTTON_PINS[i]);

        if (debounceButtonBuffer1[i] == debounceButtonBuffer2[i]) {
            buttonBuffer[i] = debounceButtonBuffer1[i];

            if (buttonBuffer[i] == BUTTON_IS_PRESSED) {

                if (counterForButtonPress1s[i] < DURATION_FOR_AUTO_INCREASING) {
                    counterForButtonPress1s[i]++;
                } else {

                    flagForButtonPress1s[i] = 1;
                }
            } else {
                counterForButtonPress1s[i] = 0;
                flagForButtonPress1s[i] = 0;
            }
        }
    }
}

unsigned char is_button_pressed(unsigned char index) {
    if (index >= NO_OF_BUTTONS) return 0;
    return (buttonBuffer[index] == BUTTON_IS_PRESSED);
}

unsigned char is_button_pressed_1s(unsigned char index) {
    if (index >= NO_OF_BUTTONS) return 0xff;
    return (flagForButtonPress1s[index] == 1);
}
