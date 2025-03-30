/**
 * @file lcd_driver.c
 * 
 * @brief contains functions for initiating and interacting with LCD screen
 * 
 * @date   Febuary 20 2025
 * 
 * @author Mario Cruz | Charlie Ai <macruz | yacil>
 */

#include <unistd.h>
#include <i2c.h>
#include <lcd_driver.h>
#include <systick.h>

/* Some LCD to PCF connection information for when sending 8 bit values through I2C:

    P0 (connected to RS on the LCD) toggles between instruction mode and data mode. RS=0 indicates that you
        are sending an instruction (clearing the display, moving cursor, etc.). RS=1 indicates you are writing data
        (sending characters to display).

    P1 (connected to RW on the LCD) should always be set to 0.
    
    P2 (connected to E on the LCD) needs to be pulsed (set then unset after a 4 bit packet is sent).

    P3 (not shown in figure) is connected to an internal transistor controlling the LCD backlight and should always
        be set as 1.

    P4-P7 (connected to D4-D7 on the LCD) controls the data bits for sending instructions and characters to the
        LCD. The LCD takes xinstructions and displays characters in 8 bits, but you can only write 4 bits of data
        at a time (see Figure 3). The PCF8574 expects a 4 bit packet to be sent once with E=1 followed by the same
        4 bit packet with E=0.
*/

/**
 * @brief I2C slave address for the LCD display.
 */
#define SLAVE_ADDR 0b01001110

/**
 * @brief Initializes the LCD display.
 *
 * This function configures the LCD display by sending initialization commands
 * over the I2C bus. It sets the display mode, clears the screen, and prepares
 * the LCD for operation.
 */
void lcd_driver_init() {
    uint8_t P0 = 0x0;
    uint8_t P1 = 0x0;
    uint8_t P3 = 0x1;

    /* the four instructions for LCD initialization */
    uint8_t instructions[4] = {0x30, 0x30, 0x30, 0x20}; 

    uint8_t P2, P4, P5, P6, P7;
    uint8_t PCFInstruction[4];

    /* Note: unshifting slave addr which has been shifted left one bit */
    uint8_t shiftedSlaveAddress = SLAVE_ADDR >> 1;

   
    for(uint8_t i = 0; i < 4; i++)
    {
        P2 = 0x1;
        //Extract upper 4 bits of instruction
        P4 = instructions[i]>>4 & 0x1;
        P5 = (instructions[i] >> 5) & 0x1;
        P6 = (instructions[i] >> 6) & 0x1;
        P7 = (instructions[i] >> 7) & 0x1;

        PCFInstruction[0] = (P0 << 0) | (P1 << 1) | (P2 << 2) | (P3 << 3) | (P4 << 4) | (P5 << 5) | (P6 << 6) | (P7 << 7);

        P2 = 0x0;
        PCFInstruction[1] = (P0 << 0) | (P1 << 1) | (P2 << 2) | (P3 << 3) | (P4 << 4) | (P5 << 5) | (P6 << 6) | (P7 << 7);
       
        //Extract lower 4 bits of instruction
        P4 = instructions[i] & 0x1;
        P5 = (instructions[i] >> 1) & 0x1;
        P6 = (instructions[i] >> 2) & 0x1;
        P7 = (instructions[i] >> 3) & 0x1;

        P2 = 0x1;
        PCFInstruction[2] = (P0 << 0) | (P1 << 1) | (P2 << 2) | (P3 << 3) | (P4 << 4) | (P5 << 5) | (P6 << 6) | (P7 << 7);

        P2 = 0x0;
        PCFInstruction[3] = (P0 << 0) | (P1 << 1) | (P2 << 2) | (P3 << 3) | (P4 << 4) | (P5 << 5) | (P6 << 6) | (P7 << 7);
       
        i2c_master_write(PCFInstruction, 4, shiftedSlaveAddress);

        systick_delay(5);

    }
    
    lcd_clear();

    return;
}

/**
 * @brief Prints a string to the LCD display.
 *
 * This function sends a string of characters to the LCD display for printing.
 *
 * @param[in] input Pointer to the null-terminated string to be printed.
 */
