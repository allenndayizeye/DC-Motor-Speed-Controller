#include <msp430.h>
#include <stdbool.h>

// --- Global Variables ---
const unsigned char lcd_num[10] = {
    0xFC, 0x60, 0xDB, 0xF3, 0x67, 0xB7, 0xBF, 0xE4, 0xFF, 0xF7
};

// CORRECTED: Array of pointers to the LCD memory registers for digits 1-5.
// This is the verified, correct mapping for the MSP-EXP430FR6989.
volatile unsigned char* const lcd_digit_registers[5] = {
    &LCDM6,  // Position 1 (Leftmost)
    &LCDM4,  // Position 2
    &LCDM19,  // Position 3
    &LCDM15, // Position 4
    &LCDM8  // Position 5
};

volatile unsigned int g_pulse_count = 0;
volatile unsigned long g_rpm_to_display = 0;
volatile bool g_new_data_ready = false;


// --- Main Application ---
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    // Port and Clock Initialization
    PJSEL0 = BIT4 | BIT5;
    PM5CTL0 &= ~LOCKLPM5;
    CSCTL0_H = CSKEY >> 8;
    CSCTL4 &= ~LFXTOFF;
    do {
      CSCTL5 &= ~LFXTOFFG;
      SFRIFG1 &= ~OFIFG;
    } while (SFRIFG1 & OFIFG);
    CSCTL0_H = 0;

    // Peripheral Configuration

    // Configure P1.3 for Pulse Input Interrupt
    P1DIR &= ~BIT3;
    P1REN |= BIT3; // Pull-up disabled for active signal source
    P1IES |= BIT3;
    P1IFG &= ~BIT3;
    P1IE |= BIT3;

    // Configure LCD
    LCDCPCTL0 = 0xFFFF;
    LCDCPCTL1 = 0xFC3F;
    LCDCPCTL2 = 0x0FFF;
    LCDCCTL0 = LCDDIV__1 | LCDPRE__16 | LCD4MUX | LCDLP;
    LCDCVCTL = VLCD_1 | VLCDREF_0 | LCDCPEN;
    LCDCCPCTL = LCDCPCLKSYNC;
    LCDCMEMCTL = LCDCLRM;
    LCDCCTL0 |= LCDON;

    // Configure Timer_A for a 3-second interval
    TA0CCR0 = 12287;
    TA0CTL = TASSEL__ACLK | MC__UP | ID__8;
    TA0CCTL0 = CCIE;

    // --- Main Loop ---
    __enable_interrupt();

    while(1)
    {
        if (g_new_data_ready) {
            unsigned long local_rpm;

            __disable_interrupt();
            local_rpm = g_rpm_to_display;
            g_new_data_ready = false;
            __enable_interrupt();

            // This display logic is now correct because the underlying register map is correct.
            *lcd_digit_registers[0] = lcd_num[(local_rpm / 10000) % 10];
            *lcd_digit_registers[1] = lcd_num[(local_rpm / 1000) % 10];
            *lcd_digit_registers[2] = lcd_num[(local_rpm / 100) % 10];
            *lcd_digit_registers[3] = lcd_num[(local_rpm / 10) % 10];
            *lcd_digit_registers[4] = lcd_num[local_rpm % 10];
        }
        __bis_SR_register(LPM3_bits);
        __no_operation();
    }
}

// --- Interrupt Service Routines (ISRs) ---

#pragma vector=PORT1_VECTOR
__interrupt void Port_1_ISR(void)
{
    if(P1IFG & BIT3) {
        g_pulse_count++;
        P1IFG &= ~BIT3;
    }
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR (void)
{
    __disable_interrupt();
    unsigned int current_pulse_count = g_pulse_count;
    g_pulse_count = 0;
    __enable_interrupt();

    // The mathematically correct formula for 4 PPR
    g_rpm_to_display = (unsigned long)current_pulse_count * 5;

    if (g_rpm_to_display > 99999) {
        g_rpm_to_display = 99999;
    }
    g_new_data_ready = true;
    __bic_SR_register_on_exit(LPM3_bits);
}