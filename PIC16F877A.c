/*
 * Keypad Door Lock (with red LED for wrong password)
 * MCU      : PIC16F877A
 * Compiler : HI-TECH C (PICC 9.83)
 * Clock    : 4 MHz (set Processor Clock Frequency = 4MHz in Proteus)
 *
 * Connections:
 *   Keypad (4x3) ROW1..ROW4 -> RB0..RB3  (inputs, internal pull-ups)
 *   Keypad (4x3) COL1..COL3 -> RB4..RB6  (outputs)
 *   LCD 16x2 (4-bit mode):
 *       RS -> RD0, E -> RD1, RW -> GND
 *       D4 -> RD4, D5 -> RD5, D6 -> RD6, D7 -> RD7
 *   Green LED (door unlocked) -> RC0 through 330 ohm resistor
 *   Red LED   (wrong password) -> RC1 through 330 ohm resistor
 *   MCLR -> +5V through a 10k resistor
 *
 * How to use:
 *   Type the 4-digit password, then press # to enter.
 *   Press * to clear the digits you typed.
 *   Default password: 4444
 *   Wrong password   -> red LED turns ON.
 *   After 3 wrong attempts the system locks for 10 seconds (red LED stays ON).
 */

#include <htc.h>

#define _XTAL_FREQ 4000000      // 4 MHz, needed by __delay_ms() and __delay_us()

// Configuration bits
__CONFIG(FOSC_XT & WDTE_OFF & PWRTE_ON & BOREN_ON & LVP_OFF & CPD_OFF & WRT_OFF & CP_OFF);

#define RS    RD0
#define EN    RD1
#define LOCK  RC0               // 1 = door unlocked (green LED on / relay on)
#define WRONG RC1               // 1 = wrong password (red LED on)

// Change the password here (4 digits)
const char password[4] = {'4', '4', '4', '4'};

// Keypad layout (4 rows x 3 columns)
const char keys[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

/* ---------------- LCD functions (4-bit mode) ---------------- */

void lcd_nibble(unsigned char n)
{
    PORTD = (PORTD & 0x0F) | (unsigned char)(n << 4);   // data on RD4..RD7
    EN = 1;
    __delay_us(10);
    EN = 0;
    __delay_us(100);
}

void lcd_cmd(unsigned char c)
{
    RS = 0;
    lcd_nibble(c >> 4);
    lcd_nibble(c & 0x0F);
    __delay_ms(2);
}

void lcd_data(unsigned char d)
{
    RS = 1;
    lcd_nibble(d >> 4);
    lcd_nibble(d & 0x0F);
    __delay_ms(1);
}

void lcd_init(void)
{
    __delay_ms(50);             // wait for LCD power-up
    RS = 0;
    lcd_nibble(0x03);
    __delay_ms(5);
    lcd_nibble(0x03);
    __delay_us(150);
    lcd_nibble(0x03);
    lcd_nibble(0x02);           // switch to 4-bit mode

    lcd_cmd(0x28);              // 4-bit, 2 lines, 5x8 font
    lcd_cmd(0x0C);              // display ON, cursor OFF
    lcd_cmd(0x06);              // cursor moves right
    lcd_cmd(0x01);              // clear display
}

void lcd_clear(void)
{
    lcd_cmd(0x01);
}

void lcd_goto(unsigned char row, unsigned char col)
{
    if (row == 1)
    {
        lcd_cmd(0x80 + col);
    }
    else
    {
        lcd_cmd(0xC0 + col);
    }
}

void lcd_print(const char *s)
{
    while (*s)
    {
        lcd_data(*s);
        s++;
    }
}

/* ---------------- Keypad function ---------------- */

// Returns the pressed key, or 0 if no key is pressed
char get_key(void)
{
    unsigned char col, row;

    for (col = 0; col < 3; col++)
    {
        PORTB = (unsigned char)(~(0x10 << col));    // pull one column LOW
        __delay_us(50);

        for (row = 0; row < 4; row++)
        {
            if ((PORTB & (1 << row)) == 0)          // row is LOW -> key pressed
            {
                __delay_ms(20);                     // debounce
                if ((PORTB & (1 << row)) == 0)
                {
                    while ((PORTB & (1 << row)) == 0);  // wait until released
                    __delay_ms(20);
                    PORTB = 0xFF;
                    return keys[row][col];
                }
            }
        }
    }

    PORTB = 0xFF;
    return 0;
}

/* ---------------- Helper functions ---------------- */

void wait_s(unsigned char s)
{
    while (s > 0)
    {
        __delay_ms(1000);
        s--;
    }
}

void show_prompt(void)
{
    lcd_clear();
    lcd_goto(1, 0);
    lcd_print("Enter Password:");
}

/* ---------------- Main program ---------------- */

void main(void)
{
    char key;
    char entered[4];
    unsigned char idx = 0;
    unsigned char attempts = 0;
    unsigned char i;
    unsigned char ok;

    ADCON1 = 0x06;              // PORTA and PORTE digital
    TRISB  = 0x0F;              // RB0..RB3 inputs (rows), RB4..RB7 outputs (columns)
    TRISC  = 0x00;              // PORTC outputs (RC0 = green LED, RC1 = red LED)
    TRISD  = 0x00;              // PORTD outputs (LCD)
    OPTION_REG &= 0x7F;         // enable PORTB weak pull-ups (RBPU = 0)

    PORTB = 0xFF;
    PORTC = 0x00;
    PORTD = 0x00;
    LOCK  = 0;                  // door locked at start
    WRONG = 0;                  // red LED off at start

    lcd_init();
    show_prompt();

    while (1)
    {
        key = get_key();
        if (key == 0)
        {
            continue;           // no key pressed
        }

        if (key >= '0' && key <= '9')
        {
            if (idx < 4)
            {
                entered[idx] = key;
                lcd_goto(2, 6 + idx);
                lcd_data('*');              // show * for each digit
                idx++;
            }
        }
        else if (key == '*')                // clear
        {
            idx = 0;
            show_prompt();
        }
        else if (key == '#')                // enter
        {
            ok = 1;
            if (idx != 4)
            {
                ok = 0;
            }
            else
            {
                for (i = 0; i < 4; i++)
                {
                    if (entered[i] != password[i])
                    {
                        ok = 0;
                    }
                }
            }

            if (ok == 1)
            {
                attempts = 0;
                lcd_clear();
                lcd_goto(1, 0);
                lcd_print("Access Granted");
                lcd_goto(2, 0);
                lcd_print("Door Unlocked");
                LOCK = 1;                   // unlock (green LED on)
                wait_s(5);                  // stay unlocked for 5 seconds
                LOCK = 0;                   // lock again
                lcd_clear();
                lcd_goto(1, 0);
                lcd_print("Door Locked");
                wait_s(1);
            }
            else
            {
                attempts++;
                lcd_clear();
                WRONG = 1;                  // red LED on
                if (attempts >= 3)
                {
                    lcd_goto(1, 0);
                    lcd_print("System Locked");
                    lcd_goto(2, 0);
                    lcd_print("Wait 10 seconds");
                    wait_s(10);
                    attempts = 0;
                }
                else
                {
                    lcd_goto(1, 0);
                    lcd_print("Wrong Password");
                    lcd_goto(2, 0);
                    lcd_print("Try again");
                    wait_s(2);
                }
                WRONG = 0;                  // red LED off
            }

            idx = 0;
            show_prompt();
        }
    }
}
