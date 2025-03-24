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
    break;
    case 1:
      res_write = sys_write(first_arg, second_arg, third_arg);
      stack -> R0 = res_write;
    break;
    case 2:
    break;
    case 3:
    break;
    case 4:
    break;
    case 5:
    break;
    case 6:
      res_read = sys_read(first_arg, second_arg, third_arg);
      stack -> R0 = res_read;
    break;
    case 7:
      sys_exit(first_arg);
    break;
    case 8:
    break;
    case 9:
    break;
    case 10:
    break;
    case 11:
    break;
    case 12:
    break;
    case 13:
    break;
    case 14:
    break;
    case 15:
    break;
    case 16:
    break;
    case 17:
    break;
    case 18:
    break;
    case 19:
    break;
    case 20:
    break;
    case 21:
    break;
    case 22:
      servo_enable = sys_servo_enable((uint8_t)first_arg, (int)second_arg);
      stack -> R0 = servo_enable;
    break;
    case 23:
      servo_set = sys_servo_set((uint8_t)first_arg,(int)second_arg);
      stack -> R0 = servo_set;
    break;

  default:
    DEBUG_PRINT( "Not implemented, svc num %d\n", svc_number );
    ASSERT( 0 );
  }
}