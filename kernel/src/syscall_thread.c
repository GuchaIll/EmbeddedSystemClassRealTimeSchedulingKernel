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
   __thread_u_stacks_low,      /**< Low address of user stack. */
   __thread_u_stacks_top,      /**< Top address of user stack. */
   __thread_k_stacks_low,      /**< Low address of kernel stack. */
   __thread_k_stacks_top;      /**< High address of kernel stack. */
 
   
   extern void* thread_kill;   /**< Pointer to the thread kill function. */
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
 
 /**
  * @struct interrupt_stack_frame
  * @brief Represents the stack frame pushed onto the stack during an interrupt.
  *
  * This structure contains the registers saved during an interrupt, including
  * general-purpose registers and special-purpose registers such as the program counter (PC)
  * and program status register (PSR).
  */
 typedef struct {
    uint32_t r0;   /**< General-purpose register R0. */
    uint32_t r1;   /**< General-purpose register R1. */
    uint32_t r2;   /**< General-purpose register R2. */
    uint32_t r3;   /**< General-purpose register R3. */
    uint32_t r12;  /**< General-purpose register R12. */
    uint32_t lr;   /**< Link register (LR). */
    uint32_t pc;   /**< Program counter (PC). */
    uint32_t xPSR; /**< Program status register (PSR). */
  } interrupt_stack_frame;
 
 /**
  * @struct global_threads_info_t
  * @brief Global thread information for tracking timing, priorities, and stack limits.
  */
 typedef struct global_threads_info 
 {
     uint32_t max_threads;   /**< Maximum number of threads. */
     uint32_t max_mutexes;   /**< Maximum number of mutexes. */
     uint32_t stack_size;    /**< Size of stack in words. */
     uint32_t tick_counter;  /**< System tick counter. */
     uint32_t current_thread;  /**< Index of the currently running thread. */
 
     uint32_t thread_time_left_in_C[16]; /**< Remaining computation time for each thread. */
     uint32_t thread_time_left_in_T[16]; /**< Remaining period time for each thread. */
     uint32_t thread_time[16]; /**< Absolute start time for each thread. */
 
     uint32_t ready_threads[16];  /**< Array of ready threads. */
     uint32_t waiting_threads[16]; /**< Array of waiting threads. */
     uint32_t mutex_index;
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
      DONE, //Exit
      BLOCKED //process blocked by mutex
 } thread_state_t;
 
 /**
  * @struct pushed_callee_stack_frame
  * @brief Stack frame pushed onto MSP before initiating a context switch.
  */
 /* Stack frame pushed onto MSP before intiating context switch */
 typedef struct {
    uint32_t *PSP;   /**< Pointer to the thread's Process Stack Pointer (PSP). */
    uint32_t r4;    /**< Register value for r4 */
    uint32_t r5;    /**< General-purpose register R5. */
    uint32_t r6;    /**< General-purpose register R6. */
    uint32_t r7;    /**< General-purpose register R7. */
    uint32_t r8;    /**< General-purpose register R8. */
    uint32_t r9;    /**< General-purpose register R9. */
    uint32_t r10;   /**< General-purpose register R10. */
    uint32_t r11;   /**< General-purpose register R11. */
    uint32_t lr;    /**< Link register (LR). */
 
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
    pushed_callee_stack_frame *msp;   /**< Pointer to the thread's kernel stack. */
 
    uint32_t priority;                /**< Dynamic Thread priority (0-16). */
    uint32_t computation_time;        /**< Computation time (C) in ticks. */
    uint32_t period;                  /**< Period (T) in ticks. */
    uint32_t svc_status;              /**< SVC status (privileged/unprivileged). */
 
    thread_state_t state;             /**< Current state of the thread. */
 
    uint32_t held_mutex_bitmap; /**< Bitmap of held mutexes. */
    uint32_t waiting_mutex_bitmap; /**< Bitmap of waiting mutexes. */
   
    uint8_t processed; /**< Flag indicating if the thread has been processed in current period. */
 } TCB_t;
 
 // Our TCB array which holds corresponding TCBs of our threads
 /** @brief Array of Thread Control Blocks (TCBs) for all threads. */
 TCB_t TCB_ARRAY[16];
 
 // Our global thread info struct that holds important information for timing, stack sizes, and max priorities.
 /** @brief Global structure holding thread-related information. */
 global_threads_info_t global_threads_info;
 
 #define MAX_MUTEXES 32
 #define NOT_LOCKED 0xFFFFFFFF
 /**
  * @brief global array for storing mutex information
  */
 kmutex_t mutex_array[MAX_MUTEXES];
 
 
 /**
  * @brief Pointer to the low address of the user stack.
  */
 uint32_t *u_stack_low;
 
 /**
  * @brief Pointer to the low address of the kernel stack.
  */
 uint32_t *k_stack_low;


 /**
  * 
  * SysTick Timer Expires
        ↓
systick_c_handler()
        ↓
[1] Decrement current thread's computation time
[2] Check if current thread exhausted time → WAITING
[3] Check all threads for period completion → READY
[4] pend_pendsv()
        ↓
PendSV Interrupt
        ↓
pendsv_c_handler()
        ↓
[1] Save current thread context (registers, stack)
[2] Call thread_scheduler()
        ↓
thread_scheduler()
        ↓
[1] Unblock threads no longer waiting for mutexes
[2] Set RUNNING threads to READY
[3] Find highest priority READY thread (RMS)
[4] Consider IPCP constraints (waiting_mutex_bitmap)
[5] Return selected thread ID
        ↓
[3] Load selected thread's context
[4] Set selected thread to RUNNING
[5] Return new stack pointer
        ↓
Hardware restores context and jumps to selected thread
        ↓
Selected thread continues execution

  */
 
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
   
   uint32_t max_threads = global_threads_info.max_threads;
   
   for(uint32_t i = 0; i < global_threads_info.max_threads; i++)
   {
       if(TCB_ARRAY[i].state == BLOCKED)
     {
       //update the waiting_mutex_bitmap for waiting threads
        if(TCB_ARRAY[i].waiting_mutex_bitmap == 0)
        {
          TCB_ARRAY[i].state = READY;
      
        }
    }
  }

   // Sets all to ready
   for(uint32_t i = 0; i < global_threads_info.max_threads; i++){
      if (TCB_ARRAY[i].state == RUNNING){
        TCB_ARRAY[i].state = READY;
      }
   }

 
   /**IPCP HLP */
   int highest_priority_thread = -1;
   uint32_t highest_priority = max_threads + 1; 
 
   //find the thread with highest dynamic priority
   for(uint32_t i = 0; i < max_threads; i++)
   {
       if(TCB_ARRAY[i].state == READY)
       {
         
           //check if the thread is holding any mutexes, if it is waiting for any mutexes
          
           if(TCB_ARRAY[i].priority <= highest_priority && TCB_ARRAY[i].waiting_mutex_bitmap == 0) // 
           {
               highest_priority = TCB_ARRAY[i].priority;
               highest_priority_thread = i;
           }
       }
   }

   //if a thread is found, return the highest priority thread
   if(highest_priority_thread != -1)
   {
   
       return highest_priority_thread;
       
   }
 
   //if no ready threads are found, check if there are any waiting threads
   //run idle if there are still waiting threads
   for(uint32_t i = 0; i < max_threads; i++)
   {
       if(TCB_ARRAY[i].state == WAITING || TCB_ARRAY[i].state == BLOCKED ) //|| TCB_ARRAY[i].state == BLOCKED
       {
         //run idle thread
           return max_threads;
       }
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
 
     uint32_t current_thread = global_threads_info.current_thread;
 
     // save svc_status
     uint32_t svc_stat = get_svc_status();
 
     pushed_callee_stack_frame * callee_saved_stk = context_ptr;
 
   
     TCB_t * TCB = &TCB_ARRAY[current_thread]; //tcb context automatically stores the entire context
  
     TCB->msp = callee_saved_stk;
     TCB->svc_status = svc_stat; 
     int priority = thread_scheduler();
 
     global_threads_info.current_thread = priority;
     TCB_t * next_TCB = &TCB_ARRAY[priority];
     next_TCB -> state = RUNNING;
 
     svc_stat = next_TCB -> svc_status;
     set_svc_status(svc_stat);
 
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
   global_threads_info.mutex_index = 0;
 
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
 
    for(uint32_t i = 0; i < max_threads+2; i++) {
      
      uint32_t *aligned_k_stack_top = k_stack_top - (i * global_threads_info.stack_size ) - sizeof(pushed_callee_stack_frame) / sizeof(uint32_t);
      uint32_t *aligned_u_stack_top = u_stack_top - (i * global_threads_info.stack_size ) - sizeof(interrupt_stack_frame) / sizeof(uint32_t);
      
      TCB_ARRAY[i].msp = (pushed_callee_stack_frame *)aligned_k_stack_top;
      TCB_ARRAY[i].msp->PSP = aligned_u_stack_top;
      TCB_ARRAY[i].state = NEW; //Q: what state should the thread be set to in init?
      TCB_ARRAY[i].svc_status = 0; // Might want to set all svc_status to 0?
    
      TCB_ARRAY[i].held_mutex_bitmap = 0;
      TCB_ARRAY[i].waiting_mutex_bitmap = 0;  
     }
 
   //Initialize the idle thread
  //assuming max value for max_threads is 14
  
  if (idle_fn == NULL) {
   idle_fn = default_idle_fn; // Use default idle function for the user input null idle fn
 }
   
   //  set the default thread to be running
 
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
    //global_threads_info.thread_time_left_in_T[prio_idle] = 1;
    global_threads_info.thread_time_left_in_C[prio_idle] = 1;
 
    // Initiating default thread values
    int prio_default = max_threads + 1;
    TCB_ARRAY[prio_default].computation_time = 0x1;
    TCB_ARRAY[prio_default].period = 0x1;
    TCB_ARRAY[prio_default].priority = prio_default;
    TCB_ARRAY[prio_default].state = RUNNING;
    TCB_ARRAY[prio_default].svc_status = 0; //clown moment
 
    global_threads_info.thread_time[prio_default] = 0;
    global_threads_info.thread_time_left_in_C[prio_default] = 1;
 
    for(uint32_t i = 0; i < max_threads; i++){
      global_threads_info.ready_threads[i] = 400;
      global_threads_info.waiting_threads[i] = 400;
    }
 
   global_threads_info.current_thread = prio_default;
 
   // Initialize the mutex array
   initialize_mutex_array();
 
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
    global_threads_info.thread_time_left_in_C[prio] = C;
   
 
    return 0;
 }
 
 /**
  * @brief Starts the scheduler.
  *
  * Initializes the SysTick timer and triggers the PendSV interrupt to start
  * scheduling threads.
  *
  * @param[in] frequency Frequency of the SysTick timer.
  * @return 0 on success.
  */
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
  
   //Return the current thread's dynamic priority
   return TCB_ARRAY[global_threads_info.current_thread].priority;
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
   int thread = global_threads_info.current_thread;
   int abs_thread_time = global_threads_info.thread_time[thread];
   return abs_thread_time;
 }
 
 /**
  * @brief Permanently deschedules the currently running thread.
  *
  * Transitions the thread to the DONE state and selects the next thread to run.
  */
 void sys_thread_kill(){
   uint32_t current_thread = global_threads_info.current_thread;
   if (current_thread == global_threads_info.max_threads+1){ // Default Thread Is Called It
     sys_exit(1);
   }
   else if(current_thread == global_threads_info.max_threads){// Idle Thread is Calling It 
     // run default idle
     interrupt_stack_frame * psp = (interrupt_stack_frame *)(TCB_ARRAY[global_threads_info.max_threads].msp -> PSP);
     psp -> pc = (uint32_t) &default_idle_fn;
   }
   else{
     TCB_ARRAY[current_thread].state = DONE;
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
 void sys_wait_until_next_period(){ 
   uint32_t current_thread = global_threads_info.current_thread;
   //check if the thread is holding mutex
   if(current_thread == global_threads_info.max_threads)
   {
      printk("Warning: Idle thread call in wait til next period\n");
     return;
   }
 
   TCB_ARRAY[current_thread].state = WAITING;
   pend_pendsv();
 }
 
 void initialize_mutex_array(){
   for(uint32_t i = 0; i < MAX_MUTEXES; i++){
       mutex_array[i].locked_by = NOT_LOCKED;
       mutex_array[i].prio_ceil = 0;
       mutex_array[i].index = i;
   }
 }
 /**
  * @brief Initializes a mutex.
  *
  * This function initializes a mutex with the specified maximum priority.
  *
  * @param[in] max_prio Maximum priority for the mutex.
  * @return Pointer to the initialized mutex.
  */
 kmutex_t *sys_mutex_init( uint32_t max_prio ) {
    
    uint32_t mutex_num = global_threads_info.mutex_index;
     if (mutex_array[mutex_num].locked_by == NOT_LOCKED ){
       mutex_array[mutex_num].locked_by = NOT_LOCKED;
       mutex_array[mutex_num].prio_ceil = max_prio;
       mutex_array[mutex_num].index = mutex_num;
       global_threads_info.mutex_index = global_threads_info.mutex_index + 1;
       return &mutex_array[mutex_num];
     }
  
   return NULL;
 }
 
 /**
  * @brief Locks a mutex.
  *
  * This function locks the specified mutex, blocking if the mutex is already locked.
  * checks if the calling thread has sufficient priority to acquire the mutex
  *
  * @param[in] mutex Pointer to the mutex to lock.
  */
 void sys_mutex_lock( kmutex_t *mutex ) {
 
   uint32_t current_thread = global_threads_info.current_thread;

    if(current_thread == global_threads_info.max_threads)
   {
     
     return;
   }

    // Check if the current thread's priority is less than or equal to the mutex's priority ceiling
    if (current_thread < mutex->prio_ceil) {
     printk("Warning: Thread %d cannot lock mutex %d because (%d) high priority(%d)\n",
            current_thread, mutex->index, TCB_ARRAY[current_thread].priority, mutex->prio_ceil);
         sys_thread_kill();
         return; 
 }
 
   // Check if the thread is trying to lock a mutex it already holds
   if (TCB_ARRAY[current_thread].held_mutex_bitmap & (1 << mutex->index)) {
     printk("Warning: Thread %d is trying to lock mutex %d again (double lock)\n", current_thread, mutex->index);
     return;
   }
 
   //check if the the mutex is locked, if it is locked, we return to avoid blocking
   if(mutex->locked_by != NOT_LOCKED)
   {
       TCB_ARRAY[current_thread].state = BLOCKED;

       //Add the mutex to the waiting_mutex_bitmap
       TCB_ARRAY[current_thread].waiting_mutex_bitmap = TCB_ARRAY[current_thread].waiting_mutex_bitmap | (1 << mutex->index); 
       //trigger context switch while waiting
       
       printk("Time:  Thread %d is blocked trying to lock %d\n",  current_thread, mutex->index);
       pend_pendsv(); 
       
        while (mutex->locked_by != NOT_LOCKED){}
        mutex->locked_by = current_thread;

        //raise the thread's priority to the mutex's priority ceiling
        if(TCB_ARRAY[current_thread].priority > mutex->prio_ceil)
        {
          TCB_ARRAY[current_thread].priority = mutex->prio_ceil;
        }
      
        //set the bit corresponding to the mutex index in the held_mutex_bitmap
        TCB_ARRAY[current_thread].held_mutex_bitmap = TCB_ARRAY[current_thread].held_mutex_bitmap | (1 << mutex->index); 
        //clear the bit corresponding to the mutex index in the waiting_mutex_bitmap
        TCB_ARRAY[current_thread].waiting_mutex_bitmap = TCB_ARRAY[current_thread].waiting_mutex_bitmap &  ~(1 << mutex->index); 
            
      return;
   }

   for (uint32_t i = 0; i < global_threads_info.max_mutexes; i++) {
    if ((mutex_array[i].locked_by != NOT_LOCKED) && (mutex_array[i].prio_ceil <= TCB_ARRAY[current_thread].priority)) {

      
      if(i == mutex->index || mutex_array[i].locked_by == current_thread){continue;}
        // If the mutex is already locked by another thread with a higher priority ceiling, block the current thread
        // In otherwords its a check about scheduler working properly
        // Its getting hung with the fact that this thread is calling lock on mutex with ceiling 1 and then 0
      
        printk("Warning: Thread %d cannot lock mutex %d because another thread holds a mutex with a higher prio ceiling: %d.\n",
               current_thread, mutex->index, i);
        return;
    }
  }
   
 
   //lock the mutex 
   mutex->locked_by = current_thread;

   //raise the thread's priority to the mutex's priority ceiling
   if(TCB_ARRAY[current_thread].priority > mutex->prio_ceil)
   {
      TCB_ARRAY[current_thread].priority = mutex->prio_ceil;
   }
 
   //set the bit corresponding to the mutex index in the held_mutex_bitmap
   TCB_ARRAY[current_thread].held_mutex_bitmap = TCB_ARRAY[current_thread].held_mutex_bitmap | (1 << mutex->index); 
   //clear the bit corresponding to the mutex index in the waiting_mutex_bitmap
   TCB_ARRAY[current_thread].waiting_mutex_bitmap = TCB_ARRAY[current_thread].waiting_mutex_bitmap &  ~(1 << mutex->index); 

 }
 
 /**
  * @brief Unlocks a mutex.
  *
  * This function unlocks the specified mutex, allowing other threads to acquire it.
  *
  * @param[in] mutex Pointer to the mutex to unlock.
  */
 void sys_mutex_unlock( kmutex_t *mutex ) {
  
    
   uint32_t current_thread = global_threads_info.current_thread;

   if(mutex->locked_by == NOT_LOCKED)
   {
     printk("Warning: Thread %d is trying to unlock an already unlocked mutex %d\n", current_thread, mutex->index);
     return;
   }
   if(mutex->locked_by != current_thread)
   {
     printk("Warning: Thread %d is trying to unlock mutex %d that it does not own\n", current_thread, mutex->index);
     return;
   }

   
 
   // Check if the mutex is already unlocked
   if (!(TCB_ARRAY[current_thread].held_mutex_bitmap & (1 << mutex->index))) {
     printk("Warning:  (double unlock)\n");
     return;
   }
 
   //unlock the mutex
   mutex->locked_by = NOT_LOCKED;

   //clear the bit corresponding to the mutex index in the held_mutex_bitmap
   TCB_ARRAY[current_thread].held_mutex_bitmap = TCB_ARRAY[current_thread].held_mutex_bitmap & ~(1 << mutex->index); 
 
   //restore the thread's priority to its original value
   //the index of the current thread in equal to its static priority
   uint32_t new_priority = current_thread;
   for (uint32_t i = 0; i < MAX_MUTEXES; i++)
   {
     //check if the thread is holding any other mutexes
     //if the priority ceiling of the held mutex is higher, set new priority to that
     
     if(TCB_ARRAY[current_thread].held_mutex_bitmap & (1 << i))
     {
       if(mutex_array[i].prio_ceil < new_priority)
       {
         new_priority = mutex_array[i].prio_ceil;
       }
     }
   }
 
   //update the dynamic priority of the thread
   TCB_ARRAY[current_thread].priority = new_priority;

   //If there are any threads waiting for the mutex, wake them up
   for(uint32_t i = 0; i < global_threads_info.max_threads; i++)
   {

       if(TCB_ARRAY[i].state == BLOCKED )
     {
       //update the waiting_mutex_bitmap for waiting threads
        TCB_ARRAY[i].waiting_mutex_bitmap = TCB_ARRAY[i].waiting_mutex_bitmap & ~(1 << mutex->index);
      
       
    }
  }
  pend_pendsv();
 }

extern uint32_t total_count;

 /**
 * @brief SysTick interrupt handler.
 *
 * This function is called every time the SysTick timer expires. It increments the
 * total tick count, sets the SysTick flag, and triggers a PendSV interrupt for scheduling.
 */


void systick_c_handler() {
  
  total_count = total_count + 1;

  int curr_running = global_threads_info.current_thread;  
  int max_threads = global_threads_info.max_threads;
  global_threads_info.thread_time[curr_running] += 1;
  if (curr_running != max_threads && curr_running != max_threads + 1){
    int time_left_in_compute = global_threads_info.thread_time_left_in_C[curr_running] - 1;
    
    if (time_left_in_compute == 0){
      // Thread finished its computation time
     if (TCB_ARRAY[curr_running].held_mutex_bitmap != 0) {
       printk("Warning: Thread %d is holding a mutex and has finished computation time. \n", curr_running);
   }

   time_left_in_compute = TCB_ARRAY[curr_running].computation_time;
   TCB_ARRAY[curr_running].state = WAITING;
      
    }
    global_threads_info.thread_time_left_in_C[curr_running] = time_left_in_compute;
  }

  //Check for Period Completions
  for (uint32_t i = 0; i < global_threads_info.max_threads; i ++){
    if (TCB_ARRAY[i].state == READY || TCB_ARRAY[i].state == WAITING || TCB_ARRAY[i].state == RUNNING){
      
      if (sys_get_time() % TCB_ARRAY[i].period == 0){
       // Thread's new period starts - release the thread
          global_threads_info.thread_time_left_in_C[i] = TCB_ARRAY[i].computation_time;   
         TCB_ARRAY[i].state = READY;
      }
      
    }
  }
  pend_pendsv();
}