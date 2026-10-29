#include <msp430.h>
#include <math.h>

volatile long int ADCXval, ADCYval, ADCZval;
volatile float Xper, Yper, Zper;
volatile int delay;

void TimerA_setup(void)
{
  TA0CCTL0 = CCIE; // Enabled interrupt
  TA0CCR0 = 3276; // 3277 / 32768 = .1s for ACLK
  TA0CTL = TASSEL_1 + MC_1; // ACLK, up mode
}

void ADC_setup(void) {
  int i =0;
  
  P6DIR &= ~(BIT3 + BIT4 + BIT5); // Configure P6.3, P6.4 as input pins + P6.5 for z axis
  P6SEL |= BIT3 + BIT4 + BIT5; // Configure P6.3, P6.4 as analog pins
  
  // configure ADC converter
  ADC12CTL0 = ADC12ON + ADC12SHT0_6 + ADC12MSC;
  ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1; // Use sample timer, single sequence
  ADC12MCTL0 = ADC12INCH_3; // ADC A3 pin -X-axis
  ADC12MCTL1 = ADC12INCH_4; // ADC A4 pin - Y-axis
  ADC12MCTL2 = ADC12INCH_5 + ADC12EOS; // ADC A5 pin - Z-axis
  
  // EOS - End of Sequence for Conversions
  ADC12IE = ADC12IE0; // Enable ADC12IFG.1
  for (i = 0; i < 0x3600; i++); // Delay for reference start-up
  ADC12CTL0 |= ADC12ENC; // Enable conversions
}

void UART_putCharacter(char c) {
  while (!(UCA1IFG&UCTXIFG)); // Wait for previous character to transmit
  UCA1TXBUF = c; // Put character into tx buffer
}

void UART_setup(void) {
  P4SEL |= BIT4 + BIT5; // Set up Rx and Tx bits
  UCA1CTL0 = 0; // Set up default RS-232 protocol
  UCA1CTL1 |= BIT0 + UCSSEL_2; // Disable device, set clock
  UCA1BR0 = 27; // 1048576 Hz / 38400
  UCA1BR1 = 0;
  UCA1MCTL = 0x94;
  UCA1CTL1 &= ~BIT0; // Start UART device
}

#define ADC_TO_G(n) ((3.0 * n / 4095 - 1.5) / 0.3) //convert to g

void sendData(void)
{
  int i;
  int magnitude;
  // Part 1 - get samples from ADC
  Xper = (ADC_TO_G(ADCXval)); // Calculate percentage outputs
  Yper = (ADC_TO_G(ADCYval)); // Calculate percentage outputs
  Zper = (ADC_TO_G(ADCZval)); // Calculate percentage outputs
  
  // Use character pointers to send one byte at a time
  char *xpointer=(char *)&Xper;
  char *ypointer=(char *)&Yper;
  char *zpointer=(char *)&Zper;
  
  UART_putCharacter(0x55); // Send header
  for(i = 0; i < 4; i++)
  { // Send x percentage - one byte at a time
    UART_putCharacter(xpointer[i]);
  }
  for(i = 0; i < 4; i++)
  { // Send y percentage - one byte at a time
    UART_putCharacter(ypointer[i]);
  }
  for(i = 0; i < 4; i++)
  { // Send y percentage - one byte at a time
    UART_putCharacter(zpointer[i]);
  }
  
  magnitude = sqrt((Xper*Xper) + (Yper*Yper) + (Zper*Zper));
  if (magnitude >= 3) {
    P1OUT |= BIT0; //turn LED on
  } else {
    P1OUT &= ~(BIT0); // turn LED off
  }
}
void main(void) {
  WDTCTL = WDTPW +WDTHOLD; // Stop WDT
  P1DIR |= BIT0;
  P1OUT &= ~(BIT0); //set RED LED off
  P4DIR |= BIT7;
  P4OUT |= BIT7; // set Green LED on
  delay = 0;
  
  TimerA_setup(); // Setup timer to send ADC data
  ADC_setup(); // Setup ADC
  UART_setup(); // Setup UART for RS-232
  _EINT();
  
  while (1){
    ADC12CTL0 |= ADC12SC; // Start conversions
    __bis_SR_register(LPM0_bits + GIE); // Enter LPM0
  }
}

#pragma vector = ADC12_VECTOR
__interrupt void ADC12ISR(void) {
  ADC12IFG = 0x00;
  ADCXval = ADC12MEM0; // Move results, IFG is cleared
  ADCYval = ADC12MEM1;
  ADCZval = ADC12MEM2; //added z val
  __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void timerA_isr() {
  sendData(); // Send data to serial app
  __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
}
