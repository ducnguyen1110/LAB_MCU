#include "led_display.h"
#include "input_processing.h"

#ifndef TIMER_CYCLE
#define TIMER_CYCLE 10
#endif


static const uint8_t segmentCodes[10] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};

static int led_buffer[4] = {0, 5, 0, 3};
static int current_seg_index = 0;


enum TrafficState {
    PHASE_RED_GREEN,
    PHASE_RED_AMBER,
    PHASE_GREEN_RED,
    PHASE_AMBER_RED
};

static enum TrafficState traffic_state = PHASE_RED_GREEN;
static int time_road1 = 5;
static int time_road2 = 3;
static int counter_1s = 0;

void update_led_buffer_mode(int val1, int val2) {
    led_buffer[0] = val1 / 10;
    led_buffer[1] = val1 % 10;
    led_buffer[2] = val2 / 10;
    led_buffer[3] = val2 % 10;
}

static void display7SEG(int num) {
    if (num < 0 || num > 9) return;
    uint8_t code = segmentCodes[num];
    for (int i = 0; i < 7; i++) {
        GPIO_PinState state = (code & (1 << i)) ? GPIO_PIN_RESET : GPIO_PIN_SET;
        HAL_GPIO_WritePin(GPIOB, (uint16_t)(1 << i), state);
    }
}


void scan_7seg_display(void) {
    HAL_GPIO_WritePin(GPIOB, EN1_Pin | EN2_Pin | EN3_Pin | EN4_Pin, GPIO_PIN_SET);

    display7SEG(led_buffer[current_seg_index]);

    switch (current_seg_index) {
        case 0:
            HAL_GPIO_WritePin(EN1_GPIO_Port, EN1_Pin, GPIO_PIN_RESET);
            break;
        case 1:
            HAL_GPIO_WritePin(EN2_GPIO_Port, EN2_Pin, GPIO_PIN_RESET);
            break;
        case 2:
            HAL_GPIO_WritePin(EN3_GPIO_Port, EN3_Pin, GPIO_PIN_RESET);
            break;
        case 3:
            HAL_GPIO_WritePin(EN4_GPIO_Port, EN4_Pin, GPIO_PIN_RESET);
            break;
    }

    current_seg_index++;
    if (current_seg_index >= 4) {
        current_seg_index = 0;
    }
}


static void set_traffic_WaitPins(enum TrafficState state) {

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                             GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);

    switch (state) {
        case PHASE_RED_GREEN:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
            break;
        case PHASE_RED_AMBER:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
            break;
        case PHASE_GREEN_RED:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
            break;
        case PHASE_AMBER_RED:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
            break;
    }
}


void reset_normal_traffic(void) {
    traffic_state = PHASE_RED_GREEN;
    time_road1 = red_duration;
    time_road2 = green_duration;
    counter_1s = 0;
    set_traffic_WaitPins(traffic_state);
    update_led_buffer_mode(time_road1, time_road2);
}


void run_normal_traffic(void) {
    counter_1s++;
    if (counter_1s >= (1000 / TIMER_CYCLE)) {
        counter_1s = 0;
        time_road1--;
        time_road2--;

        switch (traffic_state) {
            case PHASE_RED_GREEN:
                if (time_road2 <= 0) {
                    traffic_state = PHASE_RED_AMBER;
                    time_road2 = amber_duration;
                    set_traffic_WaitPins(traffic_state);
                }
                break;

            case PHASE_RED_AMBER:
                if (time_road2 <= 0 || time_road1 <= 0) {
                    traffic_state = PHASE_GREEN_RED;
                    time_road1 = green_duration;
                    time_road2 = red_duration;
                    set_traffic_WaitPins(traffic_state);
                }
                break;

            case PHASE_GREEN_RED:
                if (time_road1 <= 0) {
                    traffic_state = PHASE_AMBER_RED;
                    time_road1 = amber_duration;
                    set_traffic_WaitPins(traffic_state);
                }
                break;

            case PHASE_AMBER_RED:
                if (time_road1 <= 0 || time_road2 <= 0) {
                    traffic_state = PHASE_RED_GREEN;
                    time_road1 = red_duration;
                    time_road2 = green_duration;
                    set_traffic_WaitPins(traffic_state);
                }
                break;
        }

        update_led_buffer_mode(time_road1, time_road2);
    }
}


void blink_traffic_leds(int mode) {
    if (mode == 1) {
        run_normal_traffic();
        return;
    }

    static int blink_counter = 0;
    static uint8_t blink_state = 0;

    blink_counter++;
    if (blink_counter >= (250 / TIMER_CYCLE)) {
        blink_counter = 0;
        blink_state = !blink_state;

        GPIO_PinState pin_state = blink_state ? GPIO_PIN_RESET : GPIO_PIN_SET;

        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                                 GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);

        if (mode == 2) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_1, pin_state);
        } else if (mode == 3) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_2, pin_state);
        } else if (mode == 4) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7 | GPIO_PIN_3, pin_state);
        }
    }
}
