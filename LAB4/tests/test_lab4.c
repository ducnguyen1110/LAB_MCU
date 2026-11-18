#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Firmware/Core/Src/scheduler.c"

/* Minimal HAL stand-ins. The app code is extracted unchanged from main.c. */
typedef struct { uint32_t DR; } UART_Registers;
static UART_Registers uart_registers;
static struct { UART_Registers *Instance; } huart1 = {&uart_registers};
static int htim2;
static unsigned int tx_ready = 1;
static unsigned int led_count[5];
static uint32_t led_levels;

#define GPIOA ((void *)0)
#define GPIO_PIN_1 (1U << 1)
#define GPIO_PIN_2 (1U << 2)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_SET 1U
#define RESET 0U
#define UART_FLAG_TXE 0U
#define TIM_FLAG_UPDATE 0U
#define HAL_OK 0
#define __HAL_UART_GET_FLAG(handle, flag) ((void)(handle), (void)(flag), tx_ready)
#define __HAL_TIM_SET_COUNTER(handle, value) ((void)(handle), (void)(value))
#define __HAL_TIM_CLEAR_FLAG(handle, flag) ((void)(handle), (void)(flag))

static int HAL_TIM_Base_Start_IT(void *handle)
{
    (void)handle;
    return HAL_OK;
}

static void HAL_GPIO_TogglePin(void *port, uint32_t pin)
{
    (void)port;
    for (unsigned int i = 0; i < 5; i++) {
        if (pin == (1U << (i + 1))) {
            led_count[i]++;
            led_levels ^= pin;
            return;
        }
    }
    assert(!"Unexpected GPIO pin");
}

static unsigned int HAL_GPIO_ReadPin(void *port, uint32_t pin)
{
    (void)port;
    return (led_levels & pin) != 0U;
}

static void Error_Handler(void)
{
    fputs("Unexpected Error_Handler call\n", stderr);
    abort();
}

#include "app_sections.h"

static unsigned int calls;
static char order[64];
static unsigned int order_length;
static uint32_t self_id;

static void count_call(void) { calls++; }
static void record_a(void) { order[order_length++] = 'A'; }
static void record_b(void) { order[order_length++] = 'B'; }
static void record_c(void) { order[order_length++] = 'C'; }
static void delete_self(void)
{
    calls++;
    assert(SCH_Delete_Task(self_id) == 0);
}

static void advance(unsigned int ticks)
{
    while (ticks-- > 0) { SCH_Update(); }
}

static void test_scheduler(void)
{
    SCH_Init();
    calls = 0;
    assert(get_time() == 0);
    assert(SCH_Add_Task(count_call, 50, 50) != 0);
    advance(49);
    SCH_Dispatch_Tasks();
    assert(calls == 0);
    advance(1);
    SCH_Dispatch_Tasks();
    assert(calls == 1 && get_time() == 500);
    advance(49);
    SCH_Dispatch_Tasks();
    assert(calls == 1);
    advance(1);
    SCH_Dispatch_Tasks();
    assert(calls == 2 && get_time() == 1000);

    SCH_Init();
    order_length = 0;
    assert(SCH_Add_Task(record_a, 21, 0));
    assert(SCH_Add_Task(record_b, 7, 0));
    assert(SCH_Add_Task(record_c, 13, 0));
    advance(22); /* All overdue: select by deadline, not array order. */
    for (int i = 0; i < 10; i++) { SCH_Dispatch_Tasks(); }
    assert(order_length == 3 && memcmp(order, "BCA", 3) == 0);

    SCH_Init();
    order_length = 0;
    uint32_t old_id = SCH_Add_Task(record_a, 1, 0);
    assert(SCH_Add_Task(record_b, 1, 0));
    assert(SCH_Delete_Task(old_id) == 0);
    uint32_t new_id = SCH_Add_Task(record_c, 1, 0);
    assert(new_id != old_id);
    assert(SCH_Delete_Task(old_id) == 1);
    advance(1);
    SCH_Dispatch_Tasks();
    SCH_Dispatch_Tasks();
    assert(order_length == 2 && memcmp(order, "BC", 2) == 0);

    SCH_Init();
    calls = 0;
    self_id = SCH_Add_Task(delete_self, 0, 1);
    SCH_Dispatch_Tasks();
    advance(10);
    SCH_Dispatch_Tasks();
    assert(calls == 1);

    SCH_Init();
    calls = 0;
    assert(SCH_Add_Task(count_call, 3, 3));
    advance(10);
    for (int i = 0; i < 10; i++) { SCH_Dispatch_Tasks(); }
    assert(calls == 3);
    advance(2);
    SCH_Dispatch_Tasks();
    assert(calls == 4);

    SCH_Init();
    calls = 0;
    tick_count = UINT32_MAX - 2U; /* Inject rollover instead of waiting days. */
    assert(SCH_Add_Task(count_call, 5, 0));
    advance(4);
    SCH_Dispatch_Tasks();
    assert(calls == 0);
    advance(1);
    SCH_Dispatch_Tasks();
    assert(calls == 1);

    SCH_Init();
    assert(SCH_Add_Task(NULL, 0, 1) == 0);
    assert(SCH_Add_Task(count_call, 0x80000000U, 1) == 0);
    assert(SCH_Add_Task(count_call, 0, 0x80000000U) == 0);
    assert(SCH_Delete_Task(0) == 1);
    old_id = SCH_Add_Task(count_call, 1, 0);
    for (unsigned int i = 1; i < SCH_MAX_TASKS; i++) {
        assert(SCH_Add_Task(count_call, 1, 0));
    }
    assert(SCH_Add_Task(count_call, 1, 0) == 0);
    assert(SCH_Delete_Task(old_id) == 0);
    assert(SCH_Add_Task(count_call, 1, 0) != old_id);
    puts("PASS: scheduler timing, ordering, deletion, catch-up, rollover and capacity");
}

