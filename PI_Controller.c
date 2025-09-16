#include <msp430.h> 

#include <stdbool.h> 

 

// ------------------------- Configuration ------------------------- 

 

// Overcurrent Detection 

#define OC_SENSE_PIN    BIT6    // P2.6 Overcurrent Sense Input (Active-Low) 

#define GATE_DRIVER_1   BIT7    // P2.7 

#define GATE_DRIVER_2   BIT1    // P2.1 

#define LED_PIN         BIT0    // P1.0 for debug LED 

#define DEBOUNCE_TIME_MS 500 

 

// PI Controller 

#define DESIRED_RPM              8000 

#define KP                       0.65 

#define KI                       0.145 

#define PPR                      4 

#define PWM_PERIOD               67 

#define PWM_MAX                  (PWM_PERIOD - 1) 

#define PWM_MIN                  5 

 

// ------------------------- Global Variables ------------------------- 

 

// PI Controller State 

volatile unsigned int g_setpoint_speed = 0; 

volatile unsigned long g_pulse_count = 0; 

volatile long g_integral_sum = 0; 

volatile long g_measured_speed = 0; 

volatile unsigned long g_last_pulse_count = 0; 

 

// Output Direction 

volatile unsigned int g_current_direction = 0; 

 

// Control Flags 

volatile bool g_pi_controller_active = true; 

volatile bool g_button1_pressed = false; 

volatile bool g_button2_pressed = false; 

 

// Overcurrent Flags 

volatile bool g_overcurrent_detected = false; 

volatile bool g_fault_confirmed = false; 

 

// ------------------------- Delay Function ------------------------- 

 

void delay_ms(unsigned int ms) { 

    while (ms--) { 

        __delay_cycles(1000); // 1ms @ 1MHz 

    } 

} 

 

// ------------------------- Main Function ------------------------- 

 

int main(void) 

{ 

    WDTCTL = WDTPW | WDTHOLD; 

 

    // Clock: DCO = 8MHz, SMCLK = 1MHz 

    CSCTL0_H = CSKEY >> 8; 

    CSCTL1 = DCOFSEL_6; 

    CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK; 

    CSCTL3 = DIVA__8 | DIVS__8 | DIVM__8; 

    CSCTL0_H = 0; 

 

    // --- Setpoint Calculation --- 

    unsigned long ppm = DESIRED_RPM * PPR; 

    g_setpoint_speed = ppm / 600; 

 

    // --- PWM Pins --- 

    P3DIR |= BIT6; P3SEL1 |= BIT6;        // P3.6 = PWM Forward 

    P1DIR |= BIT3; P1SEL0 |= BIT3;        // P1.3 = PWM Reverse 

 

    // --- Encoder Input --- 

    P2DIR &= ~BIT3; 

 

    // --- Button Inputs --- 

    P1DIR &= ~(BIT1 | BIT2); 

    P1REN |= (BIT1 | BIT2); 

    P1OUT |= (BIT1 | BIT2); 

 

    // --- Overcurrent Input (P2.6) --- 

    P2DIR &= ~OC_SENSE_PIN; 

    P2REN |= OC_SENSE_PIN; 

    P2OUT |= OC_SENSE_PIN; 

 

    // --- Gate Driver Outputs (P2.1, P2.7) --- 

    P2DIR |= (GATE_DRIVER_1 | GATE_DRIVER_2); 

    P2OUT |= (GATE_DRIVER_1 | GATE_DRIVER_2); // Start HIGH 

 

    // --- Debug LED (P1.0) --- 

    P1DIR |= LED_PIN; 

    P1OUT &= ~LED_PIN; 

 

    // --- PWM Timer_B0 (Forward - P3.6) --- 

    TB0CCR0 = PWM_PERIOD - 1; 

    TB0CCTL2 = OUTMOD_7; 

    TB0CCR2 = PWM_MIN; 

    TB0CTL = TBSSEL__SMCLK | MC__UP | TBCLR; 

 

    // --- PWM Timer_A1 (Reverse - P1.3) --- 

    TA1CCR0 = PWM_PERIOD - 1; 

    TA1CCTL2 = OUTMOD_0; 

    TA1CCR2 = PWM_MIN; 

    TA1CTL = TASSEL__SMCLK | MC__UP | TACLR; 

 

    // --- PI Timer_A0: 100ms interval --- 

    TA0CCR0 = 12500 - 1; 

    TA0CCTL0 = CCIE; 

    TA0CTL = TASSEL__SMCLK | MC__UP | ID__8 | TACLR; 

 

    // --- Interrupts --- 

    P2IFG &= ~(BIT3 | OC_SENSE_PIN); 

    P2IE |= (BIT3 | OC_SENSE_PIN); 

    P2IES &= ~BIT3; 

    P2IES |= OC_SENSE_PIN; 

 

    P1IFG &= ~(BIT1 | BIT2); 

    P1IE |= (BIT1 | BIT2); 

    P1IES |= (BIT1 | BIT2); 

 

    // Unlock I/O 

    PM5CTL0 &= ~LOCKLPM5; 

    __bis_SR_register(GIE); 

 

    while(1) 

    { 

        __bis_SR_register(LPM0_bits); 

 

        // --- Overcurrent Debounce --- 

        if (g_overcurrent_detected && !g_fault_confirmed) 

        { 

            P1OUT |= LED_PIN; // Turn on LED 

 

            unsigned int start_time = 0; 

            while (start_time < DEBOUNCE_TIME_MS) 

            { 

                if (P2IN & OC_SENSE_PIN) { 

                    g_overcurrent_detected = false; 

                    P1OUT &= ~LED_PIN; //turn LED off 

                    break; 

                } 

                delay_ms(1); 

                start_time++; 

            } 

 

            if (start_time >= DEBOUNCE_TIME_MS) 

            { 

                g_fault_confirmed = true; 

 

                // Disable gate drivers 

                P2OUT &= ~(GATE_DRIVER_1 | GATE_DRIVER_2); 

 

                // Disable further overcurrent interrupts 

                P2IE &= ~OC_SENSE_PIN; 

 

                P1OUT &= ~LED_PIN;  

            } 

        } 

    } 

} 

 

