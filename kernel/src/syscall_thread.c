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
#include <mpu.h>
#include <systick.h>
#include <syscall.h>
#include <printk.h>

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

  
  extern void* thread_kill;
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

/**
 * @struct global_threads_info_t
 * @brief Global thread information for tracking timing, priorities, and stack limits.
 */
typedef struct global_threads_info 
{
    uint32_t max_threads;
    uint32_t max_mutexes;
    uint32_t stack_size; // Size of stack in words
    uint32_t tick_counter;
    uint32_t current_thread; // index of the current thread in array

    uint32_t thread_time_left_in_C[16]; // array holding how many more ticks the corresponding thread has run in period
    uint32_t thread_time_left_in_T[16]; // array holding how many more ticks the corresponding thread has left until period end
    uint32_t thread_time[16]; // array holding at what tick the corresponding thread started

    uint32_t ready_threads[16]; // array holding the ready threads
    uint32_t waiting_threads[16]; // number of ready threads
  } global_threads_info_t;

/**
 * @enum thread_state_t
 * @brief Enumeration of thread states.
 */
typedef enum thread_state{
      NEW, //process created
      READY, //process ready to run
      RUNNING, //process running
      WAITING, //process waiting for event
      DONE //Exit
} thread_state_t;

/**
 * @struct pushed_callee_stack_frame
 * @brief Stack frame pushed onto MSP before initiating a context switch.
 */
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

/**
 * @struct TCB_t
 * @brief Thread Control Block (TCB) structure.
 *
 * This structure holds the context and metadata for a thread, including
 * its stack pointer, priority, computation time, period, and state.
 */
/* TCB struct that keeps track of thread main and program stack contexts and critical timing information*/
typedef struct TCB{
  pushed_callee_stack_frame *msp;

  uint32_t priority; //dynamic priority (0-16), thread ID should equate static priority
  uint32_t computation_time;
  uint32_t period; 
  uint32_t svc_status;

  thread_state_t state;

} TCB_t;

// Our TCB array which holds corresponding TCBs of our threads
/** @brief Array of Thread Control Blocks (TCBs) for all threads. */
TCB_t TCB_ARRAY[16];

// Our global thread info struct that holds important information for timing, stack sizes, and max priorities.
/** @brief Global structure holding thread-related information. */
global_threads_info_t global_threads_info;

uint32_t *u_stack_low;

uint32_t *k_stack_low;

/* ub_test(C, T)
  1. Computes C/T of new thread 
  2. Computes overall utilization based on threads already created
  3. Sums new thread utilization with old total utilization and checks against corresponding ub bound 
  4. Returns the result
*/

/**
 * @brief Performs the utilization bound (UB) test for thread creation.
 *
 * @param[in] C Computation time of the new thread.
 * @param[in] T Period of the new thread.
 * @return 0 if the thread passes the UB test, -1 otherwise.
 */
