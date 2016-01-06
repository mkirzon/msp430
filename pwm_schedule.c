//************************************************************************
// PWM Output with Schedule
// MSP430F5529
//    PWM Output on Pin 1.2
//    RTC, Calendar Mode
//    LPM0 mode (PWM),   ACLK + SMCLK + FLL
//    LPM3 mode (Sleep), ACLK
//    ACLK Source: XT1 oscillator (32kHz)
//    SMCLK Source: DCOCLK
//
// Created by Mark Kirzon
// Last Modified Jan. 6, 2015

#include <msp430.h>

#define SMCLK 1000000L
#define PWM_FREQ 2000                       // PWM Frequency in Hz
#define PWM_DUTY 10                         // PWM Duty Cycle %

#define SET_SLEEP_TIMER setTimer(0, 0, 5);
#define SET_PWM_TIMER setTimer(0, 0, 2);

static void setTimer(int hr, int min, int sec);
static void configPWM(void);

static int pwm_on;

void main(void)
{
  WDTCTL = WDTPW | WDTHOLD;                 // Stop WDT

  // Output Pin Configuration
  P1DIR |= BIT2;                            // P1.2 (PWM) to output
  P1SEL |= BIT2;                            // P1.2 to TA0.1
  P1OUT = 0;

  // Clocks out to pins
  P1DIR |= BIT0;                            // ACLK to p1.0
  P1SEL |= BIT0;
  P2DIR |= BIT2;                            // SMCLK to p2.2
  P2SEL |= BIT2;

  // Clock Configuration
  UCSCTL3 |= SELREF_2;                      // Set DCO FLL Reference = REFO
  UCSCTL3 |= FLLREFDIV_1;                   // FLL refclk / 2
  UCSCTL4 &= ~SELS_7;
  UCSCTL4 |= SELA_0 | SELS_3;               // XT1 src for ACLK, DCOCLK src for SMCLK

  __bis_SR_register(SCG0);                  // Disable the FLL control loop
  UCSCTL0 = 0x0000;                         // Set lowest possible DCOx, MODx
  UCSCTL1 = DCORSEL_0;                      // Select DCO range 1MHz operation
  UCSCTL2 = FLLD_0 + 60;                    // Set DCO Multiplier for 1MHz
                                            // (N + 1) * FLLRef = Fdco
                                            // (60 + 1) * 32768 = 1MHz
                                            // Set FLL Div = fDCOCLK/1
  __bic_SR_register(SCG0);
  __delay_cycles(63000);

  // RTC Configuration
  RTCCTL0 |= RTCTEVIE;                      // Time Event interrupt enable
  RTCCTL01 |= RTCSSEL_0 | RTCTEV_2;         // ACLK source, Time Event at Midnight

  // PWM on Timer_A Configuration
  configPWM();
  enablePWM();
  SET_PWM_TIMER;

  // Enter Low Power Mode for PWM
  _BIS_SR(LPM3_bits + GIE);                 // LPM0, enable global interrupt

  while (1);

}

static void setTimer(int hr, int min, int sec) {
  RTCCTL01 |= RTCHOLD;                      // Pause timer

  RTCCTL01 &= ~RTCMODE;                     // Toggle Mode to clear time
  RTCCTL01 |= RTCMODE;

  long secTotal = 24*3600L - hr*3600L - min*60 - sec;

  RTCSEC = secTotal % 60;
  secTotal /= 60;
  RTCMIN = secTotal % 60;
  RTCHOUR = secTotal / 60;

  RTCCTL01 &= ~RTCHOLD;                         // Resume timer
}

static void configPWM(void) {
  TA0CCR0 = SMCLK / PWM_FREQ;                   // Upper bin count
  TA0CCR1 = SMCLK / PWM_FREQ * PWM_DUTY / 100;  // Duty cycle bin count
}

static void enablePWM(void) {
  TA0CCTL1 = OUTMOD_7;                          // Reset/set mode
  TA0CTL = TASSEL_2 + MC_1;                     // SMCLK source, Up mode
  pwm_on = 1;                                   // Update State
}

static void disablePWM(void) {
  TA0CCTL1 &= ~OUTMOD_7;                        // Out mode
  TA0CTL = 0;                                   // Remove clock source
  pwm_on = 0;                                   // Update State
}

#pragma vector = RTC_VECTOR
__interrupt void RTC_isr (void) {

  RTCCTL01 &= ~RTCTEVIFG;                       // Clear interrupt flag
  if (pwm_on) {
    SET_SLEEP_TIMER;
    disablePWM();
    _BIC_SR_IRQ(LPM0_bits);
    _BIS_SR_IRQ(LPM3_bits + GIE);
  }
  else {
    SET_PWM_TIMER;
    enablePWM();
    _BIC_SR_IRQ(LPM3_bits);
    _BIS_SR_IRQ(LPM0_bits + GIE);
  }
}