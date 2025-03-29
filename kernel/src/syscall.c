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


#define UNUSED __attribute__((unused))

extern char __heap_low;
extern char __heap_top;

static void* curr_program_break = &__heap_low;

/* Shifts Heap Pointers To Unused Memory*/
void *sys_sbrk(int incr){
  void *previous_heap_end = curr_program_break;
  
  if (incr + (char*)curr_program_break >= (char*)&__heap_top){
    return (void*)-1;
  }

  curr_program_break = (char*)curr_program_break + incr;

  return (void*)previous_heap_end;
}

/* Writes to STDOUT */
int sys_write(int file, char *ptr, int len){
  if (file != 1) {return -1;}
  //int curr_ind = 0;
  //while (curr_ind < len){
   // if (0 == uart_put_byte(ptr[curr_ind])){
  //    curr_ind ++;
  //  }
 // }
 for(int i = 0; i < len; i++){
   while(uart_put_byte(ptr[i]) );
 }
  return len;
}

/* Reads From STDIN and Echos Back */
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

/* Exit Program */
void sys_exit(int status){
  printk("Exit status: %d\n", status);
  uart_flush();
  uart_put_byte(status);

  nvic_irq(38, IRQ_DISABLE);
  while (1) {
  }
}