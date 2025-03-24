/**
 * @file uart.c
 *
 * @brief Interrupt-driven UART implementation. Similar to a stream buffer.
 *
 * @date March 22, 2025
 *
 * @author Mario Cruz
 */

#include <rcc.h>
#include <uart.h>
#include <nvic.h>
#include <gpio.h>
#include <arm.h>

/** @brief The UART register map. */
struct uart_reg_map {
    volatile uint32_t SR;   /**< Status Register */
    volatile uint32_t DR;   /**<  Data Register */
    volatile uint32_t BRR;  /**<  Baud Rate Register */
    volatile uint32_t CR1;  /**<  Control Register 1 */
    volatile uint32_t CR2;  /**<  Control Register 2 */
    volatile uint32_t CR3;  /**<  Control Register 3 */
    volatile uint32_t GTPR; /**<  Guard Time and Prescaler Register */
};

/** @brief Base address for UART2 */
#define UART2_BASE  (struct uart_reg_map *) 0x40004400

/** @brief Enable  Bit for UART Config register */
#define UART_EN (1 << 13)

/** @brief Enable  Bit for UART Clock */
#define UARTCLOCK_EN (1 << 17)

/** @brief Enable Bit for UART Transmit */
#define TX_EN (1 << 3)

/** @brief Enable Bit for UART Receive */
#define RX_EN (1 << 2)

/** @brief Transmit Speed Setting Val*/
#define USARTDIV (0x8 << 4 | 0xB)

/** @brief Transmit Ready Bit in UART Status Register */
#define SR_TRANSMITREADY (1 << 7)

/** @brief Receive Ready Bit in UART Status Register */
#define SR_RECEIVEREADY (1 << 5)

#define CR1_TXEIE (1 << 7)

#define CR1_RXNEIE (1 << 5)

#define UNUSED __attribute__((unused))

#define size_of_Queue (16)

typedef struct Queue{
  char array[size_of_Queue]; 
  uint32_t tail;
  uint32_t header;
  uint32_t count;
}Queue;

Queue TransmitBuffer;
Queue ReceiveBuffer;

void initBuffer(){
  TransmitBuffer.tail = 0;
  TransmitBuffer.header = 0;
  TransmitBuffer.count = 0;

  ReceiveBuffer.tail = 0;
  ReceiveBuffer.header = 0;
  ReceiveBuffer.count = 0;
};

void enqueue(char c, Queue * buffer){
  if (buffer -> count >= size_of_Queue){
    return;
  }
  buffer->count = buffer-> count + 1;
  buffer->array[buffer ->header] = c;
  buffer -> header = (buffer -> header + 1) % size_of_Queue;
  return;
}

char dequeue(Queue * buffer){
  char c;
  buffer -> count = buffer ->count - 1;
  c = buffer -> array[buffer ->tail];
  buffer -> tail = (buffer -> tail + 1) % size_of_Queue;
  return c;
}

void uart_init(int baud){
  (void) baud; /* Here to suppress the Unused Variable Error. Baud is hardcoded*/
                
  struct rcc_reg_map *rcc = RCC_BASE;
  rcc -> apb1_enr |= UARTCLOCK_EN;

  struct uart_reg_map *uart = UART2_BASE;

  uart -> CR1 |= TX_EN | RX_EN | CR1_RXNEIE;
  uart -> CR1 |= UART_EN;

  /* Note: UARTDIV is 8.681 */
  uart -> BRR = (uart -> BRR & 0xFFFF0000) | USARTDIV;

  initBuffer();

  nvic_irq(38, IRQ_ENABLE);

  /* Initializing Port A Pin 2 and Pin 3 which act as TX and RX Respectively*/
  gpio_init(GPIO_A, 2 , MODE_ALT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);
  gpio_init(GPIO_A, 3, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);


  return;

}

int uart_put_byte(char c){
  int state = save_interrupt_state_and_disable(); /* Logic for enabling or not*/

  if (TransmitBuffer.count == size_of_Queue){
    restore_interrupt_state(state);
    return -1;
  }

  struct uart_reg_map *uart = UART2_BASE;
  if (TransmitBuffer.count == 0){
    enqueue(c, &TransmitBuffer);

    if (!state){
      uart -> CR1 |= CR1_TXEIE;
    }

  }
  else{
    enqueue(c, &TransmitBuffer);
  }

  restore_interrupt_state(state);
  return 0;
  
  
}

void USART2_TX_IRQHandler() {
  struct uart_reg_map *uart = UART2_BASE;
  char c;
  if (TransmitBuffer.count == 1){
    c = dequeue(&TransmitBuffer);
    uart -> CR1 &= ~CR1_TXEIE;
  }
  else{
    c = dequeue(&TransmitBuffer);
  }
  uart -> DR = c;
  return;
}

int uart_get_byte(char *c){
  int state = save_interrupt_state_and_disable(); /* Logic for enabling or not*/

  if (ReceiveBuffer.count == 0){
    restore_interrupt_state(state);
    return -1;
  }
  
  struct uart_reg_map *uart = UART2_BASE;
  char str;
  if (ReceiveBuffer.count == size_of_Queue){
    str = dequeue(&ReceiveBuffer);

    
    if (!state){
    uart -> CR1 |= CR1_RXNEIE;
    }

  }
  else{
    str = dequeue(&ReceiveBuffer);
  }
  *c = str;

  restore_interrupt_state(state);
  return 0;
}

void USART2_RX_IRQHandler() {
  struct uart_reg_map *uart = UART2_BASE;

  char c = uart -> DR & 0xFF;

  if (ReceiveBuffer.count == size_of_Queue - 1){
    enqueue(c, &ReceiveBuffer);
    uart->CR1 &= ~CR1_RXNEIE;
  }
  else{
    enqueue(c, &ReceiveBuffer);
  }
  return;
}

void USART2_IRQHandler() {
  struct uart_reg_map *uart = UART2_BASE;
  int execute_RX = (uart -> SR & SR_RECEIVEREADY) && (ReceiveBuffer.count < size_of_Queue);
  int execute_TX = (uart -> SR & SR_TRANSMITREADY) && (TransmitBuffer.count > 0);
  if (execute_RX && execute_TX){
    USART2_RX_IRQHandler();
    USART2_TX_IRQHandler();
  }
  else if(execute_TX){
    USART2_TX_IRQHandler();
  }
  else {
    USART2_RX_IRQHandler();
    }

  nvic_clear_pending(38);  
}

void uart_flush(){
  struct uart_reg_map *uart = UART2_BASE;

  ReceiveBuffer.count = 0;
  ReceiveBuffer.header = 0;
  ReceiveBuffer.tail = 0;
  
  TransmitBuffer.count = 0;
  TransmitBuffer.header = 0;
  TransmitBuffer.tail = 0;

  uart -> CR1 &= ~CR1_RXNEIE;
  uart -> CR1 &= ~CR1_TXEIE;
}