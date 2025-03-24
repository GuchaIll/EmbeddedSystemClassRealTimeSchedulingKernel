/**
 * @file servok.c
 *
 * @brief Implements servo control system calls and a timer interrupt handler for PWM servo control    
 *
 * @date March 20 2025  
 *
 * @author Mario Cruz and Charlie Ai 
 */

#include <unistd.h>
#include <syscall.h>
#include "gpio.h"
#include "timer.h"

#define UNUSED __attribute__((unused))

//array to tracking if the servo is enabled
volatile uint8_t servo_enabled[2] = {0, 0};
volatile uint32_t servo_pulse_width[2] = {0, 0};

#define PWM_PIN1 "B3"
#define PWM_PIN2 "B10"



/*
 * sys_servo_enable: update the servo enable status and clear the corresponding GPIO pin when disabled.
 */
int sys_servo_enable(UNUSED uint8_t channel, UNUSED uint8_t enabled){
  if(channel > 1){
    return -1;
  }
  servo_enabled[channel] = enabled;
  if(!enabled){

    switch (channel){
      case 0:
        //clear gpio pins
        gpio_clr(GPIO_A, 0);
       
        break;
      case 1:     
        //clear gpio pin
        gpio_clr(GPIO_B, 10);

        break;
  
  }
  
}
return 0;
}

/*
 * sys_servo_set: set the servo pulse width based on the desired angle.
 */
int sys_servo_set(UNUSED uint8_t channel, UNUSED uint8_t angle){
  if(channel > 1 || angle > 180){
    return -1;
  }
  // timer_interval = 0;

  servo_pulse_width[channel] = (uint32_t)((0.6 + (angle/ 180.0) * 1.8) * 100.0);
  
  return 0;
}
volatile uint32_t timer_interval = 0;

/*
 * TIMER2_SERVO_IRQHandler: handle the timer2 interrupt for generating PWM signals to control servos.
 */
void TIMER2_SERVO_IRQHandler(){

  timer_clear_interrupt_bit(2);
  
  if(servo_enabled[0])
  {
      if(timer_interval == 0)
      {
          gpio_set(GPIO_A, 0);
      }
      else if(timer_interval == servo_pulse_width[0]/2)
      {
          gpio_clr(GPIO_A, 0);
      }
  }

  if(servo_enabled[1])
  {
      if(timer_interval == 0)
      {
          gpio_set(GPIO_B, 10);
      }
      else if(timer_interval == servo_pulse_width[1]/2)
      {
          gpio_clr(GPIO_B, 10);
      }
  }
  timer_interval++;
  if(timer_interval >= 1000)
  {
    timer_interval = 0;
  }
  
  return;
}