/* Check the handout's Delay/RunMe lifecycle, not just callback output. */
static void test_delay_and_runme(void)
{
    SCH_Init();
    calls = 0;
    assert(SCH_Add_Task(count_call, 2, 3));
    assert(SCH_tasks_G[Head].Delay == 2 && SCH_tasks_G[Head].RunMe == 0);
    advance(1);
    assert(SCH_tasks_G[Head].Delay == 1 && SCH_tasks_G[Head].RunMe == 0);
    advance(1);
    assert(SCH_tasks_G[Head].Delay == 0 && SCH_tasks_G[Head].RunMe == 1);
    assert(calls == 0); /* Update must NEVER call application code. */
    SCH_Dispatch_Tasks();
    assert(calls == 1);
    assert(SCH_tasks_G[Head].Delay == 3 && SCH_tasks_G[Head].RunMe == 0);

    SCH_Init();
    assert(SCH_Add_Task(count_call, 50, 0));
    assert(SCH_Add_Task(count_call, 100, 0));
    assert(SCH_Add_Task(count_call, 150, 0));
    uint32_t index = Head;
    for (unsigned int i = 0; i < 3; i++) {
        assert(SCH_tasks_G[index].Delay == 50);
        index = Next[index];
    }
    assert(index == SCH_NO_INDEX);

    SCH_Init();
    calls = 0;
    for (unsigned int i = 0; i < SCH_MAX_TASKS; i++) {
        assert(SCH_Add_Task(count_call, 1, 0));
    }
    advance(10); /* All 40 become overdue while main is not dispatching. */
    for (unsigned int i = 0; i < SCH_MAX_TASKS; i++) {
        SCH_Dispatch_Tasks();
    }
    assert(calls == SCH_MAX_TASKS && Head == SCH_NO_INDEX);
    assert(Error_code_G == ERROR_SCH_NONE);
    assert(SCH_Delete_Task(0) == 1);
    assert(Error_code_G == ERROR_SCH_CANNOT_DELETE_TASK);
    puts("PASS: Delay countdown, RunMe flag, delta storage and 40 simultaneous releases");
}

typedef struct {
    uint32_t id;
    uint32_t release;
    uint32_t period;
    unsigned int tag;
} ModelTask;

static ModelTask reference[SCH_MAX_TASKS];
static uint32_t model_now;
static uint32_t random_state;

static uint32_t random_value(void)
{
    random_state = random_state * 1664525U + 1013904223U;
    return random_state;
}

/* Compare every queue node with an independent absolute-time model. */
static void check_delta_queue(void)
{
    unsigned int seen[SCH_MAX_TASKS] = {0};
    unsigned int count = 0;
    uint32_t release = model_now - overdue_ticks;
    uint32_t index = Head;
    uint32_t previous_id = 0;
    uint32_t previous_release = 0;

    assert(tick_count == model_now);
    while (index != SCH_NO_INDEX) {
        assert(index < SCH_MAX_TASKS && count++ < SCH_MAX_TASKS);
        release += SCH_tasks_G[index].Delay;
        unsigned int slot = 0;
        while (slot < SCH_MAX_TASKS && reference[slot].id != SCH_tasks_G[index].TaskID) {
            slot++;
        }
        assert(slot < SCH_MAX_TASKS && !seen[slot]);
        seen[slot] = 1;
        assert(reference[slot].release == release);
        assert(reference[slot].period == SCH_tasks_G[index].Period);
        if (index == Head) {
            assert(SCH_tasks_G[index].RunMe == (release <= model_now));
        } else {
            assert(SCH_tasks_G[index].RunMe == 0);
            assert(release > previous_release ||
                   (release == previous_release && SCH_tasks_G[index].TaskID > previous_id));
        }
        previous_release = release;
        previous_id = SCH_tasks_G[index].TaskID;
        index = Next[index];
    }
    for (unsigned int i = 0; i < SCH_MAX_TASKS; i++) {
        assert(seen[i] == (reference[i].id != 0));
    }
}

