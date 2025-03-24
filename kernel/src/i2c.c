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
    volatile u_int32_t CR1; // Control Register 1
    volatile u_int32_t CR2; // Control Register 2
    volatile u_int32_t OAR1; // Own Address Register 1
    volatile u_int32_t OAR2; // Own Address Register 2
    volatile u_int32_t DR; // Data Register
    volatile u_int32_t SR1; // Status Register 1
    volatile u_int32_t SR2; // Status Register 2
    volatile u_int32_t CCR; // Clock Control Register
    volatile u_int32_t TRISER; // T RISE Register (Maximum rise time in Fm/Sm mode (Master mode))
    volatile u_int32_t FLTR;  // Noise Filter Register
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

/*
 * i2c_master_init: I2C initialization function
 * 
 * Sets B8 and B9 to SCL and SDA respectively
 * 
 * clk -- Any 16 bit value
 * 
 * Note: clk input is not used as we can hardcode the rate
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

/*
 * i2c_master_start: I2C start function
 */
void i2c_master_start() {
    struct i2c_reg_map * i2c = I2C_BASE_ADDRESS;
    i2c -> CR1 |= I2C_START;
    while ((i2c -> SR1 & (I2C_CHECK_SB_EV5)) == 0){}
    return;
}

/*
 * i2c_master_stop: I2C stop function
 */
void i2c_master_stop() {
    struct i2c_reg_map * i2c = I2C_BASE_ADDRESS;
    i2c -> CR1 |= I2C_STOP;
    return;
}

/*
 * i2c_master_write: I2C write from master to slave function
 * 
 * buf -- pointer to array of vals that should be sent to slave
 * 
 * len -- length of the array of vals
 * 
 * slave_addr -- address of slave
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

/** @brief Note: not fully implemented */
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
