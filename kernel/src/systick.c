/**
* @file systick.c
*
* @brief Implements the systick timer functionality including initialization, delay, tick retrieval, and tick handler.
*
* @date March 20 2025
*
* @author Mario Cruz and Charlie Ai
*/

#include <unistd.h>
#include <systick.h>
#include "syscall_thread.h"
#include <arm.h>

struct sysclock_map{
    volatile uint32_t STK_CTRL;
    volatile uint32_t STK_LOAD;
    volatile uint32_t STK_VAL;
    volatile uint32_t STK_CALIB;
};

#define UNUSED __attribute__((unused))

#define STK_BASE (struct sysclock_map *)  0xE000E010

#define STK_CTRL_EN_COUNT 1

#define STK_CTRL_TICK_EXCEPTION_EN (1 << 1)

#define STK_LOAD_MASK (0xFFFF)

// 16MHz processor clock 15999 cycles to make 1ms
#define STK_TICK_LOAD_VAL (0x3E7F)

#define STK_COUNT_FLAG (1<<16)

#define STK_CLKSOURCE (1<<2)

#define UNUSED __attribute__((unused))


volatile uint32_t total_count;

void systick_init(UNUSED uint32_t frequency) {
    struct sysclock_map * sys = STK_BASE;

    uint32_t reload_val = (16000000 / frequency) - 1;
    sys -> STK_LOAD = reload_val;

    sys -> STK_CTRL = sys -> STK_CTRL | STK_CTRL_TICK_EXCEPTION_EN;
    sys -> STK_CTRL = sys -> STK_CTRL | STK_CTRL_EN_COUNT;
    sys -> STK_CTRL = sys -> STK_CTRL | STK_CLKSOURCE;

    total_count = 0;
}

void systick_delay(UNUSED uint32_t ticks) {
    uint32_t desired_ticks = total_count + ticks;
    while (total_count < desired_ticks){}
    return;
}

uint32_t systick_get_ticks() {
    return total_count;
}

/* Called every millisecond*/
/* Calls pendSV -
    Programming maneual Page 225 talks about the ICSR register bits 28 for 
    setting bit and 27 for clearing from the System Control Block/ nvm thats given to us*/

void systick_c_handler() {
    total_count = total_count + 1;
    pend_pendsv();
}