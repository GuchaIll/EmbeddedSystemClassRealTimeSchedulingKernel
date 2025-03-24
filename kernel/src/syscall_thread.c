/** @file   syscall_thread.c
 *
 *  @brief  
 *
 *  @date   
 *
 *  @author 
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
  uint32_t *stack_base_addr;
  uint32_t *stack_limit;

  uint32_t priority; //dynamic priority (0-16), thread ID should equate static priority
  uint32_t computation_time;
  uint32_t period; 
  thread_state_t state;
  void (*thread_fn)(void *); //function pointer to thread function
  void *vargp; //arguments to pass to thread function
  uint32_t context[16]; //context for thread
} TCB_t;

//array of thread control blocks 
//corresponding to static stack priorities
TCB_t threads[16];

void *pendsv_c_handler(void *context_ptr){
  (void) context_ptr;
  return NULL;
}


int sys_thread_init(
  uint32_t max_threads,
  uint32_t stack_size,
  void *idle_fn,
  uint32_t max_mutexes
){
  
  global_threads_info.max_threads = max_threads;
  global_threads_info.stack_size = mm_log2ceil_size(stack_size);

  //check if the stack size and max threads input are valid < 32KB
  if(max_threads > 16 || max_threads * stack_size * 4 > 32 * 1024) {
      return -1;
  }

  global_threads_info.tick_counter = 0;


  //allocate space for kernel and user stacks
  uint32_t *k_stack_top = (uint32_t *) &__thread_k_stacks_top;
  uint32_t *u_stack_top = (uint32_t *) &__thread_u_stacks_top;
  uint32_t *k_stack_low = (uint32_t *) &__thread_k_stacks_low;
  uint32_t *u_stack_low = (uint32_t *) &__thread_u_stacks_low;

  //Q: the handout said that kernel and user stacks are initiated differently?
  for(int i = 0; i < max_threads; i++) {
    threads[i].user_sp = u_stack_top - (i * stack_size * 4);
    threads[i].kernel_sp = k_stack_top - (i * stack_size * 4);
    threads[i].state = NEW; //Q: what state should the thread be set to in init?
    threads[i].stack_base_addr = u_stack_top - (i * stack_size * 4);
    threads[i].stack_limit = threads[i].stack_base_addr - (stack_size * 4);
  }
  //Initialize the idle thread
  threads[max_threads - 1].thread_fn = idle_fn;
  threads[max_threads - 1].priority = max_threads - 1;
  threads[max_threads - 1].vargp = NULL;

  
  return 0;
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

  threads[prio].thread_fn = fn;
  threads[prio].priority = prio;
  threads[prio].computation_time = C;
  threads[prio].period = T;
  threads[prio].vargp = vargp;

  uint32_t *user_sp = threads[prio].user_sp;
  user_sp -= sizeof(interrupt_stack_frame) / sizeof(uint32_t);

  interrupt_stack_frame *isf = (interrupt_stack_frame *) user_sp;
  isf->r0 = (uint32_t) vargp;
  isf->r1 = 0;
  isf->r2 = 0;
  isf->r3 = 0;
  isf->r12 = 0;
  isf->lr = LR_RETURN_TO_USER_PSP; //Q: what value should lr be set to, how do we know which mode should be returned to
  isf->pc = (uint32_t) fn;
  isf->xPSR = XPSR_INIT; //Q: how does the value of xPSR affect the thread

  threads[prio].user_sp = user_sp;

  threads[prio].state = READY;

  //Q: do we need to set up the kernel stack as well?
  


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
