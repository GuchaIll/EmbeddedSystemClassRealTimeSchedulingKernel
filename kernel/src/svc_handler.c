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
#include <servok.h>

/**
 * @brief Attribute to mark unused function parameters.
 */
#define UNUSED __attribute__((unused))

/**
 * @struct stack_frame_map
 * @brief Represents the stack frame layout during an SVC interrupt.
 *
 * This structure maps the stack frame pushed onto the stack during an SVC interrupt,
 * including general-purpose registers, the program counter, and the processor status register.
 */
struct stack_frame_map {
    volatile uint32_t R0;         /**< General-purpose register R0. */
    volatile uint32_t R1;         /**< General-purpose register R1. */
    volatile uint32_t R2;         /**< General-purpose register R2. */
    volatile uint32_t R3;         /**< General-purpose register R3. */
    volatile uint32_t R12;        /**< General-purpose register R12. */
    volatile uint32_t LR;         /**< Link register (LR). */
    volatile uint32_t PC;         /**< Program counter (PC). */
    volatile uint32_t PSR;        /**< Program status register (PSR). */
    volatile uint32_t fifth_arg;   /**< Fifth argument passed to the SVC handler. */
};

/**
 * @brief Handles the SVC interrupt.
 *
 * This function processes the SVC interrupt by extracting the SVC number and arguments
 * from the stack frame and dispatching the appropriate system call.
 *
 * @param[in] stack_p Pointer to the stack frame at the time of the SVC interrupt.
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
  uint32_t first_arg = (stack -> R0);
  uint32_t second_arg = (stack -> R1);
  uint32_t third_arg = (stack -> R2);
  uint32_t fourth_arg = (stack -> R3);
  
  /* Theoretically one block above PSR */
  uint32_t fifth_arg = (stack -> fifth_arg);

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
      res_write = sys_write(first_arg, (char*)second_arg, third_arg);
      stack -> R0 = res_write;
    break;
    case 2:
      stack -> R0 = 1;

    break;
    case 3:
      stack -> R0 = 1;
    break;
    case 4:
      stack -> R0 = 1;
    break;
    case 5:
      stack -> R0 = 1;
    break;
    case 6:
      res_read = sys_read(first_arg, (char*)second_arg, third_arg);
      stack -> R0 = res_read;
    break;
    case 7:
      sys_exit(first_arg);
    break;
    case 9:
      stack -> R0 = (uint32_t)sys_thread_init(first_arg, (uint32_t)second_arg, (void*)third_arg, fourth_arg);
    break;
    case 10:
      stack -> R0 = (uint32_t)sys_thread_create((void*)first_arg, (uint32_t)second_arg, third_arg, fourth_arg, (void*)fifth_arg);
    break;
    case 11:
      sys_thread_kill();
    break;
    case 12:
      stack -> R0 = sys_scheduler_start(first_arg);
    break;
    case 13:
      //stack -> R0 = sys_mutex_init();
    break;
    case 14:
      //stack -> R0 = sys_mutex_lock();
    break;
    case 15:
      //stack -> R0 = sys_mutex_unlock();
    break;
    case 16:
      sys_wait_until_next_period();
    break;
    case 17:
      stack -> R0 = sys_get_time();
    break;
    case 19:
      stack -> R0 = sys_get_priority();
    break;
    case 20:
      stack -> R0 = sys_thread_time();
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