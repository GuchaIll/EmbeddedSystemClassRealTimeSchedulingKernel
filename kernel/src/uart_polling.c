/**
 * @file uart_polling.c
 * 
 * @brief contains functions for initializing and doing UART communication for STM32 peripherals
 * 
 * @date   Febuary 20 2025
 * 
 * @author Mario Cruz | Charlie Ai <macruz | yacil>
 */

#include <unistd.h>
#include <gpio.h>
#include <rcc.h>
#include <uart_polling.h>

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

/**
 * @brief initializes UART to given baud rate with 8-bit word length, 1 stop bit, 0 parity bits
 *
 * @param baud Baud rate -- not needed since we can hardcode this val
 */
void uart_polling_init (int baud){
    (void) baud; /* Here to suppress the Unused Variable Error. Baud is hardcoded*/
                
    struct rcc_reg_map *rcc = RCC_BASE;
    rcc -> apb1_enr |= UARTCLOCK_EN;

    struct uart_reg_map *uart = UART2_BASE;
    uart->CR1 |= UART_EN | TX_EN | RX_EN;

    /* Note: UARTDIV is 8.681 */
    uart -> BRR = (uart -> BRR && 0xFFFF0000) | USARTDIV;

    /* Initializing Port A Pin 2 and Pin 3 which act as TX and RX Respectively*/
    gpio_init(GPIO_A, 2 , MODE_ALT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);
    gpio_init(GPIO_A, 3, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);

    return;
}

/**
 * @brief transmits a byte over UART
 *
 * @param c character to be sent
 */
void uart_polling_put_byte (char c){
    struct uart_reg_map *uart = UART2_BASE;
    while ((uart -> SR & SR_TRANSMITREADY) == 0){ } 
    uart -> DR = c & 0xFF;
    return;
}

/**
 * @brief receives a byte over UART
 */
char uart_polling_get_byte () {
    struct uart_reg_map *uart= UART2_BASE;
    while ((uart -> SR & SR_RECEIVEREADY) == 0){ }
    return uart -> DR & 0xFF;
}