int ub_test(int C, int T){
  int max_threads = global_threads_info.max_threads;
  float utilization = (float)C/T;
  int compute, period = 0;
  int count = 1;
  for (int index = 0; index < max_threads; index ++){
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

/* sysTickFlag:
  Flag to keep track of context switching not started by the systick so as to not decrement C or T when that happens*/
extern uint32_t sysTickFlag;


/**
 * @brief Scheduler function to manage thread execution.
 *
 * This function reduces the computation time (C) of the currently running thread,
 * transitions threads to the appropriate state based on their computation time
 * and period, and selects the next thread to run.
 *
 * thread_scheduler()
  1. Reduces the compute time left of the currently running thread 
  2  If compute time is 0, then switch to WAITING state for the thread
  3. Reduces the period time left of all the threads 
  4. If period time left is 0, then switch to READY state and reset the values of C time left and T time left 
  5. Loops through the thread array to find the closest thread to priority 0 that is READY state
  6. Switches the next thread to be run's state to RUNNING and returns its priority
 * @return The priority of the next thread to run.
 */
int thread_scheduler(){
  /* Get currently running thread */
  uint32_t curr_running = sys_get_priority();
  uint32_t max_threads = global_threads_info.max_threads;
  

  // Sets all to ready
  for(uint32_t i = 0; i < global_threads_info.max_threads + 2; i++){
    if (TCB_ARRAY[i].state == RUNNING){
      TCB_ARRAY[i].state = READY;
    }
  }

  //Perform timer and compute time changes only if pend_sv is called by systick

  if(sysTickFlag == 1){
    if (curr_running != max_threads && curr_running != max_threads + 1){
      int time_left_in_compute = global_threads_info.thread_time_left_in_C[curr_running] - 1;
      global_threads_info.thread_time[curr_running] ++;
      if (time_left_in_compute == 0){
        time_left_in_compute = TCB_ARRAY[curr_running].computation_time;
        TCB_ARRAY[curr_running].state = WAITING;
      }
      global_threads_info.thread_time_left_in_C[curr_running] = time_left_in_compute;
    }
    else{
      TCB_ARRAY[curr_running].state = RUNNING;
    }

    int time_left_in_period;
    for (uint32_t i = 0; i < global_threads_info.max_threads; i ++){
      if (TCB_ARRAY[i].state == READY || TCB_ARRAY[i].state == WAITING){
        time_left_in_period = global_threads_info.thread_time_left_in_T[i] - 1;
        if (time_left_in_period == 0){
          TCB_ARRAY[i].state = READY;
          time_left_in_period = TCB_ARRAY[i].period;
          global_threads_info.thread_time_left_in_C[i] = TCB_ARRAY[i].computation_time;
        }
        global_threads_info.thread_time_left_in_T[i] = time_left_in_period;
      }
    }
    
    clear_systick_flag();
}
  
  /* Pick next thread to run based on algorithm and current thread with status Running */
  int boolean = 0;
  for (uint32_t priority = 0; priority < max_threads; priority ++){
    if (TCB_ARRAY[priority].state == WAITING){
      boolean = 1;
    }
    if (TCB_ARRAY[priority].state == READY){
      return priority;
    }
  }
  if (boolean == 1){
    return max_threads;
  }
  return max_threads + 1;
}


/**
 * @brief PendSV handler for context switching.
 *
 * This function saves the current thread's context, selects the next thread to run,
 * and restores the context of the next thread.
 *
 * pendsv_c_handler(context_ptr)
  1. Saves context to TCB (basically just saving the msp)
  2. Calls the scheduler to return priority of the next thread to be run
  3. Retrieves the msp of the next thread to be run and returns that value

 * @param[in] context_ptr Pointer to the context of the current thread.
 * @return Pointer to the context of the next thread to run.
 */

void *pendsv_c_handler(void *context_ptr){
    /* Store current thread context from memory pointed to by context_ptr into TCB array */

    uint32_t current_thread = sys_get_priority();

    // save svc_status
    uint32_t svc_stat = get_svc_status();

    pushed_callee_stack_frame * callee_saved_stk = context_ptr;

  
    TCB_t * TCB = &TCB_ARRAY[current_thread]; //tcb context automatically stores the entire context
  
    /** You might have to alter it but the TA said we do not need to set the register individually here,
     * could just set TCB->msp = *callee_saved_stk
     */
    TCB->msp = callee_saved_stk;

    TCB->svc_status = svc_stat;
  
    int priority = thread_scheduler();

    global_threads_info.current_thread = priority;

    //if((uint32_t)priority != current_thread)
    //{
   //   TCB_t * current_TCB = &TCB_ARRAY[current_thread];
    //  current_TCB -> state = READY;
   // }

    TCB_t * next_TCB = &TCB_ARRAY[priority];

    next_TCB -> state = RUNNING;

    svc_stat = next_TCB -> svc_status;
    set_svc_status(svc_stat);

    global_threads_info.waiting_threads[priority] = 400;
    global_threads_info.ready_threads[priority] = 400;

    /* Load pointer to TCB to return value*/
    return next_TCB -> msp;
}

/**
 * @brief Default idle function.
 *
 * This function is executed when no other threads are ready to run.
 */
void default_idle_fn(){
  while(1){
    wait_for_interrupt();
  }
}


/**
 * @brief Initializes the thread system.
 *
 * Sets up the global thread information, allocates stack space, and initializes
 * the idle thread and default thread.
 * 
 * sys_thread_init(max_threads, stack_size, idle_fn, max_mutexes
 *1. Updates global thread values such as max_threads and stack size
 * 2. Computes all psp and msp pointers for threads to be run
 * 3. Sets up the idle_fn with the default values at the final value of priortiy in the array (depends on max_threads) 
 * 4. Sets up interrupt stack frame for idle_fn on its psp
 * 5. Sets up default values in the precomputed msp that contain psp and r4, etc, error code in lr.
 *
 * @param[in] max_threads Maximum number of threads.
 * @param[in] stack_size Stack size for each thread.
 * @param[in] idle_fn Pointer to the idle function.
 * @param[in] max_mutexes Maximum number of mutexes.
 * @return 0 on success, -1 on failure.
 */
int sys_thread_init(uint32_t max_threads, uint32_t stack_size, void *idle_fn, uint32_t max_mutexes){
  global_threads_info.max_mutexes = max_mutexes;
  global_threads_info.max_threads = max_threads;

  uint32_t size = 256;
  while (size < stack_size){
    size = size * 2;
  }
  global_threads_info.stack_size = size;

  //check if the stack size and if max threads input are valid < 32KB
  if(max_threads > 14 || max_threads * stack_size * 4 > 32 * 1024) {
      return -1;
  }

  global_threads_info.tick_counter = 0;

  //allocate space for kernel and user stacks
  uint32_t *k_stack_top = (uint32_t *) &__thread_k_stacks_top;
  uint32_t *u_stack_top = (uint32_t *) &__thread_u_stacks_top;

  //Q: the handout said that kernel and user stacks are initiated differently?
   for(uint32_t i = 0; i < max_threads+2; i++) {
     // TCB_ARRAY[i].msp = k_stack_top - (i * global_threads_info.stack_size * 4);
     // ((pushed_callee_stack_frame *)TCB_ARRAY[i].msp)->PSP = u_stack_top - (i * global_threads_info.stack_size * 4);
     
     uint32_t *aligned_k_stack_top = k_stack_top - (i * global_threads_info.stack_size ) - sizeof(pushed_callee_stack_frame) / sizeof(uint32_t);
     uint32_t *aligned_u_stack_top = u_stack_top - (i * global_threads_info.stack_size ) - sizeof(interrupt_stack_frame) / sizeof(uint32_t);
     
     TCB_ARRAY[i].msp = (pushed_callee_stack_frame *)aligned_k_stack_top;
     TCB_ARRAY[i].msp->PSP = aligned_u_stack_top;
     //TCB_ARRAY[i].msp = (pushed_callee_stack_frame *)(k_stack_top - (i * global_threads_info.stack_size * 4) - sizeof(pushed_callee_stack_frame));
     //TCB_ARRAY[i].msp -> PSP = (u_stack_top - (i * global_threads_info.stack_size * 4) - sizeof(interrupt_stack_frame)); // Why cast to pushed_callee_stack?
     TCB_ARRAY[i].state = NEW; //Q: what state should the thread be set to in init?
     TCB_ARRAY[i].svc_status = 0; // Might want to set all svc_status to 0?
   }

  //Initialize the idle thread
 // threads[max_threads - 1].thread_fn = idle_fn;
 //assuming max value for max_threads is 14
 
 if (idle_fn == NULL) {
  idle_fn = default_idle_fn; // Use default idle function for the user input null idle fn
}
  // ?? I think idle thread goes into second to last priority and default goes into last
  // Also should set the default thread to be running

  int prio_idle = max_threads;
  TCB_ARRAY[prio_idle].computation_time = 0x1;
  TCB_ARRAY[prio_idle].period = 0x1;
  TCB_ARRAY[prio_idle].priority = prio_idle;

  interrupt_stack_frame *user_sp = (interrupt_stack_frame *)TCB_ARRAY[prio_idle].msp->PSP;

  user_sp->r0 = 0;
  user_sp->r1 = 0;
  user_sp->r2 = 0;
  user_sp->r3 = 0;
  user_sp->r12 = 0;
  user_sp->lr = (uint32_t)&thread_kill; // changed from default
  user_sp->pc = (uint32_t) idle_fn;
  user_sp->xPSR = XPSR_INIT;

  TCB_ARRAY[prio_idle].msp->r4 = 0;
  TCB_ARRAY[prio_idle].msp->r5 = 0;
  TCB_ARRAY[prio_idle].msp->r6 = 0;
  TCB_ARRAY[prio_idle].msp->r7 = 0;
  TCB_ARRAY[prio_idle].msp->r8 = 0;
  TCB_ARRAY[prio_idle].msp->r9 = 0;
  TCB_ARRAY[prio_idle].msp->r10 = 0;
  TCB_ARRAY[prio_idle].msp->r11 = 0;
  TCB_ARRAY[prio_idle].msp->lr = LR_RETURN_TO_USER_PSP;
  

  TCB_ARRAY[prio_idle].state = READY;
  TCB_ARRAY[prio_idle].svc_status = 0; //clown moment

  global_threads_info.thread_time[prio_idle] = 0;
  global_threads_info.thread_time_left_in_T[prio_idle] = 1;
  global_threads_info.thread_time_left_in_C[prio_idle] = 1;

  // Initiating default thread values
  int prio_default = max_threads + 1;
  TCB_ARRAY[prio_default].computation_time = 0x1;
  TCB_ARRAY[prio_default].period = 0x1;
  TCB_ARRAY[prio_default].priority = prio_default;
  TCB_ARRAY[prio_default].state = RUNNING;
  TCB_ARRAY[prio_default].svc_status = 0; //clown moment

  global_threads_info.thread_time[prio_default] = 0;
  global_threads_info.thread_time_left_in_T[prio_default] = 1;
  global_threads_info.thread_time_left_in_C[prio_default] = 1;

  for(uint32_t i = 0; i < max_threads; i++){
    global_threads_info.ready_threads[i] = 400;
    global_threads_info.waiting_threads[i] = 400;
  }

  global_threads_info.current_thread = prio_default;


  return 0;
}


/**
 * @brief Creates a new thread.
 *
 * Initializes the thread's TCB, sets up its stack, and configures its
 * computation time and period.
 * 
 * sys_thread_create(fn, prio, C, T, vargp)
 * 1. Do checks to ensure that thread creation would be valid
 * 2. Updates TCB of corresponding priority with values from inputs 
 * 3. Sets up the stack pointed to by the threads MSP (at precomputed address) to contain default values PSP (precomputed), r4 - r11, lr
 * 4. Sets up the stack pointed to by the threads PSP with interrupt stack frame and fills it in with function pointer and vargp
 * 5. Sets up the global thread info values to include things like appropriate thread absolute start time, and time left in period. 
 *
 * @param[in] fn Function pointer to the thread's entry function.
 * @param[in] prio Priority of the thread.
 * @param[in] C Computation time (C) in ticks.
 * @param[in] T Period (T) in ticks.
 * @param[in] vargp Pointer to the thread's argument.
 * @return 0 on success, -1 on failure.
 */
int sys_thread_create(void *fn,uint32_t prio,uint32_t C,uint32_t T, void *vargp){

  if(prio >= global_threads_info.max_threads || TCB_ARRAY[prio].state == READY) {
    return -1;
  }

    //check if the thread is new or can be recycled, if not return -1
   // if(TCB_ARRAY[prio].state != DONE && TCB_ARRAY[prio].state != NEW) {
   //   return -1;
   // }
  

  if(ub_test(C, T) < 0) {
    return -1;
  }

  TCB_ARRAY[prio].priority = prio;
  TCB_ARRAY[prio].computation_time = C;
  TCB_ARRAY[prio].period = T;

  interrupt_stack_frame * user_sp = (interrupt_stack_frame *)TCB_ARRAY[prio].msp->PSP;
  
  user_sp->r0 = (uint32_t) vargp;
  user_sp->r1 = 0;
  user_sp->r2 = 0;
  user_sp->r3 = 0;
  user_sp->r12 = 0;
  user_sp->lr = (uint32_t) &thread_kill;
  user_sp->pc = (uint32_t) fn;
  user_sp->xPSR = XPSR_INIT;

  TCB_ARRAY[prio].msp->r4 = 0;
  TCB_ARRAY[prio].msp->r5 = 0;
  TCB_ARRAY[prio].msp->r6 = 0;
  TCB_ARRAY[prio].msp->r7 = 0;
  TCB_ARRAY[prio].msp->r8 = 0;
  TCB_ARRAY[prio].msp->r9 = 0;
  TCB_ARRAY[prio].msp->r10 = 0;
  TCB_ARRAY[prio].msp->r11 = 0;
  TCB_ARRAY[prio].msp->lr = LR_RETURN_TO_USER_PSP;

  TCB_ARRAY[prio].svc_status = 0;
  TCB_ARRAY[prio].state = READY;

  /* Setting New Thread System Time Variables */
  global_threads_info.thread_time[prio] = 0;
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
/**
 * @brief Gets the priority of the currently running thread.
 *
 * @return The priority of the currently running thread.
 */
uint32_t sys_get_priority(){
 
  //printk("Error: No thread is running");
  return global_threads_info.current_thread;
}

// Global From Systick.c
extern uint32_t total_count;

/**
 * @brief Gets the total tick counts since the global timer has started.
 *
 * @return the current total tick values.
 */
uint32_t sys_get_time(){
  uint32_t timeValue = systick_get_ticks();
  return timeValue;
}

/**
 * @brief Gets the absolute time of the currently running thread.
 *
 * @return The absolute time of the currently running thread.
 */
uint32_t sys_thread_time(){
  int priority = sys_get_priority();
  int abs_thread_time = global_threads_info.thread_time[priority];
  return abs_thread_time;
}

/**
 * @brief Permanently deschedules the currently running thread.
 *
 * Transitions the thread to the DONE state and selects the next thread to run.
 */
void sys_thread_kill(){
  uint32_t priority = sys_get_priority();
  // uint32_t max_threads = global_threads_info.max_threads;
  if (priority == global_threads_info.max_threads+1){ // Default Thread Is Called It
    sys_exit(1);
  }
  else if(priority == global_threads_info.max_threads){// Idle Thread is Calling It 
    // run default idle
    interrupt_stack_frame * psp = (interrupt_stack_frame *)(TCB_ARRAY[global_threads_info.max_threads].msp -> PSP);
    psp -> pc = (uint32_t) &default_idle_fn;
  }
  else{
    TCB_ARRAY[priority].state = DONE;
    // for (uint32_t i = 0; i < max_threads; i ++){

    // }
    pend_pendsv();
  }
  return;
}

/**
 * @brief Sets the current thread to wait until the next period.
 *
 * Transitions the thread to the WAITING state and triggers the PendSV interrupt
 * to invoke the scheduler.
 */
void sys_wait_until_next_period(){ //needs fixing
  int priority = sys_get_priority();
  TCB_ARRAY[priority].state = WAITING;

  pend_pendsv();
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