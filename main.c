//******************************************************************************
//  MSP430G2553  - Calibracion DCO y salida de 32kHZ.
//  Calibra las constantes del DCO para el 553 y provee por el pin 1.0
//	la salida de 32kHz util para calibrar otros controladores sin cristal.
//
//  Basado en msp430g2x55_dco_flashcal.c
//           MSP430G2xx3
//         ---------------
//     /|\|            XIN|-
//      | |               | 32kHz
//      --|RST        XOUT|-
//        |               |
//        |           P2.0|--> LED
//        |           P1.0|--> ACLK = 32kHz
//
//******************************************************************************
#include <msp430.h>

#define DELTA_1MHZ    244                   // 244 x 4096Hz = 999.4Hz
#define DELTA_8MHZ    1953                  // 1953 x 4096Hz = 7.99MHz
#define DELTA_12MHZ   2930                  // 2930 x 4096Hz = 12.00MHz
#define DELTA_16MHZ   3906                  // 3906 x 4096Hz = 15.99MHz

unsigned char CAL_DATA[8];                  // Temp. storage for constants
volatile unsigned int i;
int j;
char *Flash_ptrA;                           // Segment A pointer
void Set_DCO(unsigned int Delta);

int main(void)
{
  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
  for (i = 0; i < 0xfffe; i++);             // Delay for XTAL stabilization
  P2OUT = 0x00;                             // Limpiar salida del P2

  P1DIR |= BIT0;							//  P1.0 salida de ACLK=32kHz
  P1SEL = BIT0;                             //
  P1SEL2 &= ~BIT0;							//

  P2DIR |= BIT0;                            // P2.0 Salida
  j = 0;                                    // Reset pointer

  Set_DCO(DELTA_16MHZ);                     // Set DCO and obtain constants
  CAL_DATA[j++] = DCOCTL;
  CAL_DATA[j++] = BCSCTL1;

  Set_DCO(DELTA_12MHZ);                     // Set DCO and obtain constants
  CAL_DATA[j++] = DCOCTL;
  CAL_DATA[j++] = BCSCTL1;

  Set_DCO(DELTA_8MHZ);                      // Set DCO and obtain constants
  CAL_DATA[j++] = DCOCTL;
  CAL_DATA[j++] = BCSCTL1;

  Set_DCO(DELTA_1MHZ);                      // Set DCO and obtain constants
  CAL_DATA[j++] = DCOCTL;
  CAL_DATA[j++] = BCSCTL1;

  Flash_ptrA = (char *)0x10C0;              // Point to beginning of seg A
  FCTL2 = FWKEY + FSSEL0 + FN1;             // MCLK/3 for Flash Timing Generator
  FCTL1 = FWKEY + ERASE;                    // Set Erase bit
  FCTL3 = FWKEY + LOCKA;                    // Clear LOCK & LOCKA bits
  *Flash_ptrA = 0x00;                       // Dummy write to erase Flash seg A
  FCTL1 = FWKEY + WRT;                      // Set WRT bit for write operation
  Flash_ptrA = (char *)0x10F8;              // Point to beginning of cal consts
  for (j = 0; j < 8; j++)
    *Flash_ptrA++ = CAL_DATA[j];            // re-flash DCO calibration data
  FCTL1 = FWKEY;                            // Clear WRT bit
  FCTL3 = FWKEY + LOCKA + LOCK;             // Set LOCK & LOCKA bit

  while (1)
  {
    P2OUT ^= BIT0;                          // Toggle LED P2.0
    for (i = 0; i < 0x4000; i++);           // SW Delay
  }
}

void Set_DCO(unsigned int Delta)            // Set DCO to selected frequency
{
  unsigned int Compare, Oldcapture = 0;

  BCSCTL1 |= DIVA_3;                        // ACLK = LFXT1CLK/8
  TACCTL0 = CM_1 + CCIS_1 + CAP;            // CAP, ACLK
  TACTL = TASSEL_2 + MC_2 + TACLR;          // SMCLK, cont-mode, clear

  while (1)
  {
    while (!(CCIFG & TACCTL0));             // Wait until capture occured
    TACCTL0 &= ~CCIFG;                      // Capture occured, clear flag
    Compare = TACCR0;                       // Get current captured SMCLK
    Compare = Compare - Oldcapture;         // SMCLK difference
    Oldcapture = TACCR0;                    // Save current captured SMCLK

    if (Delta == Compare)
      break;                                // If equal, leave "while(1)"
    else if (Delta < Compare)
    {
      DCOCTL--;                             // DCO is too fast, slow it down
      if (DCOCTL == 0xFF)                   // Did DCO roll under?
        if (BCSCTL1 & 0x0f)
          BCSCTL1--;                        // Select lower RSEL
    }
    else
    {
      DCOCTL++;                             // DCO is too slow, speed it up
      if (DCOCTL == 0x00)                   // Did DCO roll over?
        if ((BCSCTL1 & 0x0f) != 0x0f)
          BCSCTL1++;                        // Sel higher RSEL
    }
  }
  TACCTL0 = 0;                              // Stop TACCR0
  TACTL = 0;                                // Stop Timer_A
  BCSCTL1 &= ~DIVA_3;                       // ACLK = LFXT1CLK
}
