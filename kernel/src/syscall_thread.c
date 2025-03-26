/** @file   syscall_thread.c
 *
 *  @brief  sys calls for thread creation and initialization
 *
 *  @date   3/24/25
 *
 *  @author Charlie Ai and Mario Cruz
 */

#include <stdint.h>
#include "syscall_thread.h"
#include "syscall_mutex.h"
#include <arm.h>

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

/** ub_table:
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

/** Stack frame pushed onto PSP upon exception
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

/** Info that is important for tracking timing, maximum priorities, and stack limits*/
typedef struct global_threads_info 
{
    uint32_t max_threads;
    uint32_t stack_size;
    uint32_t tick_counter;
    uint32_t current_thread; // index of the current thread in array

    uint32_t thread_time_left_in_C[16]; // array holding how many more ticks the corresponding thread has run in period
    uint32_t thread_time_left_in_T[16]; // array holding how many more ticks the corresponding thread has left until period end
    uint32_t thread_absolute_time_start[16]; // array holding at what tick the corresponding thread started

} global_threads_info_t;

/* Enumerating our thread states */
typedef enum thread_state{
      NEW, //process created
      READY, //process ready to run
      RUNNING, //process running
      WAITING, //process waiting for event
      DONE //Exit
} thread_state_t;

/* Stack frame pushed onto MSP before intiating context switch */
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

/* TCB struct that keeps track of thread main and program stack contexts and critical timing information*/
typedef struct TCB{
  pushed_callee_stack_frame *msp;

  uint32_t priority; //dynamic priority (0-16), thread ID should equate static priority
  uint32_t computation_time;
  uint32_t period; 
  thread_state_t state;

} TCB_t;

// Our TCB array which holds corresponding TCBs of our threads
TCB_t TCB_ARRAY[16];

// Our global thread info struct that holds important information for timing, stack sizes, and max priorities.
global_threads_info_t global_threads_info;

/* ub_test(C, T)
  1. Computers C/T of new thread 
  2. Computes overall utilization based on threads already created
  3. Sums new thread utilization with old total utilization and checks against corresponding ub bound 
  4. Returns the result
*/
int ub_test(int C, int T){
  float utilization = (float)C/T;
  int compute, period = 0;
  int count = 1;
  for (int index = 0; index < 16; index ++){
    if (TCB_ARRAY[index].state == NEW || TCB_ARRAY[index].state == DONE){continue;} // Uninitialized Thread Index in TCB Array
    compute = TCB_ARRAY[index].computation_time;
    period = TCB_ARRAY[index].period;
    utilization += (float)compute/period;
    count ++;
  }
  if (utilization <= ub_table[count]){
    return 0;
  }
  return -1;
}


/* thread_scheduler()
  1. Reduces the compute time left of the currently running thread 
  2  If compute time is 0, then switch to WAITING state for the thread
  3. Reduces the period time left of all the threads 
  4. If period time left is 0, then switch to READY state and reset the values of C time left and T time left 
  5. Loops through the thread array to find the closest thread to priority 0 that is READY state
  6. Switches the next thread to be run's state to RUNNING and returns its priority
*/
int thread_scheduler(){
  /* Get currently running thread */
  int curr_running = sys_get_priority();
  
  /* Decrement compute time left  value */
  int time_left_in_compute = global_threads_info.thread_time_left_in_C[curr_running] - 1;
  global_threads_info.thread_time_left_in_C[curr_running] = time_left_in_compute;

  /* Check if thread used up all computation time */
  if (time_left_in_compute == 0){ // Just finished compute time
    sys_wait_until_next_period();
  }

  /* Decrement/reset all values of thread_time_left_in_period and reset state if necessary */
  for (int p = 0; p < 16; p ++){
    int time_left_in_period = global_threads_info.thread_time_left_in_T[p] - 1;
    global_threads_info.thread_time_left_in_T[p] = time_left_in_period;
    if (time_left_in_period == 0 && (TCB_ARRAY[p].state == WAITING || TCB_ARRAY[p].state == RUNNING)){
      global_threads_info.thread_time_left_in_T[p] = TCB_ARRAY[p].period;
      TCB_ARRAY[p].state = READY;
    }
  }
  
  /* Pick next thread to run based on algorithm and current thread with status Running */
  int priority = 0;
  while (TCB_ARRAY[priority].state != READY){
    priority ++;
  }
  return priority;
}


/* pendsv_c_handler(context_ptr)
  1. Saves context to TCB (basically just saving the msp)
  2. Calls the scheduler to return priority of the next thread to be run
  3. Retrieves the msp of the next thread to be run and returns that value
*/
void *pendsv_c_handler(void *context_ptr){
    /* Store current thread context from memory pointed to by context_ptr into TCB array */

    uint32_t current_thread = sys_get_priority();

    pushed_callee_stack_frame * callee_saved_stk = context_ptr;
  
    TCB_t * TCB = &TCB_ARRAY[current_thread]; //tcb context automatically stores the entire context
  
    /** You might have to alter it but the TA said we do not need to set the register individually here,
     * could just set TCB->msp = *callee_saved_stk
     */
    TCB->msp->PSP = callee_saved_stk ->PSP;
    TCB->msp->r4 = callee_saved_stk ->r4;
    TCB->msp->r5 = callee_saved_stk -> r5;
    TCB->msp->r6 = callee_saved_stk -> r6;
    TCB->msp->r7 = callee_saved_stk -> r7;
    TCB->msp->r8 = callee_saved_stk -> r8;
    TCB->msp->r9 = callee_saved_stk -> r9;
    TCB->msp->r10 = callee_saved_stk -> r10;
    TCB->msp->r11 = callee_saved_stk -> r11;
    TCB->msp->lr = callee_saved_stk -> lr;
  
  
    int priority = thread_scheduler();
    TCB_t * next_TCB = &TCB_ARRAY[priority];
    next_TCB -> state = RUNNING;
  
    /* Load pointer to TCB to return value*/
    return next_TCB;
}


