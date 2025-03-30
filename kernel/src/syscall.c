/**
 * @file syscall.c
 *
 * @brief system call functions to be used in user programs to interact with critical resources     
 *
 * @date  March 16 2025    
 *
 * @author Mario Cruz and Charlie Ai
 */

#include <unistd.h>
#include <syscall.h>
#include <uart.h>
#include <printk.h>
#include <nvic.h>
#include <arm.h>

/**
 * @brief Attribute to mark unused function parameters.
 */
#define UNUSED __attribute__((unused))

/**
 * @brief Pointer to the start of the heap.
 */
extern char __heap_low;

/**
 * @brief Pointer to the end of the heap.
 */
extern char __heap_top;

/**
 * @brief Current program break (end of the heap).
 */
static void* curr_program_break = &__heap_low;

/**
 * @brief Adjusts the program break to allocate or deallocate memory.
 *
 * This function shifts the program break to allocate or deallocate memory
 * for the calling process. It ensures that the program break does not exceed
 * the heap boundaries.
 *
 * @param[in] incr The number of bytes to increase or decrease the program break.
 * @return A pointer to the previous program break on success, or `(void*)-1` if the allocation fails.
 */
void *sys_sbrk(int incr){
  void *previous_heap_end = curr_program_break;
  
  if (incr + (char*)curr_program_break >= (char*)&__heap_top){
    return (void*)-1;
  }

  curr_program_break = (char*)curr_program_break + incr;

  return (void*)previous_heap_end;
}

/**
 * @brief Writes data to STDOUT.
 *
 * This function writes the specified number of bytes from the buffer to
 * the standard output (UART). It blocks until all bytes are written.
 *
 * @param[in] file The file descriptor (must be 1 for STDOUT).
 * @param[in] ptr Pointer to the buffer containing the data to write.
 * @param[in] len The number of bytes to write.
 * @return The number of bytes written on success, or -1 if the file descriptor is invalid.
 */
int sys_write(int file, char *ptr, int len){
  if (file != 1) {return -1;}
  
 for(int i = 0; i < len; i++){
   while(uart_put_byte(ptr[i]) );
 }
  return len;
}

/**
 * @brief Reads data from STDIN and echoes it back.
 *
 * This function reads up to the specified number of bytes from the standard
 * input (UART) into the buffer. It echoes the input back to the standard output.
 *
 * @param[in] file The file descriptor (must be 0 for STDIN).
 * @param[out] ptr Pointer to the buffer where the input data will be stored.
 * @param[in] len The maximum number of bytes to read.
 * @return The number of bytes read on success, or -1 if the file descriptor is invalid.
 */
int sys_read(int file, char *ptr, int len){
  if (file != 0) {return -1;} /* Case For Not Reading From STDIN*/
  char c[2];
  c[0] = 'M';
  c[1] = '\0';
  int curr_ind = 0;
  while (curr_ind < len){
    if (uart_get_byte(c) < 0){
      continue;
    };

    /* End Of Trans Character */
    if (c[0] == 4){ return curr_ind;} 

    /* Backspace Character */
    else if (c[0] == '\b'){
      if (curr_ind > 0){
        curr_ind -= 1;}
        printk("\b \b");
        continue;
      }


    /* New Line */
    else if (c[0] == '\n'){
      ptr[curr_ind] = '\n';
      printk(c);
      curr_ind ++;
      return curr_ind;}
    
    /* Regular Character */
    else {
      ptr[curr_ind] = c[0];
      printk(c);}

    curr_ind ++;
  }
  return len;
}

/**
 * @brief Terminates the program.
 *
 * This function terminates the program by printing the exit status,
 * flushing the UART buffer, and disabling interrupts. It enters an
 * infinite loop to halt execution.
 *
 * @param[in] status The exit status code.
 */
void sys_exit(int status){
  printk("Exit status: %d\n", status);
  uart_flush();
  disable_interrupts();
  while (1) {
  }
}