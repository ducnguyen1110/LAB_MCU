/*
 * input_processing.h
 *
 *  Created on: Oct 26, 2026
 *      Author: ASUS
 */

/*
 */

#ifndef INC_INPUT_PROCESSING_H_
#define INC_INPUT_PROCESSING_H_

#include "main.h"

extern int currentMode;
extern int red_duration;
extern int amber_duration;
extern int green_duration;

void fsm_for_input_processing(void);
void auto_increase_tick_10ms(void);

#endif /* INC_INPUT_PROCESSING_H_ */