void lcd_print(char *input){
    /* Note: unshifting slave addr which has been shifted left one bit */
    uint8_t shiftedSlaveAddress = SLAVE_ADDR >> 1;

    char c;
    uint8_t upper4Instruction, lower4Instruction, alternateUpper4, alternateLower4;
    uint8_t lcd_print_instruction [4];
    int i = 0;

    while(input[i] != '\0'){
        c = input[i];
        // Prepare the upper half : c with control bits being 1100 (1=1, E = 1, RW=0, RS=1)
        upper4Instruction = (c & 0xF0) | 0b1101;

        alternateUpper4 = (c & 0xF0) | 0b1001; // Flip E

        // Prepare the lower half : c with control bits being 1100 (1=1, E = 1, RW=0, RS=1)
        lower4Instruction = ((c & 0x0F) << 4) | 0b1101;
        
        alternateLower4 = ((c & 0x0F) << 4) | 0b1001; // Flip E

        lcd_print_instruction[0] = upper4Instruction;
        lcd_print_instruction[1] = alternateUpper4;
        lcd_print_instruction[2] = lower4Instruction;
        lcd_print_instruction[3] = alternateLower4;

        i2c_master_write(lcd_print_instruction, 4, shiftedSlaveAddress);
        systick_delay(5);
        i ++;
    }
    return;
}

/**
 * @brief Sets the cursor position on the LCD display.
 *
 * This function moves the cursor to the specified row and column on the LCD.
 *
 * @param[in] row The row number (0-based index).
 * @param[in] col The column number (0-based index).
 */
void lcd_set_cursor(uint8_t row, uint8_t col){
    /* Note: unshifting slave addr which has been shifted left one bit */
    uint8_t shiftedSlaveAddress = SLAVE_ADDR >> 1;
    uint8_t cursor_address;

    /* Note: because the MSB of data is set to 1, otherwise would be 40 and 00 respectively */
    if (row == 1) {cursor_address = 0xC0 | col;} 
    else {cursor_address = 0x80 | col;} 
    
    uint8_t upper4Instruction, lower4Instruction, alternateUpper4, alternateLower4;
    uint8_t cursor_set_instruction [4];
    
        
    // Prepare the upper half : c with control bits being 1100 (1=1, E = 1, RW=0, RS=0)
    upper4Instruction = (cursor_address & 0xF0) | 0b1100;

    alternateUpper4 = (cursor_address & 0xF0) | 0b1000; // Flip E

    // Prepare the lower half : c with control bits being 1100 (1=1, E = 1, RW=0, RS=0)
    lower4Instruction = ((cursor_address & 0x0F) << 4) | 0b1100;

    alternateLower4 = ((cursor_address & 0x0F) << 4) | 0b1000; // Flip E

    cursor_set_instruction[0] = upper4Instruction;
    cursor_set_instruction[1] = alternateUpper4;
    cursor_set_instruction[2] = lower4Instruction;
    cursor_set_instruction[3] = alternateLower4;

    i2c_master_write(cursor_set_instruction, 4, shiftedSlaveAddress);
    systick_delay(5);
    return;
}

/**
 * @brief Clears the LCD display.
 *
 * This function clears all characters from the LCD display 
 */
void lcd_clear() {
     uint8_t P0 = 0x0;
     uint8_t P1 = 0x0;
     uint8_t P3 = 0x1;
     uint8_t P2, P4, P5, P6, P7;
     uint8_t clearInstruction = 0b00000001;
     uint8_t PCFInstruction[4];

     P2 = 0x1;

     //Extract upper 4 bits of instruction
     P4 = clearInstruction >>4 & 0x1;
     P5 = (clearInstruction >> 5) & 0x1;
     P6 = (clearInstruction >> 6) & 0x1;
     P7 = (clearInstruction >> 7) & 0x1;

     PCFInstruction[0] = (P0 << 0) | (P1 << 1) | (P2 << 2) | (P3 << 3) | (P4 << 4) | (P5 << 5) | (P6 << 6) | (P7 << 7);

     P2 = 0x0;
     PCFInstruction[1] = (P0 << 0) | (P1 << 1) | (P2 << 2) | (P3 << 3) | (P4 << 4) | (P5 << 5) | (P6 << 6) | (P7 << 7);
    
     //Extract lower 4 bits of instruction
     P4 = clearInstruction & 0x1;
     P5 = (clearInstruction >> 1) & 0x1;
     P6 = (clearInstruction >> 2) & 0x1;
     P7 = (clearInstruction >> 3) & 0x1;

     P2 = 0x1;
     PCFInstruction[2] = (P0 << 0) | (P1 << 1) | (P2 << 2) | (P3 << 3) | (P4 << 4) | (P5 << 5) | (P6 << 6) | (P7 << 7);

     P2 = 0x0;
     PCFInstruction[3] = (P0 << 0) | (P1 << 1) | (P2 << 2) | (P3 << 3) | (P4 << 4) | (P5 << 5) | (P6 << 6) | (P7 << 7);

    /* Note: unshifting slave addr which has been shifted left one bit */
     uint8_t shiftedSlaveAddress = SLAVE_ADDR >> 1;
     
     i2c_master_write(PCFInstruction, 4, shiftedSlaveAddress);

     systick_delay(2000);
     return;
}
