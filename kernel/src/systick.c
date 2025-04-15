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

/**
 * @struct sysclock_map
 * @brief Represents the memory-mapped registers of the SysTick timer.
 */
struct sysclock_map{
    volatile uint32_t STK_CTRL; /**< SysTick control and status register. */
    volatile uint32_t STK_LOAD; /**< SysTick reload value register. */
    volatile uint32_t STK_VAL;  /**< SysTick current value register. */
    volatile uint32_t STK_CALIB; /**< SysTick calibration value register. */
};

/**
 * @brief Attribute to mark unused function parameters.
 */
#define UNUSED __attribute__((unused))

/**
 * @brief Base address of the SysTick timer registers.
 */
#define STK_BASE (struct sysclock_map *)  0xE000E010

/**
 * @brief Enables the SysTick counter.
 */
#define STK_CTRL_EN_COUNT 1

/**
 * @brief Enables SysTick exception generation on timer expiration.
 */
#define STK_CTRL_TICK_EXCEPTION_EN (1 << 1)

/**
 * @brief Mask for the SysTick reload value.
 */
#define STK_LOAD_MASK (0xFFFF)

// 16MHz processor clock 15999 cycles to make 1ms
/**
 * @brief Default reload value for a 1ms tick (16MHz clock).
 */
#define STK_TICK_LOAD_VAL (0x3E7F)

/**
 * @brief Flag indicating that the SysTick counter has reached 0.
 */
#define STK_COUNT_FLAG (1<<16)

/**
 * @brief Selects the processor clock as the SysTick clock source.
 */
#define STK_CLKSOURCE (1<<2)


/**
 * @brief Total number of ticks since the SysTick timer was initialized.
 */
volatile uint32_t total_count;

/**
 * @brief Initializes the SysTick timer.
 *
 * Configures the SysTick timer with the specified frequency, enabling the counter,
 * exception generation, and selecting the processor clock as the clock source.
 *
 * @param[in] frequency The desired frequency of the SysTick timer in Hz.
 */
void systick_init(UNUSED uint32_t frequency) {
    struct sysclock_map * sys = STK_BASE;

    uint32_t reload_val = (16000000 / frequency) - 1;
    sys -> STK_LOAD = reload_val;

    sys -> STK_CTRL = sys -> STK_CTRL | STK_CTRL_TICK_EXCEPTION_EN;
    sys -> STK_CTRL = sys -> STK_CTRL | STK_CTRL_EN_COUNT;
    sys -> STK_CTRL = sys -> STK_CTRL | STK_CLKSOURCE;

    total_count = 0;
}

/**
 * @brief Creates a delay using the SysTick timer.
 *
 * This function blocks execution until the specified number of ticks has elapsed.
 *
 * @param[in] ticks The number of ticks to delay.
 */
void systick_delay(UNUSED uint32_t ticks) {
    uint32_t desired_ticks = total_count + ticks;
    while (total_count < desired_ticks){}
    return;
}

/**
 * @brief Retrieves the total number of ticks since the SysTick timer was initialized.
 *
 * @return The total number of ticks.
 */
uint32_t systick_get_ticks() {
    return total_count;
}

/**
 * @brief Flag indicating whether a SysTick interrupt has occurred.
 */
volatile int sysTickFlag = 0;

/**
 * @brief Clears the SysTick interrupt flag.
 *
 * This function resets the `sysTickFlag` to 0, indicating that the SysTick interrupt
 * has been handled.
 */
void clear_systick_flag() {
    sysTickFlag = 0;
}
/* Called every millisecond*/
/* Calls pendSV -
    Programming maneual Page 225 talks about the ICSR register bits 28 for 
    setting bit and 27 for clearing from the System Control Block/ nvm thats given to us*/
