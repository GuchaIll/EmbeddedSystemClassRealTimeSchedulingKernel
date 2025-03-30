/**
 * @file i2c.c
 * 
 * @brief contains functions for initializing and doing I2C communication for STM32 peripherals
 * 
 * @date   Febuary 20 2025
 * 
 * @author Mario Cruz | Charlie Ai <macruz | yacil>
 */

#include <unistd.h>
#include <gpio.h>
#include <i2c.h>
#include <rcc.h>

/** @brief Defining the i2c register map */
struct i2c_reg_map {
    volatile u_int32_t CR1; /**< Control Register 1. */
    volatile u_int32_t CR2; /**< Control Register 2. */
    volatile u_int32_t OAR1; /**< Own Address Register 1. */
    volatile u_int32_t OAR2; /**< Own Address Register 2. */
    volatile u_int32_t DR; /**< Data Register. */
    volatile u_int32_t SR1; /**< Status Register 1. */
    volatile u_int32_t SR2; /**< Status Register 2. */
    volatile u_int32_t CCR; /**< Clock Control Register. */
    volatile u_int32_t TRISER; /**< Maximum rise time register in Fm/Sm mode (Master mode). */
    volatile u_int32_t FLTR; /**< Noise Filter Register. */
};

/** @brief RCC I2C1 Enable*/
#define I2C1_EN (1 << 21)

/** @brief I2C1 Base Address*/
#define I2C_BASE_ADDRESS (struct i2c_reg_map*) 0x40005400

/** @brief Acknowledge Bit Enable*/
#define I2C_ACK_EN (1 << 10)

/** @brief Start Condition Which Switches to Master */
#define I2C_START (1 << 8)

/** @brief STOP Condition Which Switches to Slave*/
#define I2C_STOP (1 << 9)

/** @brief Setting Peripheral Clock to 16MHz */
#define I2C_16MHz_EN (1 << 4)

/** @brief Setting CCR Value 160 (0xA0): 
 * 1/2 * 1/100khz = CCR * 1/16Mhz
 * CRR is 160
*/
#define I2C_SET_CCR (0xA0)

/** @brief Simply Used for Accessing Random Bit in SR2 */
#define I2C_CHECK_TRA (1 << 2)

/** @brief BTF in SR1*/
#define I2C_CHECK_BTF (1 << 2)

/** @brief  ADDR bit in SR1*/
#define I2C_CHECK_ADDR (1 << 1)

/** @brief Enable I2C for Peripherals */
#define I2C_EN_PERIPHERAL (1)

/** @brief SB in SR1 */
#define I2C_CHECK_SB_EV5 (1 << 0)

/** @brief TXE Bit in SR1 */
#define I2C_CHECK_TXE_EV8 (1 << 7)


 /**
 * @brief Initializes the I2C peripheral.
 *
 * Configures the GPIO pins for I2C communication, sets the clock rate, and enables the I2C peripheral.
 * i2c_master_init: I2C initialization function
 * Sets B8 and B9 to SCL and SDA respectively
 * clk -- Any 16 bit value
 * Note: clk input is not used as we can hardcode the rate
 * 
 * @param[in] clk The clock rate (not used, hardcoded in the implementation).
 */
void i2c_master_init(uint16_t clk){
    (void) clk; /* Supressing unused variable error since we can hardcode I2C clock rate */
               
    gpio_init(GPIO_B, 8, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_HIGH, PUPD_NONE, ALT4); // D15 is SCL
    gpio_init(GPIO_B, 9, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_HIGH, PUPD_NONE, ALT4); // D14 is SDA

    struct rcc_reg_map * rcc = RCC_BASE;
    rcc -> apb1_enr |= I2C1_EN;

    struct i2c_reg_map * i2c = I2C_BASE_ADDRESS;
    i2c -> CCR |= I2C_SET_CCR;
    i2c -> CR2 |= I2C_16MHz_EN;
    i2c -> CR1 |= I2C_ACK_EN;
    i2c -> CR1 |= I2C_EN_PERIPHERAL;

    return;
}

/**
 * @brief Sends a START condition on the I2C bus.
 *
 * This function initiates communication by sending a START condition and waits until the START condition is acknowledged.
 */
void i2c_master_start() {
    struct i2c_reg_map * i2c = I2C_BASE_ADDRESS;
    i2c -> CR1 |= I2C_START;
    while ((i2c -> SR1 & (I2C_CHECK_SB_EV5)) == 0){}
    return;
}

/**
 * @brief Sends a STOP condition on the I2C bus.
 *
 * This function terminates communication by sending a STOP condition.
 */
void i2c_master_stop() {
    struct i2c_reg_map * i2c = I2C_BASE_ADDRESS;
    i2c -> CR1 |= I2C_STOP;
    return;
}


 /**
 * @brief Writes data from the master to a slave device on the I2C bus.
 *
 * Sends the specified buffer of data to the slave device at the given address.
 *
 * @param[in] buf Pointer to the buffer containing the data to send.
 * @param[in] len Length of the buffer.
 * @param[in] slave_addr Address of the slave device.
 * @return 0 on success.
 */
int i2c_master_write(uint8_t *buf, uint16_t len, uint8_t slave_addr){
    struct i2c_reg_map * i2c = I2C_BASE_ADDRESS;

    i2c_master_start();
    i2c -> DR = (slave_addr << 1) & 0xFE; 

    while ((i2c -> SR1 & (I2C_CHECK_ADDR)) == 0){ };
    /* Note: Simply have to access SR2 in anyway we want so we discard n*/
    int n = (i2c -> SR2 & I2C_CHECK_TRA);
    (void) n;

    for (int i = 0; i < len; i ++){
        while((i2c -> SR1 & (I2C_CHECK_TXE_EV8)) == 0){ };
        i2c -> DR = buf[i];
    }

    while((!(i2c -> SR1 & I2C_CHECK_BTF) || !(i2c -> SR1 & I2C_CHECK_TXE_EV8))){ } 

    i2c_master_stop();
    return 0;
}

/**
 * @brief Reads data from a slave device on the I2C bus.
 *
 * Reads the specified number of bytes from the slave device into the provided buffer.
 *
 * @param[out] buf Pointer to the buffer to store the received data.
 * @param[in] len Number of bytes to read.
 * @param[in] slave_addr Address of the slave device.
 * @return The first byte of the received data.
 */
int i2c_master_read(uint8_t *buf, uint16_t len, uint8_t slave_addr){
    struct i2c_reg_map * i2c = I2C_BASE_ADDRESS;
    i2c_master_start();
    while ((i2c -> SR1 & (I2C_CHECK_TXE_EV8)) == 0){};
    i2c -> DR = slave_addr |= 0x01; 
    while ((i2c -> SR1 & (I2C_CHECK_ADDR)) == 0){};
    int n = (i2c -> SR2 & I2C_CHECK_TRA);
    (void)n;
    for (int i = 0; i < len; i += 8){
        buf[i] = i2c -> DR;
        while((i2c -> SR1 & (1<<6)) == 0){};
    };
    while((i2c -> SR1 & I2C_CHECK_BTF) == 0){};
    i2c_master_stop();
    return *buf;
}
