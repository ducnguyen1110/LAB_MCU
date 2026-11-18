#include "scheduler.h"

#ifndef SCH_HOST_TEST
#include "stm32f1xx.h"
#endif

#define SCH_NO_INDEX SCH_MAX_TASKS

/* Task fields and task array from Program 1.7 of the lab handout. */
volatile sTask SCH_tasks_G[SCH_MAX_TASKS];
volatile uint8_t Error_code_G = ERROR_SCH_NONE;

/* Queue links are slot indices, independent of the stable TaskID. */
static uint32_t Next[SCH_MAX_TASKS];
static volatile uint32_t Head = SCH_NO_INDEX;
static volatile uint32_t tick_count;
static uint32_t next_task_id = 1U;

/* Elapsed ticks after a ready head's scheduled release. Do not lose time
 * while main is busy executing a callback or processing other work.
 */
static volatile uint32_t overdue_ticks;

static uint32_t SCH_Enter_Critical(void)
{
#ifdef SCH_HOST_TEST
    return 0U; /* Single-threaded host tests inject interrupts explicitly. */
#else
    uint32_t previous = __get_PRIMASK();
    __disable_irq();
    return previous;
#endif
}

static void SCH_Exit_Critical(uint32_t previous)
{
#ifdef SCH_HOST_TEST
    (void)previous;
#else
    __set_PRIMASK(previous);
#endif
}

/* Main only, interrupts masked. Releases at 50, 100, 150 ticks are stored
 * as delta delays 50 -> 50 -> 50. Equal releases are ordered by TaskID.
 */
static void SCH_Insert_Task(uint32_t index, uint32_t delay)
{
    uint32_t previous = SCH_NO_INDEX;
    uint32_t current = Head;

    while (current != SCH_NO_INDEX) {
        if (delay < SCH_tasks_G[current].Delay ||
            (delay == SCH_tasks_G[current].Delay &&
             SCH_tasks_G[index].TaskID < SCH_tasks_G[current].TaskID)) {
            break;
        }
        delay -= SCH_tasks_G[current].Delay;
        previous = current;
        current = Next[current];
    }

    SCH_tasks_G[index].Delay = delay;
    SCH_tasks_G[index].RunMe = 0U;
    Next[index] = current;
    if (current != SCH_NO_INDEX) {
        SCH_tasks_G[current].Delay -= delay;
    }
    if (previous == SCH_NO_INDEX) {
        if (Head != SCH_NO_INDEX) {
            SCH_tasks_G[Head].RunMe = 0U;
        }
        Head = index;
    } else {
        Next[previous] = index;
    }
}

/* Main only, interrupts masked. Preserve later release times on removal. */
static void SCH_Remove_Task(uint32_t index, uint32_t previous)
{
    uint32_t following = Next[index];
    if (following != SCH_NO_INDEX) {
        SCH_tasks_G[following].Delay += SCH_tasks_G[index].Delay;
    }
    if (previous == SCH_NO_INDEX) {
        Head = following;
    } else {
        Next[previous] = following;
    }
    Next[index] = SCH_NO_INDEX;
    SCH_tasks_G[index].RunMe = 0U;
}

/* Apply elapsed time to ONE head. Main will dispatch remaining overdue
 * tasks in order; the timer ISR never needs to walk the queue.
 */
static void SCH_Prepare_Head(uint32_t elapsed)
{
    if (Head == SCH_NO_INDEX) {
        overdue_ticks = 0U;
    } else if (SCH_tasks_G[Head].Delay <= elapsed) {
        overdue_ticks = elapsed - SCH_tasks_G[Head].Delay;
        SCH_tasks_G[Head].Delay = 0U;
        SCH_tasks_G[Head].RunMe = 1U;
    } else {
        SCH_tasks_G[Head].Delay -= elapsed;
        SCH_tasks_G[Head].RunMe = 0U;
        overdue_ticks = 0U;
    }
}

void SCH_Init(void)
{
    uint32_t previous = SCH_Enter_Critical();
    Head = SCH_NO_INDEX;
    tick_count = 0U;
    overdue_ticks = 0U;
    next_task_id = 1U;
    Error_code_G = ERROR_SCH_NONE;
    for (uint32_t i = 0U; i < SCH_MAX_TASKS; i++) {
        SCH_tasks_G[i] = (sTask){0};
        Next[i] = SCH_NO_INDEX;
    }
    SCH_Exit_Critical(previous);
}

