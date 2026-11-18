/*
 * scheduler.h
 *
 *  Created on: Nov 9, 2026
 *      Author: ASUS
 */

#ifndef INC_SCHEDULER_H_
#define INC_SCHEDULER_H_

#include <stdint.h>

#define SCH_TICK_MS 10U
#define SCH_MAX_TASKS 40U
#define NO_TASK_ID 0U

/* Program 1.7, PDF page 14. Delay is relative to the previous queue node;
 * only the head counts down. RunMe is 1 when the head is ready to dispatch.
 */
typedef struct {
    void (*pTask)(void);
    uint32_t Delay;
    uint32_t Period;
    uint8_t RunMe;
    uint32_t TaskID;
} sTask;

#define ERROR_SCH_NONE 0U
#define ERROR_SCH_TOO_MANY_TASKS 1U
#define ERROR_SCH_CANNOT_DELETE_TASK 2U
#define ERROR_SCH_INVALID_TASK 3U

/* Inspect in the debugger; modify tasks through the API only. */
extern volatile sTask SCH_tasks_G[SCH_MAX_TASKS];
extern volatile uint8_t Error_code_G;

/* Initialize before enabling TIM2. Add/Delete/Dispatch run only in main. */
void SCH_Init(void);
/* Timer ISR: decrease head Delay and set RunMe at expiry, O(1). */
void SCH_Update(void);
void SCH_Dispatch_Tasks(void);

/* Delay/period are ticks, at most INT32_MAX. Period 0 means one-shot.
 * Returns a nonzero stable ID on success, or 0 on invalid input/full pool.
 */
uint32_t SCH_Add_Task(void (*pFunction)(void),
                      uint32_t delay_ticks,
                      uint32_t period_ticks);

/* Returns 0 on success, 1 if the ID is not active. */
uint8_t SCH_Delete_Task(uint32_t taskID);

/* Elapsed milliseconds; wraps modulo 2^32. */
uint32_t get_time(void);

#endif /* INC_SCHEDULER_H_ */
