/** @file   syscall_thread.c
 *
 *  @brief  sys calls for thread creation and initialization
 *
 *  @date   3/24/25
 *
 *  @author Mario Cruz
 */

#include <stdint.h>
#include "syscall_thread.h"
#include "syscall_mutex.h"

/** @brief      Initial XPSR value, all 0s except thumb bit. */
#define XPSR_INIT 0x1000000

/** @brief Interrupt return code to user mode using PSP.*/
#define LR_RETURN_TO_USER_PSP 0xFFFFFFFD
/** @brief Interrupt return code to kernel mode using MSP.*/
#define LR_RETURN_TO_KERNEL_MSP 0xFFFFFFF1

/**
 * @brief      Heap high and low pointers.
 */
//@{
extern char
  __thread_u_stacks_low,
  __thread_u_stacks_top,
  __thread_k_stacks_low,
  __thread_k_stacks_top;
//@}

/**
 * @brief      Precalculated values for UB test.
 */
float ub_table[] = {
  0.000, 1.000, .8284, .7798, .7568,
  .7435, .7348, .7286, .7241, .7205,
  .7177, .7155, .7136, .7119, .7106,
  .7094, .7083, .7075, .7066, .7059,
  .7052, .7047, .7042, .7037, .7033,
  .7028, .7025, .7021, .7018, .7015,
  .7012, .7009
};

/**
 * @struct user_stack_frame
 *
 * @brief  Stack frame upon exception.
 */
typedef struct {
  uint32_t r0;   /** @brief Register value for r0 */
  uint32_t r1;   /** @brief Register value for r1 */
  uint32_t r2;   /** @brief Register value for r2 */
  uint32_t r3;   /** @brief Register value for r3 */
  uint32_t r12;  /** @brief Register value for r12 */
  uint32_t lr;   /** @brief Register value for lr*/
  uint32_t pc;   /** @brief Register value for pc */
  uint32_t xPSR; /** @brief Register value for xPSR */
} interrupt_stack_frame;

/**
 * @struct tcb_info_frame
 *
 * @brief  Thread Control block info frame.
 */
typedef struct {
  uint32_t psp;
  uint32_t r0;
  uint32_t r4;  /** @brief Register value for r12 */
  uint32_t r5;   /** @brief Register value for r0 */
  uint32_t r6;   /** @brief Register value for r1 */
  uint32_t r7;   /** @brief Register value for r2 */
  uint32_t r8;   /** @brief Register value for r3 */
  uint32_t r9;  /** @brief Register value for r12 */
  uint32_t r10;   /** @brief Register value for r0 */
  uint32_t r11;   /** @brief Register value for r1 */
  uint32_t lr;   /** @brief Register value for lr*/
  uint32_t COMPUTE_TIME; /** @brief Worst case compute time for task*/
  uint32_t PERIOD_TIME; /** @brief How often the thread needs to be run (Deadline = Period)*/
  uint32_t PRIORITY; /** @brief Unique static priority of task (lower equates to higher priority)*/
  uint32_t STATUS; /** @brief Task status (Running, Runnable, Waiting, and potentiall Cancelled*/
} tcb_info_frame;


static tcb_info_frame* TCB_Array[16];

static int max_size_TCB;

static int curr_size_TCB = 0;

/* Make the assumption that C cannot be 0 */
int ub_test(){
  float utilization = 0;
  int C, T;
  for (int index = 0; index < curr_size_TCB; index ++){
    C = TCB_Array[index] -> COMPUTE_TIME;
    if (C == 0){continue;} // Uninitialized Thread Index in TCB Array
    T = TCB_Array[index] -> PERIOD_TIME;
    utilization += (float)C/T;
  }
  if (utilization <= ub_table[curr_size_TCB]){
    return 0;
  }
  return -1;
}

void *pendsv_c_handler(void *context_ptr){
  (void) context_ptr;

  /* Store current thread context from memory pointed to by context_ptr into TCB array */

  /* Do UB test to decide if current task set is executable? Here or in thread create? */

  /* Pick next thread to run based on algorithm and current thread with status Running */

  /* Load the next thread context onto memory pointed to by context_ptr, change psp or msp if needed?,
    change status in previous and next threads to appropriate status.*/

  return NULL;
}



int sys_thread_init(uint32_t max_threads, uint32_t stack_size, void *idle_fn, uint32_t max_mutexes){
  (void) max_threads; (void) stack_size; (void) idle_fn; (void) max_mutexes;
  return -1;

  /* Sets the size of TCB info array */

  /* Returns back to the user if the rounded stack_size value can fit in the stack space allocated based on max threads. 
    */

  /* Initialize the idle funtion which should be used when there are no more threads to run (I believe if the user wants
    a different function than the default idle function)
    */
}


int sys_thread_create(void *fn, uint32_t prio, uint32_t C, uint32_t T, void *vargp){
  (void) fn; (void) prio; (void) C; (void) T; (void) vargp;
  return -1;

  /* Add to the corresponding index in TCB table array based on unique priority as index */
  tcb_info_frame * TCB = TCB_Array[curr_size_TCB];
  
  TCB -> r0 = vargp;
  TCB -> PRIORITY = prio;
  TCB -> COMPUTE_TIME = C;
  TCB -> PERIOD_TIME = T;

  TCB -> psp = fn;

  curr_size_TCB ++;

  /* call scheduler algorithm to determine if new set is still schedulable */
  if (ub_test() < 0){
    return -1;
  }

  return 0;
}

int sys_scheduler_start( uint32_t frequency ){
  (void) frequency;
  return -1;
}

uint32_t sys_get_priority(){
  return 0U;
}

uint32_t sys_get_time(){
  return 0U;
}

uint32_t sys_thread_time(){
  return 0U;
}

void sys_thread_kill(){
}

void sys_wait_until_next_period(){
}

kmutex_t *sys_mutex_init( uint32_t max_prio ) {
  (void) max_prio;
  return NULL;
}

void sys_mutex_lock( kmutex_t *mutex ) {
  (void) mutex;
}

void sys_mutex_unlock( kmutex_t *mutex ) {
  (void) mutex;
}