/* Like Program 1.9: update Delay and mark RunMe, NEVER run the callback.
 * The delta queue needs only one countdown, so there is no O(n) search.
 */
void SCH_Update(void)
{
    tick_count++;
    if (Head == SCH_NO_INDEX) {
        return;
    }
    if (SCH_tasks_G[Head].RunMe != 0U) {
        overdue_ticks++;
        return;
    }
    if (SCH_tasks_G[Head].Delay > 0U) {
        SCH_tasks_G[Head].Delay--;
    }
    if (SCH_tasks_G[Head].Delay == 0U) {
        SCH_tasks_G[Head].RunMe++;
    }
}

uint32_t get_time(void)
{
    return tick_count * SCH_TICK_MS;
}

uint32_t SCH_Add_Task(void (*pFunction)(void),
                      uint32_t delay_ticks, uint32_t period_ticks)
{
    uint32_t previous = SCH_Enter_Critical();
    if (pFunction == 0 || next_task_id == NO_TASK_ID ||
        delay_ticks > 0x7FFFFFFFUL || period_ticks > 0x7FFFFFFFUL ||
        delay_ticks > UINT32_MAX - overdue_ticks) {
        Error_code_G = ERROR_SCH_INVALID_TASK;
        SCH_Exit_Critical(previous);
        return NO_TASK_ID;
    }

    uint32_t index = 0U;
    while (index < SCH_MAX_TASKS && SCH_tasks_G[index].pTask != 0) {
        index++;
    }
    if (index == SCH_MAX_TASKS) {
        Error_code_G = ERROR_SCH_TOO_MANY_TASKS;
        SCH_Exit_Critical(previous);
        return NO_TASK_ID;
    }

    SCH_tasks_G[index].pTask = pFunction;
    SCH_tasks_G[index].Period = period_ticks;
    SCH_tasks_G[index].TaskID = next_task_id++;
    uint32_t id = SCH_tasks_G[index].TaskID;

    /* Delay for a newly added task starts NOW, even if the head is overdue. */
    SCH_Insert_Task(index, overdue_ticks + delay_ticks);
    SCH_Prepare_Head(overdue_ticks);
    SCH_Exit_Critical(previous);
    return id;
}

uint8_t SCH_Delete_Task(uint32_t taskID)
{
    uint32_t saved_interrupts = SCH_Enter_Critical();
    uint32_t previous = SCH_NO_INDEX;
    uint32_t index = Head;
    while (index != SCH_NO_INDEX) {
        if (SCH_tasks_G[index].TaskID == taskID) {
            SCH_Remove_Task(index, previous);
            SCH_tasks_G[index] = (sTask){0};
            if (previous == SCH_NO_INDEX) {
                SCH_Prepare_Head(overdue_ticks);
            }
            SCH_Exit_Critical(saved_interrupts);
            return 0U;
        }
        previous = index;
        index = Next[index];
    }

    Error_code_G = ERROR_SCH_CANNOT_DELETE_TASK;
    SCH_Exit_Critical(saved_interrupts);
    return 1U;
}

/* Like Program 1.11: execute only when RunMe is set. Searches and periodic
 * re-insertion happen in main, outside SCH_Update().
 */
void SCH_Dispatch_Tasks(void)
{
    uint32_t previous = SCH_Enter_Critical();
    uint32_t index = Head;
    if (index == SCH_NO_INDEX || SCH_tasks_G[index].RunMe == 0U) {
        SCH_Exit_Critical(previous);
        return;
    }

    void (*function_to_run)(void) = SCH_tasks_G[index].pTask;
    uint32_t period = SCH_tasks_G[index].Period;
    SCH_tasks_G[index].RunMe--;
    SCH_Remove_Task(index, SCH_NO_INDEX);

    if (period == 0U) {
        SCH_tasks_G[index] = (sTask){0}; /* One-shot: release the slot. */
    } else {
        /* Repeat from the scheduled release, not callback completion. */
        SCH_Insert_Task(index, period);
    }
    SCH_Prepare_Head(overdue_ticks);
    SCH_Exit_Critical(previous);

    /* Restore interrupt control BEFORE executing application code. */
    (*function_to_run)();
}
