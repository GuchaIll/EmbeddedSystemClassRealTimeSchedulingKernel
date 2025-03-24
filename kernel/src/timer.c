/**
 * @file timer.c 
 *
 * @brief internal timer interrupts implementation
 *
 * @date 3/20/25
 *
 * @author Mario Cruz and Charlie Ai
 */

#include <unistd.h>
#include <timer.h>
#include <rcc.h>
#include <nvic.h>


#define UNUSED __attribute__((unused))

#define TIM_CR1_CEN (1 << 0)
#define TIM_SR_UIF (1 << 0)
#define TIM_DIER_UIE (1 << 0)
#define TIM_EGR_UG (1 << 0)

#define TIM2_EN (1 << 0)
#define TIM3_EN (1 << 1)
#define TIM4_EN (1 << 2)
#define TIM5_EN (1 << 3)

#define NVIC_TIM2_IRQ 28
#define NVIC_TIM3_IRQ 29
#define NVIC_TIM4_IRQ 30
#define NVIC_TIM5_IRQ 50 //nvic table showing missing TIM4 fix



/** @brief tim2_5 */
struct tim2_5 {
  volatile uint32_t cr1; /**< 00 Control Register 1 */
  volatile uint32_t cr2; /**< 04 Control Register 2 */
  volatile uint32_t smcr; /**< 08 Slave Mode Control */
  volatile uint32_t dier; /**< 0C DMA/Interrupt Enable */
  volatile uint32_t sr; /**< 10 Status Register */
  volatile uint32_t egr; /**< 14 Event Generation */
  volatile uint32_t ccmr[2]; /**< 18-1C Capture/Compare Mode */
  volatile uint32_t ccer; /**< 20 Capture/Compare Enable */
  volatile uint32_t cnt; /**< 24 Counter Register */
  volatile uint32_t psc; /**< 28 Prescaler Register */
  volatile uint32_t arr; /**< 2C Auto-Reload Register */
  volatile uint32_t reserved_1; /**< 30 */
  volatile uint32_t ccr[4]; /**< 34-40 Capture/Compare */
  volatile uint32_t reserved_2; /**< 44 */
  volatile uint32_t dcr; /**< 48 DMA Control Register */
  volatile uint32_t dmar; /**< 4C DMA address for full transfer Register */
  volatile uint32_t or; /**< 50 Option Register */
};



struct tim2_5* const timer_base[] = {(void *)0x0,   // N/A - Don't fill out
                                     (void *)0x0,   // N/A - Don't fill out
                                     (void *)0x40000000,    // TODO: fill out address for TIMER 2
                                     (void *)0x40000400,    // TODO: fill out address for TIMER 3
                                     (void *)0x40000800,    // TODO: fill out address for TIMER 4
                                     (void *)0x40000C00};   // TODO: fill out address for TIMER 5


/*
* Starts the timer
*
* @param timer - The timer
* @param prescaler - Prescalar for clock
* @param Period - Period of the timer interrupt
*/

void timer_init(int timer, uint32_t prescalar, uint32_t period) {
  struct rcc_reg_map *rcc = RCC_BASE;
  rcc->apb1_enr = rcc->apb1_enr| TIM2_EN;

  struct tim2_5 *timerBase = timer_base[timer];
  timerBase->psc = prescalar ;
  timerBase->arr = period ;
  timerBase->dier = timerBase->dier | TIM_DIER_UIE;
  timerBase->egr = timerBase->egr | TIM_EGR_UG;
  timerBase->cr1 = TIM_CR1_CEN;


  
  switch(timer) {
    case 2:
      nvic_irq(NVIC_TIM2_IRQ, IRQ_ENABLE);
      break;
    case 3:
      rcc->apb1_enr = rcc->apb1_enr|  TIM3_EN;
      nvic_irq(NVIC_TIM3_IRQ, IRQ_ENABLE);
      break;
    case 4:
      rcc->apb1_enr = rcc->apb1_enr | TIM4_EN;
      nvic_irq(NVIC_TIM4_IRQ, IRQ_ENABLE);
      break;
    case 5:
      rcc->apb1_enr =rcc->apb1_enr |  TIM5_EN;
      nvic_irq(NVIC_TIM5_IRQ, IRQ_ENABLE);
      break;
  }
  
  
}

/*
 * timer_disable: disable the specified timer.
 */
void timer_disable(UNUSED int timer) {

  if(timer < 2 || timer > 5) {
    return;
  }

  struct tim2_5 *timerBase = timer_base[timer];
  timerBase->cr1 &= ~TIM_CR1_CEN;

  struct rcc_reg_map *rcc = RCC_BASE;
  switch(timer) {
    case 2:
      rcc->apb1_enr = rcc->apb1_enr & ~TIM2_EN;
      break;
    case 3:
      rcc->apb1_enr = rcc->apb1_enr & ~TIM3_EN;
      break;
    case 4:
      rcc->apb1_enr = rcc->apb1_enr & ~TIM4_EN;
      break;
    case 5:
      rcc->apb1_enr =  rcc->apb1_enr & ~TIM5_EN;
      break;
  }
  
}

/*
 * time r_clear_interrupt_bit: clear the update interrupt flag for the specified timer.
 */
void timer_clear_interrupt_bit(UNUSED int timer) {
  struct tim2_5 *timerBase = timer_base[timer];
  timerBase->sr = timerBase->sr & ~TIM_SR_UIF;
}