static void test_random_operations(void)
{
    void (*functions[])(void) = {record_a, record_b, record_c};
    for (unsigned int seed = 1; seed <= 6; seed++) {
        SCH_Init();
        memset(reference, 0, sizeof(reference));
        model_now = 0;
        uint32_t next_id = 1;
        random_state = seed;
        for (unsigned int step = 0; step < 5000; step++) {
            uint32_t operation = random_value() % 10U;
            if (operation < 3) {
                uint32_t delay = random_value() % 40U;
                uint32_t period = random_value() % 26U;
                unsigned int tag = random_value() % 3U;
                unsigned int slot = 0;
                while (slot < SCH_MAX_TASKS && reference[slot].id != 0) { slot++; }
                uint32_t actual_id = SCH_Add_Task(functions[tag], delay, period);
                if (slot == SCH_MAX_TASKS) {
                    assert(actual_id == 0);
                } else {
                    assert(actual_id == next_id);
                    reference[slot] = (ModelTask){next_id++, model_now + delay, period, tag};
                }
            } else if (operation < 5) {
                unsigned int slot = random_value() % SCH_MAX_TASKS;
                uint32_t id = reference[slot].id;
                assert(SCH_Delete_Task(id) == (id == 0));
                reference[slot].id = 0;
            } else if (operation < 8) {
                unsigned int ticks = random_value() % 8U + 1U;
                advance(ticks);
                model_now += ticks;
            } else {
                int selected = -1;
                for (unsigned int i = 0; i < SCH_MAX_TASKS; i++) {
                    if (reference[i].id == 0 || reference[i].release > model_now) { continue; }
                    if (selected < 0 || reference[i].release < reference[selected].release ||
                        (reference[i].release == reference[selected].release &&
                         reference[i].id < reference[selected].id)) {
                        selected = (int)i;
                    }
                }
                order_length = 0;
                SCH_Dispatch_Tasks();
                if (selected < 0) {
                    assert(order_length == 0);
                } else {
                    assert(order_length == 1 && order[0] == (char)('A' + reference[selected].tag));
                    if (reference[selected].period == 0) {
                        reference[selected].id = 0;
                    } else {
                        reference[selected].release += reference[selected].period;
                    }
                }
            }
            check_delta_queue();
        }
    }
    puts("PASS: 30000 randomized add/delete/tick/dispatch operations against reference model");
}

static char serial_output[16384];
static unsigned int serial_length;

static void drain_uart(void)
{
    while (uart_tx_tail != uart_tx_head) {
        UART_Poll();
        assert(serial_length + 1 < sizeof(serial_output));
        serial_output[serial_length++] = (char)uart_registers.DR;
    }
    serial_output[serial_length] = '\0';
}

static void test_demo(void)
{
    uart_tx_head = uart_tx_tail = 0;
    demo_setup();
    uint32_t tail_before = uart_tx_tail;
    tx_ready = 0;
    UART_Poll();
    assert(uart_tx_tail == tail_before);
    tx_ready = 1;
    drain_uart();

    for (unsigned int tick = 0; tick < 1000; tick++) {
        SCH_Update();
        for (unsigned int ready = 0; ready < SCH_MAX_TASKS; ready++) {
            SCH_Dispatch_Tasks();
            drain_uart();
        }
        assert(last_tick_callback_ms == (tick + 1U) * 10U);
    }

    assert(led_count[0] == 20 && led_count[1] == 10);
    assert(led_count[2] == 6 && led_count[3] == 5 && led_count[4] == 4);
    char *first = strstr(serial_output, "ONESHOT 70ms t=70 ms");
    char *second = strstr(serial_output, "ONESHOT 130ms t=130 ms");
    char *third = strstr(serial_output, "ONESHOT 210ms t=210 ms");
    assert(first && second && third && first < second && second < third);
    assert(strstr(first + 1, "ONESHOT 70ms") == NULL);
    assert(strstr(second + 1, "ONESHOT 130ms") == NULL);
    assert(strstr(third + 1, "ONESHOT 210ms") == NULL);
    assert(strstr(serial_output, "DELETE one-shot result=0 t=600 ms"));
    assert(strstr(serial_output, "ERROR") == NULL);
    assert(strstr(serial_output, "TICK t=") == NULL);
    for (unsigned int i = 0; i < 5; i++) {
        for (unsigned int edge = 1; edge <= led_count[i]; edge++) {
            char line[64];
            snprintf(line, sizeof(line), "LED%u %s t=%u ms\r\n", i + 1U,
                     edge % 2U ? "ON" : "OFF", edge * (i + 1U) * 500U);
            assert(strstr(serial_output, line) != NULL);
        }
    }
    puts("PASS: 10-second demo: 5 LEDs log their state after each toggle, silent 10 ms callback,");
    puts("      ordered one-shots, delete demo and nonblocking UART poll");
}

int main(void)
{
    test_scheduler();
    test_delay_and_runme();
    test_random_operations();
    test_demo();
    return 0;
}