// ------------------------- ISRs ------------------------- 

 

// --- Button ISR --- 

#pragma vector=PORT1_VECTOR 

__interrupt void Port_1_ISR(void) 

{ 

    if (P1IFG & BIT1) { 

        g_button1_pressed = true; 

        __delay_cycles(20000); 

        P1IFG &= ~BIT1; 

    } 

    if (P1IFG & BIT2) { 

        g_button2_pressed = true; 

        __delay_cycles(20000); 

        P1IFG &= ~BIT2; 

    } 

} 

 

// --- Port 2 ISR: Encoder + Overcurrent --- 

#pragma vector=PORT2_VECTOR 

__interrupt void Port_2_ISR(void) 

{ 

    if (P2IFG & BIT3) { 

        g_pulse_count++; 

        P2IFG &= ~BIT3; 

    } 

 

    if (P2IFG & OC_SENSE_PIN) { 

        if (!g_fault_confirmed) { 

            g_overcurrent_detected = true; 

        } 

        P2IFG &= ~OC_SENSE_PIN; 

        __bic_SR_register_on_exit(LPM0_bits); 

    } 

} 

 

// --- PI Controller ISR --- 

#pragma vector=TIMER0_A0_VECTOR 

__interrupt void PI_Controller_ISR(void) 

{ 

    // --- Button 1: Toggle PI --- 

    if (g_button1_pressed) { 

        g_button1_pressed = false; 

        g_pi_controller_active = !g_pi_controller_active; 

        g_integral_sum = 0; 

 

        if (g_pi_controller_active) { 

            if (g_current_direction == 0) { 

                // Forward 

                TA1CCTL2 = OUTMOD_0; 

                P1SEL0 &= ~BIT3; P1DIR |= BIT3; P1OUT &= ~BIT3; P1REN |= BIT3; 

                P3REN &= ~BIT6; P3SEL1 |= BIT6; 

                TB0CCTL2 = OUTMOD_7; TB0CCR2 = PWM_MIN; 

            } else { 

                // Reverse 

                TB0CCTL2 = OUTMOD_0; 

                P3SEL1 &= ~BIT6; P3DIR |= BIT6; P3OUT &= ~BIT6; P3REN |= BIT6; 

                P1REN &= ~BIT3; P1SEL0 |= BIT3; 

                TA1CCTL2 = OUTMOD_7; TA1CCR2 = PWM_MIN; 

            } 

        } else { 

            // Motor OFF 

            TB0CCTL2 = OUTMOD_0; 

            P3SEL1 &= ~BIT6; P3DIR |= BIT6; P3OUT &= ~BIT6; P3REN |= BIT6; 

 

            TA1CCTL2 = OUTMOD_0; 

            P1SEL0 &= ~BIT3; P1DIR |= BIT3; P1OUT &= ~BIT3; P1REN |= BIT3; 

        } 

    } 

 

    // --- Button 2: Toggle Direction --- 

    if (g_button2_pressed) { 

        g_button2_pressed = false; 

        g_current_direction = !g_current_direction; 

        g_integral_sum = 0; 

 

        if (g_pi_controller_active) { 

            if (g_current_direction == 0) { 

                // Forward 

                TA1CCTL2 = OUTMOD_0; 

                P1SEL0 &= ~BIT3; P1DIR |= BIT3; P1OUT &= ~BIT3; P1REN |= BIT3; 

                P3REN &= ~BIT6; P3SEL1 |= BIT6; 

                TB0CCTL2 = OUTMOD_7; TB0CCR2 = PWM_MIN; 

            } else { 

                // Reverse 

                TB0CCTL2 = OUTMOD_0; 

                P3SEL1 &= ~BIT6; P3DIR |= BIT6; P3OUT &= ~BIT6; P3REN |= BIT6; 

                P1REN &= ~BIT3; P1SEL0 |= BIT3; 

                TA1CCTL2 = OUTMOD_7; TA1CCR2 = PWM_MIN; 

            } 

        } 

    } 

 

    // --- PI Logic --- 

    if (g_pi_controller_active) { 

        long error = g_setpoint_speed - (g_pulse_count - g_last_pulse_count); 

        g_last_pulse_count = g_pulse_count; 

 

        float p_term = KP * error; 

        g_integral_sum += error; 

        if (g_integral_sum > 250) g_integral_sum = 250; 

        if (g_integral_sum < -250) g_integral_sum = -250; 

        float i_term = KI * g_integral_sum; 

 

        int output = (int)(p_term + i_term); 

        if (output < PWM_MIN) output = PWM_MIN; 

        if (output > PWM_MAX) output = PWM_MAX; 

 

        if (g_current_direction == 0) { 

            TB0CCR2 = output; 

        } else { 

            TA1CCR2 = output; 

        } 

    } else { 

        g_last_pulse_count = g_pulse_count; 

        g_measured_speed = 0; 

    } 

} 

 

 

 