/* sys_thread_init(max_threads, stack_size, idle_fn, max_mutexes
  1. Updates global thread values such as max_threads and stack size
  2. Computes all psp and msp pointers for threads to be run
  3. Sets up the idle_fn with the default values at the final value of priortiy in the array (depends on max_threads) 
  4. Sets up interrupt stack frame for idle_fn on its psp
  5. Sets up default values in the precomputed msp that contain psp and r4, etc, error code in lr.
*/
int sys_thread_init(uint32_t max_threads, uint32_t stack_size, void *idle_fn, uint32_t max_mutexes){
  
  global_threads_info.max_threads = max_threads;
  global_threads_info.stack_size = mm_log2ceil_size(stack_size);

  //check if the stack size and if max threads input are valid < 32KB
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
    // TCB_ARRAY[i].msp = k_stack_top - (i * global_threads_info.stack_size * 4);
    // ((pushed_callee_stack_frame *)TCB_ARRAY[i].msp)->PSP = u_stack_top - (i * global_threads_info.stack_size * 4);


    TCB_ARRAY[i].msp = k_stack_top - (i * global_threads_info.stack_size * 4) - sizeof(pushed_callee_stack_frame);
    TCB_ARRAY[i].msp -> PSP = u_stack_top - (i * global_threads_info.stack_size * 4); // Why cast to pushed_callee_stack?

    // TCB_ARRAY[i].msp -> PSP = u_stack_top - (i * global_threads_info.stack_size * 4) - sizeof(interrupt_stack_frame);
    TCB_ARRAY[i].state = NEW; //Q: what state should the thread be set to in init?
  }


  //Initialize the idle thread
 // threads[max_threads - 1].thread_fn = idle_fn;
 //assuming max value for max_threads is 14

  int prio = max_threads - 1;
  TCB_ARRAY[prio].computation_time = 0xFFFFFFFF;
  TCB_ARRAY[prio].period = 0xFFFFFFFF;
  TCB_ARRAY[prio].priority = max_threads - 1;

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

  /* NEW */
  /* Setting Idle Thread System Time Variables */
  global_threads_info.thread_absolute_time_start[max_threads-1] = 0;
  global_threads_info.thread_time_left_in_T[max_threads-1] = 1;
  global_threads_info.thread_time_left_in_C[max_threads-1] = 0;
  /* END NEW */

  return 0;
}


/* sys_thread_create(fn, prio, C, T, vargp)
  1. Do checks to ensure that thread creation would be valid
  2. Updates TCB of corresponding priority with values from inputs 
  3. Sets up the stack pointed to by the threads MSP (at precomputed address) to contain default values PSP (precomputed), r4 - r11, lr
  4. Sets up the stack pointed to by the threads PSP with interrupt stack frame and fills it in with function pointer and vargp
  5. Sets up the global thread info values to include things like appropriate thread absolute start time, and time left in period. 
*/
int sys_thread_create(void *fn,uint32_t prio,uint32_t C,uint32_t T,void *vargp){

  if(prio >= global_threads_info.max_threads || TCB_ARRAY[prio].state == READY) {
    return -1;
  }

  if(ub_test(C, T) < 0) {
    return -1;
  }

  TCB_ARRAY[prio].priority = prio;
  TCB_ARRAY[prio].computation_time = C;
  TCB_ARRAY[prio].period = T;

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

  /* Setting New Thread System Time Variables */
  global_threads_info.thread_absolute_time_start[prio] = sys_get_time();
  global_threads_info.thread_time_left_in_T[prio] = T;
  global_threads_info.thread_time_left_in_C[prio] = C;
  /* END NEW */

  return 0;
}


/* Inits the systick timer and runs the pend_sv interrupt */
int sys_scheduler_start( uint32_t frequency ){
  systick_init(frequency);
  pend_pendsv();
  return 0;
}


/* Loops through TCB array to find priority of thread which is running */
uint32_t sys_get_priority(){
  for (int i = 0; i < 16; i ++){
    if (TCB_ARRAY[i].state == RUNNING){
      return i;
    } 
  }
  return 0U;
}


// Global From Systick.c
extern uint32_t total_count;
uint32_t sys_get_time(){
  return total_count;
}


// Gets priority and indexes into global thread info
uint32_t sys_thread_time(){
  int priority = sys_get_priority();
  int abs_thread_time = global_threads_info.thread_absolute_time_start[priority];
  return abs_thread_time;
}


/* Permanently deschedules the thread its called from. Checks if valid to kill this particular thread */
void sys_thread_kill(){
  int priority = sys_get_priority();
  if (priority == 0U){ // Default Thread Is Called It
    sys_exit();
  }
  else if(priority == global_threads_info.max_threads-1){// Idle Thread is Calling It 
    // run default idle
  }
  else{
    TCB_ARRAY[priority].state = DONE;
  }
  return;
}


/* Sets current thread to wait until next period*/
void sys_wait_until_next_period(){
  int priority = sys_get_priority();
  TCB_ARRAY[priority].state = WAITING;
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