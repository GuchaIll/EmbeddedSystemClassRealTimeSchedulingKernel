/**
 * @file keypad_driver.c
 * 
 * @brief contains functions for initializing keypad pins and receiving data from keypad presses
 * 
 * @date   Febuary 20 2025
 * 
 * @author Mario Cruz | Charlie Ai <macruz | yacil>
 */

#include <unistd.h>
#include <gpio.h>
#include <keypad_driver.h>
#include <systick.h>

/**
 * @brief GPIO port and pin configurations for keypad columns.
 */
//@{
#define KEYPAD_COL1_PORT GPIO_B         /**< GPIO port for column 1. */
#define KEYPAD_COL1_NUMBER 10           /**< GPIO pin number for column 1. */

#define KEYPAD_COL2_PORT GPIO_A         /**< GPIO port for column 2. */
#define KEYPAD_COL2_NUMBER 10           /**< GPIO pin number for column 2. */

#define KEYPAD_COL3_PORT GPIO_A         /**< GPIO port for column 3. */
#define KEYPAD_COL3_NUMBER 9            /**< GPIO pin number for column 3. */
//@}


/**
 * @brief GPIO port and pin configurations for keypad rows.
 */
//@{
#define KEYPAD_ROW1_PORT GPIO_B         /**< GPIO port for row 1. */
#define KEYPAD_ROW1_NUMBER 5            /**< GPIO pin number for row 1. */

#define KEYPAD_ROW2_PORT GPIO_B         /**< GPIO port for row 2. */
#define KEYPAD_ROW2_NUMBER 6             /**< GPIO pin number for row 2. */

#define KEYPAD_ROW3_PORT GPIO_C         /**< GPIO port for row 3. */
#define KEYPAD_ROW3_NUMBER 7            /**< GPIO pin number for row 3. */

#define KEYPAD_ROW4_PORT GPIO_A         /**< GPIO port for row 4. */
#define KEYPAD_ROW4_NUMBER 8            /**< GPIO pin number for row 4. */
//@}

/* Global Variables For State: For debouncing could check every 5
   milliseconds after returning from read_keypad with a non null char,
   and only allow to enter the function again once we read every 5 milli
   after the initial return and the button is not pressed anymore*/





/**
 * @brief Sets the specified column pin to high.
 *
 * @param[in] col Column number (1 to 3).
 */
void set_col(int col){
    switch(col)
    {
        case 1:
            gpio_set(KEYPAD_COL1_PORT, KEYPAD_COL1_NUMBER);
            break;
        case 2:
            gpio_set(KEYPAD_COL2_PORT, KEYPAD_COL2_NUMBER);
            break;
        case 3:
            gpio_set(KEYPAD_COL3_PORT, KEYPAD_COL3_NUMBER);
            break;
        default:
            break;
    }
}

/**
 * @brief Reads the state of the specified row pin.
 *
 * @param[in] row Row number (1 to 4).
 * @return 1 if the row pin is high, 0 if it is low, or -1 for invalid row.
 */
int read_row(int row){
    int a;
    switch(row)
    {
        case 1:
            a = gpio_read(KEYPAD_ROW1_PORT, KEYPAD_ROW1_NUMBER);
            return a;
        case 2:
            return gpio_read(KEYPAD_ROW2_PORT, KEYPAD_ROW2_NUMBER);
        case 3:
            return gpio_read(KEYPAD_ROW3_PORT, KEYPAD_ROW3_NUMBER);
        case 4:
            return gpio_read(KEYPAD_ROW4_PORT, KEYPAD_ROW4_NUMBER);
        default:
            return -1;
    }
}

/**
 * @brief Initializes the GPIO pins for the keypad.
 *
 * Configures the column pins as output and the row pins as input with pull-down resistors.
 */
void keypad_init() {
    /* Col Inits*/
    gpio_init(KEYPAD_COL1_PORT, KEYPAD_COL1_NUMBER, MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);
    gpio_init(KEYPAD_COL2_PORT, KEYPAD_COL2_NUMBER, MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);
    gpio_init(KEYPAD_COL3_PORT, KEYPAD_COL3_NUMBER, MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);

    /* Row Inits*/
    gpio_init(KEYPAD_ROW1_PORT, KEYPAD_ROW1_NUMBER, MODE_INPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_PULL_DOWN, ALT0);
    gpio_init(KEYPAD_ROW2_PORT, KEYPAD_ROW2_NUMBER, MODE_INPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_PULL_DOWN, ALT0);
    gpio_init(KEYPAD_ROW3_PORT, KEYPAD_ROW3_NUMBER, MODE_INPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_PULL_DOWN, ALT0);
    gpio_init(KEYPAD_ROW4_PORT, KEYPAD_ROW4_NUMBER, MODE_INPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_PULL_DOWN, ALT0);
    return;
}

/**
 * @brief Reads the character corresponding to a keypad press.
 *
 * Scans the keypad by setting each column high and checking the rows for a press.
 *
 * @return The character corresponding to the pressed key, or '\0' if no key is pressed.
 */
char keypad_read() {
    char pressed = '\0';
    for(int i = 1; i <= 3; i++) {
        gpio_clr(KEYPAD_COL1_PORT, KEYPAD_COL1_NUMBER);
        gpio_clr(KEYPAD_COL2_PORT, KEYPAD_COL2_NUMBER);
        gpio_clr(KEYPAD_COL3_PORT, KEYPAD_COL3_NUMBER);
        set_col(i);
        for(int j = 1; j <= 4; j++)
        {
            if(read_row(j))
            {
                switch(i)
                {
                    case 1:
                        switch(j)
                        {
                            case 1:
                                delay();
                                return  '1';
                            case 2:
                                delay();
                                return  '4';
                            case 3:
                                delay();
                                return '7';
                            case 4:
                                delay();
                                return '*';
                            
                        }
                        break;
                    case 2:
                        switch(j)
                        {
                            case 1:
                                delay();
                                return  '2';
                            case 2:
                                delay();
                                return '5';
                            case 3:
                                delay();
                                return '8';
                            case 4:
                                delay();
                                return '0';
                        }
                        break;
                    case 3:
                        switch(j)
                        {
                            case 1:
                                delay();
                                return '3';
                            case 2:
                                delay();
                                return '6';
                            case 3:
                                delay();
                                return '9';
                            case 4:
                                delay();
                                return '#';
                            
                        }
                        break;

                    
                }
            }
        }
    }

    return pressed;
}


/**
 * @brief Adds a delay to debounce keypad presses.
 *
 * This function introduces a time interval after a key press is detected
 * to prevent multiple detections of the same press.
 */
void delay()
{
    // systick_delay(1000);
    return;
};