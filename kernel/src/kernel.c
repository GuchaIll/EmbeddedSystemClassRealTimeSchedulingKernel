#include <stdlib.h>
#include <string.h>

#include <gpio.h>

#include <systick.h>
#include <lcd_driver.h>
#include <keypad_driver.h>


#include <timer.h>
#include <nvic.h>

#include "arm.h"
#include "kernel.h"
#include "timer.h"
#include "i2c.h"
#include "lcd_driver.h"
#include "printk.h"
#include "uart_polling.h"
#include "uart.h"
#include "stdint.h"
#include "syscall.h"
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "arm.h"


#define BAUD_RATE_115200 0x8B

int kernel_main() {
    init_349();  //Do not remove this function
    gpio_init(GPIO_A, 0, MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_HIGH, PUPD_NONE, ALT0);
    gpio_init(GPIO_B, 10, MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_HIGH, PUPD_NONE, ALT0);
    uart_init(0);
    //timer_init(2, 160, 1);
    
    
    enter_user_mode();
    while(1){
    }
    return 0;
}
