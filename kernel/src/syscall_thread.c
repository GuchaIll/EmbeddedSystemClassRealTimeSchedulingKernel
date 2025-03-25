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

typedef struct global_threads_info 
{
    uint32_t max_threads;
    uint32_t stack_size;
    uint32_t tick_counter;

} global_threads_info_t;

global_threads_info_t global_threads_info;

typedef enum thread_state
{
  NEW, //process created
  READY, //process ready to run
  RUNNING, //process running
  WAITING, //process waiting for event
  DONE //Exit
} thread_state_t;

typedef struct TCB
{
  uint32_t *user_sp; //user stack pointer
  uint32_t *kernel_sp; //kernel stack pointer
  
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t lr;

  uint32_t *stack_base_addr;
  uint32_t *stack_limit;

  uint32_t priority; //dynamic priority (0-16), thread ID should equate static priority
  uint32_t computation_time;
  uint32_t period; 
  thread_state_t state;

  void *vargp; //arguments to pass to thread function

} TCB_t;

//array of thread control blocks 
//corresponding to static stack priorities
TCB_t threads[16];

void *pendsv_c_handler(void *context_ptr){
  (void) context_ptr;

  /* Store current thread context from memory pointed to by context_ptr into TCB array */

  /* Do UB test to decide if current task set is executable? Here or in thread create? */

  /* Pick next thread to run based on algorithm and current thread with status Running */

  /* Load the next thread context onto memory pointed to by context_ptr, change psp or msp if needed?,
    change status in previous and next threads to appropriate status.*/

  return NULL;
}


int sys_thread_init(
  uint32_t max_threads,
  uint32_t stack_size,
  void *idle_fn,
  uint32_t max_mutexes
){
  (void) max_threads; (void) stack_size; (void) idle_fn; (void) max_mutexes;
  return -1;
}

int sys_thread_create(
  void *fn,
  uint32_t prio,
  uint32_t C,
  uint32_t T,
  void *vargp
){
  //check if the thread is already created, or a thread already exists with same priority
  //return error
  if(prio >= global_threads_info.max_threads || threads[prio].state == READY) {
    return -1;
  }

  threads[prio].priority = prio;
  threads[prio].computation_time = C;
  threads[prio].period = T;
  threads[prio].vargp = vargp;

  uint32_t *user_sp = threads[prio].user_sp;
  user_sp -= sizeof(interrupt_stack_frame) / sizeof(uint32_t);

  interrupt_stack_frame *isf_u = (interrupt_stack_frame *) user_sp;
  isf_u->r0 = (uint32_t) vargp;
  isf_u->r1 = 0;
  isf_u->r2 = 0;
  isf_u->r3 = 0;
  isf_u->r12 = 0;
  isf_u->lr = LR_RETURN_TO_USER_PSP;
  isf_u->pc = (uint32_t) fn;
  isf_u->xPSR = XPSR_INIT;

  threads[prio].r4 = 0;
  threads[prio].r5 = 0;
  threads[prio].r6 = 0;
  threads[prio].r7 = 0;
  threads[prio].r8 = 0;
  threads[prio].r9 = 0;
  threads[prio].r10 = 0;
  threads[prio].r11 = 0;
  threads[prio].lr = LR_RETURN_TO_USER_PSP;

  threads[prio].user_sp = user_sp;

  interrupt_stack_frame *isf_k = (interrupt_stack_frame *) threads[prio].kernel_sp;

  threads[prio].state = READY;

  //Q: do we need to set up the kernel stack as well?
  if(ub_test() < 0) {
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
