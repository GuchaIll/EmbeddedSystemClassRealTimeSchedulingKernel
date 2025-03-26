/**
 * @file svc_handler.c
 *
 * @brief provides a handler for SVC interrupts, decoding the SVC number and dispatching system calls.    
 *
 * @date  March 20 2025     
 *
 * @author Mario Cruz and Charlie Ai
 */

#include <stdint.h>
#include <debug.h>
#include <syscall.h>
#include <syscall_thread.h>

#define UNUSED __attribute__((unused))

struct stack_frame_map {
    volatile uint32_t R0;  
    volatile uint32_t R1;   
    volatile uint32_t R2;  
    volatile uint32_t R3;  
    volatile uint32_t R12;  
    volatile uint32_t LR;  
    volatile uint32_t PC; 
    volatile uint32_t PSR;
};

/*
 * svc_c_handler: process the SVC interrupt, extract the SVC number and arguments, and dispatch the appropriate system call.
 */
void svc_c_handler( uint32_t * stack_p ) {
  set_svc_status(1);
  struct stack_frame_map * stack = (struct stack_frame_map *) stack_p;
  /* Referencing Program Counter */
  uint32_t * pc_pointer = (uint32_t*)(stack -> PC);
  /* Referencing Old SVC Instruction Underneath Address At PC */
  uint32_t svc_instruction = *(((uint16_t*)pc_pointer) - 1);
  /* Extracting Lower 8 Bits Which Are SVC Number*/
  uint32_t svc_number = (svc_instruction & 0xFF);
  int first_arg = (stack -> R0);
  char * second_arg = (char*)(stack -> R1);
  int third_arg = (stack -> R2);
  int fourth_arg = (stack -> R3);
  
  /* Theoretically one block above PSR */
  int fifth_arg = (stack -> PSR + 1);

  int res_write;
  void* res_sbrk;
  int res_read;
  int servo_enable;
  int servo_set;
  switch ( svc_number ) {
    case 0:
      res_sbrk = sys_sbrk(first_arg);
      stack -> R0 = (uint32_t)res_sbrk;
      set_svc_status(0);
    break;
    case 1:
      res_write = sys_write(first_arg, second_arg, third_arg);
      stack -> R0 = res_write;
      set_svc_status(0);
    break;
    case 2:
      stack -> R0 = 1;
      set_svc_status(0);

    break;
    case 3:
      stack -> R0 = 1;
      set_svc_status(0);
    break;
    case 4:
      stack -> R0 = 1;
      set_svc_status(0);
    break;
    case 5:
      stack -> R0 = 1;
      set_svc_status(0);
    break;
    case 6:
      res_read = sys_read(first_arg, second_arg, third_arg);
      stack -> R0 = res_read;
      set_svc_status(0);
    break;
    case 7:
      sys_exit(first_arg);
      set_svc_status(0);
    break;
    case 9:
      stack -> R0 = (uint32_t)sys_thread_init(first_arg, second_arg, (void*)third_arg, fourth_arg);
      set_svc_status(0);
    break;
    case 10:
      stack -> R0 = (uint32_t)sys_thread_create((void*)first_arg, (uint32_t)second_arg, third_arg, fourth_arg, (void*)fifth_arg);
      set_svc_status(0);
    break;
    case 11:
      sys_thread_kill();
      set_svc_status(0);
    break;
    case 12:
      stack -> R0 = sys_scheduler_start(first_arg);
      set_svc_status(0);
    break;
    case 13:
      stack -> R0 = sys_mutex_init();
      set_svc_status(0);
    break;
    case 14:
      stack -> R0 = sys_mutex_lock();
      set_svc_status(0);
    break;
    case 15:
      stack -> R0 = sys_mutex_unlock();
      set_svc_status(0);
    break;
    case 16:
      sys_wait_until_next_period();
      set_svc_status(0);
    break;
    case 17:
      stack -> R0 = sys_get_time();
      set_svc_status(0);
    break;
    case 19:
      stack -> R0 = sys_get_priority();
      set_svc_status(0);
    break;
    case 20:
      stack -> R0 = sys_thread_time();
      set_svc_status(0);
    break;
    case 22:
      servo_enable = sys_servo_enable((uint8_t)first_arg, (int)second_arg);
      stack -> R0 = servo_enable;
      set_svc_status(0);
    break;
    case 23:
      servo_set = sys_servo_set((uint8_t)first_arg,(int)second_arg);
      stack -> R0 = servo_set;
      set_svc_status(0);
    break;

  default:
    DEBUG_PRINT( "Not implemented, svc num %d\n", svc_number );
    ASSERT( 0 );
    set_svc_status(0);
  }
}