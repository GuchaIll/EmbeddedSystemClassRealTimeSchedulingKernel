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

/**
 * @brief Attribute to mark unused function parameters.
 */
#define UNUSED __attribute__((unused))


/**
 * @brief Array to track if the servos are enabled.
 *
 * Each index corresponds to a servo channel (0 or 1).
 */
volatile uint8_t servo_enabled[2] = {0, 0};

/**
 * @brief Array to store the pulse width for each servo.
 *
 * Each index corresponds to a servo channel (0 or 1).
 * The pulse width is calculated based on the desired angle.
 */
volatile uint32_t servo_pulse_width[2] = {0, 0};

/**
 * @brief GPIO pin for PWM signal to servo 1.
 */
#define PWM_PIN1 "B3"

/**
 * @brief GPIO pin for PWM signal to servo 2.
 */
#define PWM_PIN2 "B10"


/**
 * @brief Enables or disables a servo.
 *
 * This function updates the enable status of the specified servo channel.
 * If the servo is disabled, the corresponding GPIO pin is cleared.
 *
 * @param[in] channel The servo channel (0 or 1).
 * @param[in] enabled 1 to enable the servo, 0 to disable it.
 * @return 0 on success, -1 if the channel is invalid.
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

/**
 * @brief Sets the servo pulse width based on the desired angle.
 *
 * This function calculates the pulse width for the specified servo channel
 * based on the desired angle (0 to 180 degrees). The pulse width is stored
 * in the `servo_pulse_width` array.
 *
 * @param[in] channel The servo channel (0 or 1).
 * @param[in] angle The desired angle (0 to 180 degrees).
 * @return 0 on success, -1 if the channel or angle is invalid.
 */
int sys_servo_set(UNUSED uint8_t channel, UNUSED uint8_t angle){
  if(channel > 1 || angle > 180){
    return -1;
  }
  // timer_interval = 0;

  servo_pulse_width[channel] = (uint32_t)((0.6 + (angle/ 180.0) * 1.8) * 100.0);
  
  return 0;
}

/**
 * @brief Timer interval counter for generating PWM signals.
 *
 * This variable is incremented in the timer interrupt handler and is used
 * to determine when to set or clear the GPIO pins for PWM generation.
 */
volatile uint32_t timer_interval = 0;

/**
 * @brief Timer interrupt handler for generating PWM signals to control servos.
 *
 * This function is called when the timer interrupt occurs. It generates PWM
 * signals by setting and clearing the GPIO pins for the servos based on the
 * current timer interval and the pulse width for each servo.
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