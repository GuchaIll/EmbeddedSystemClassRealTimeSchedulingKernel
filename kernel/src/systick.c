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
#include <arm.h>

/** @brief Defining the sysclock register map*/
struct sysclock_map{
    volatile uint32_t STK_CTRL;
    volatile uint32_t STK_LOAD;
    volatile uint32_t STK_VAL;
    volatile uint32_t STK_CALIB;
};

#define STK_BASE (struct sysclock_map *)  0xE000E010

#define STK_CTRL_EN_COUNT 1

#define STK_CTRL_TICK_EXCEPTION_EN (1 << 1)

#define STK_LOAD_MASK (0xFFFF)

// 16MHz processor clock 15999 cycles to make 1ms
#define STK_TICK_LOAD_VAL (0x3E7F)

// 16MHz processor clock 1599999 cycles to make 1s frequency
#define STK_TICK_NEW_LOAD_VAL (0xF423FF)

#define STK_COUNT_FLAG (1<<16)

#define STK_CLKSOURCE (1<<2)

#define UNUSED __attribute__((unused))


volatile uint32_t total_count;


void systick_init(UNUSED uint32_t frequency) {
    struct sysclock_map * sys = STK_BASE;
    
    u_int16_t loadval;
    /* Since 15999 is the number of cycles for 1 ms frequency timer 
        we could  to convert this to a 1 second frequency for easier #s
        therefore 16000 * 1000 - 1 = 15999999 load value. Since frequency
        per seconds. We can just divide our new load value by the frequency
        input to achieve our desired value*/

    loadval = STK_TICK_NEW_LOAD_VAL/frequency;

    sys -> STK_LOAD = (loadval);

    sys -> STK_CTRL = sys -> STK_CTRL | STK_CTRL_TICK_EXCEPTION_EN;
    sys -> STK_CTRL = sys -> STK_CTRL | STK_CTRL_EN_COUNT;
    sys -> STK_CTRL = sys -> STK_CTRL | STK_CLKSOURCE;

    total_count = 0;
}

/* When called, should delay the program this many ticks milliseconds*/
void systick_delay(uint32_t ticks) {
    uint32_t desired_ticks = total_count + ticks;
    while (total_count < desired_ticks){}
    return;
}
/* Grabs number of ticks elapsed (number of times systick_c_handler called since boot)*/
uint32_t systick_get_ticks(){
    return total_count;
}

/* Called every millisecond*/
/* Calls pendSV -
    Programming maneual Page 225 talks about the ICSR register bits 28 for 
    setting bit and 27 for clearing from the System Control Block/ nvm thats given to us*/

void systick_c_handler() {
    pend_pendsv();
    total_count += 1;
}