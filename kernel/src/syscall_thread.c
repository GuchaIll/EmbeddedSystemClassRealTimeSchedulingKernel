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
    uint32_t current_thread; //index of the current thread in array

} global_threads_info_t;

extern global_threads_info_t global_threads_info;

typedef enum thread_state
{
  NEW, //process created
  READY, //process ready to run
  RUNNING, //process running
  WAITING, //process waiting for event
  DONE //Exit
} thread_state_t;

typedef struct {
  uint32_t *PSP;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t lr;

} pushed_callee_stack_frame;

typedef struct TCB
{
 //main stack pointer
  pushed_callee_stack_frame *msp;

  uint32_t priority; //dynamic priority (0-16), thread ID should equate static priority
  uint32_t computation_time;
  uint32_t period; 
  thread_state_t state;

  void *vargp; //arguments to pass to thread function

} TCB_t;

//array of thread control blocks 
//corresponding to static stack priorities
TCB_t TCB_ARRAY[16];

int ub_test(){
  float utilization = 0;
  int C, T, count = 0;
  for (int index = 0; index < 16; index ++){
    if (TCB_ARRAY[index].state == NEW || TCB_ARRAY[index].state == DONE){continue;} // Uninitialized Thread Index in TCB Array
    C = TCB_ARRAY[index].computation_time;
    T = TCB_ARRAY[index].period;
    utilization += (float)C/T;
    count ++;
  }
  if (utilization <= ub_table[count]){
    return 0;
  }
  return -1;
}

void *pendsv_c_handler(void *context_ptr){
    /* Store current thread context from memory pointed to by context_ptr into TCB array */

    uint32_t current_thread = sys_get_priority();

    pushed_callee_stack_frame * callee_saved_stk = context_ptr;
  
    TCB_t * TCB = &TCB_ARRAY[current_thread]; //tcb context automatically stores the entire context
  
    /** You might have to alter it but the TA said we do not need to set the register individually here,
     * could just set TCB->msp = *callee_saved_stk
     */
    TCB->msp->r4 = callee_saved_stk->r4;
    TCB->msp->r5 = callee_saved_stk -> r5;
    TCB->msp->r6 = callee_saved_stk -> r6;
    TCB->msp->r7 = callee_saved_stk -> r7;
    TCB->msp->r8 = callee_saved_stk -> r8;
    TCB->msp->r9 = callee_saved_stk -> r9;
    TCB->msp->r10 = callee_saved_stk -> r10;
    TCB->msp->r11 = callee_saved_stk -> r11;
    TCB->msp->lr = callee_saved_stk -> lr;
  
    /* Go through the threads and determine which ones should be READY or WAITING*/
  
    /* Still Have To Implement It*/
  
    /* Pick next thread to run based on algorithm and current thread with status Running */
  
    int priority = 0;
    while (TCB_ARRAY[priority].state != READY){
      priority ++;
    }
  
    TCB_t * next_TCB = &TCB_ARRAY[priority];
    next_TCB -> state = RUNNING;
  
    /* Load pointer to TCB to return value*/
    return next_TCB;
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
    
    TCB_ARRAY[i].msp = k_stack_top - (i * global_threads_info.stack_size * 4);
    ((pushed_callee_stack_frame *)TCB_ARRAY[i].msp)->PSP = u_stack_top - (i * global_threads_info.stack_size * 4);
    TCB_ARRAY[i].state = NEW; //Q: what state should the thread be set to in init?
   
  }
  //Initialize the idle thread
 // threads[max_threads - 1].thread_fn = idle_fn;
 //assuming max value for max_threads is 14
  int prio = 14;
  TCB_ARRAY[prio].computation_time = 0xFFFFFFFF;
  TCB_ARRAY[prio].period = 0xFFFFFFFF;
  TCB_ARRAY[prio].priority = max_threads - 1;
  TCB_ARRAY[prio].vargp = 0;

  uint32_t *user_sp = (uint32_t *)TCB_ARRAY[prio].msp->PSP;
  user_sp -= sizeof(interrupt_stack_frame) / sizeof(uint32_t);

  interrupt_stack_frame *isf_u = (interrupt_stack_frame *) user_sp;
  isf_u->r0 = 0;
  isf_u->r1 = 0;
  isf_u->r2 = 0;
  isf_u->r3 = 0;
  isf_u->r12 = 0;
  isf_u->lr = LR_RETURN_TO_USER_PSP;
  isf_u->pc = (uint32_t) idle_fn;
  isf_u->xPSR = XPSR_INIT;

  TCB_ARRAY[prio].msp->r4 = 0;
  TCB_ARRAY[prio].msp->r5 = 0;
  TCB_ARRAY[prio].msp->r6 = 0;
  TCB_ARRAY[prio].msp->r7 = 0;
  TCB_ARRAY[prio].msp->r8 = 0;
  TCB_ARRAY[prio].msp->r9 = 0;
  TCB_ARRAY[prio].msp->r10 = 0;
  TCB_ARRAY[prio].msp->r11 = 0;
  TCB_ARRAY[prio].msp->lr = LR_RETURN_TO_USER_PSP;

  TCB_ARRAY[prio].msp->PSP = user_sp;
  TCB_ARRAY[prio].state = READY;

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
  if(prio >= global_threads_info.max_threads || TCB_ARRAY[prio].state == READY) {
    return -1;
  }

  TCB_ARRAY[prio].priority = prio;
  TCB_ARRAY[prio].computation_time = C;
  TCB_ARRAY[prio].period = T;
  TCB_ARRAY[prio].vargp = vargp;

  uint32_t *user_sp = TCB_ARRAY[prio].msp->PSP;
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

  TCB_ARRAY[prio].msp->r4 = 0;
  TCB_ARRAY[prio].msp->r5 = 0;
  TCB_ARRAY[prio].msp->r6 = 0;
  TCB_ARRAY[prio].msp->r7 = 0;
  TCB_ARRAY[prio].msp->r8 = 0;
  TCB_ARRAY[prio].msp->r9 = 0;
  TCB_ARRAY[prio].msp->r10 = 0;
  TCB_ARRAY[prio].msp->r11 = 0;
  TCB_ARRAY[prio].msp->lr = LR_RETURN_TO_USER_PSP;

  TCB_ARRAY[prio].msp->PSP = user_sp;




  TCB_ARRAY[prio].state = READY;

  //Q: do we need to set up the kernel stack as well?
  if(ub_test() < 0) {
    return -1;
  }

  return 0;
}

int sys_scheduler_start( uint32_t frequency ){
 
  systick_init(frequency);

  //setting up the default thread 
  int prio = 15;
  TCB_ARRAY[prio].computation_time = 0xFFFFFFFF;
  TCB_ARRAY[prio].period = 0xFFFFFFFF;
  TCB_ARRAY[prio].priority = 15;
  TCB_ARRAY[prio].vargp = 0;

  pend_pendsv();

  return 0;
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
