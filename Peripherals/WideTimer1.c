
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"

void (*WidePeriodicTask1)(void);   // user function

uint8_t userTask1 = 0; // 0 for not used, 1 for used

// ***************** WideTimer0A_Init ****************
// Activate WideTimer0 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
//          priority 0 (highest) to 7 (lowest)
// Outputs: none
void WideTimer1_Init(void(*task)(void), uint32_t period, uint32_t priority){
  SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R1;   // 0) activate WTIMER1
  WTIMER1_CTL_R = 0x00000000;    // 1) disable WTIMER1A during setup
  WTIMER1_CFG_R = 0x00000000;    // 2) configure for 64-bit mode
  WTIMER1_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  WTIMER1_TAILR_R = period-1;    // 4) reload value
  WTIMER1_TBILR_R = 0;           // bits 63:32
  WTIMER1_TAPR_R = 0;            // 5) bus clock resolution
  WTIMER1_ICR_R = 0x00000001;    // 6) clear WTIMER1A timeout flag TATORIS
  WTIMER1_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI24_R = (NVIC_PRI24_R&NVIC_PRI24_INTA_M)|(priority<<NVIC_PRI24_INTA_S); // priority 
// interrupts enabled in the main program after all devices initialized
// vector number 110, interrupt number 94
  NVIC_EN3_R |= 1;            // 9) enable IRQ 96 in NVIC
  WTIMER1_CTL_R = 0x00000001;    // 10) enable
}

void WideTimer1_OneShotInit(){
  SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R1;   // 0) activate WTIMER1
  WTIMER1_CTL_R = 0x00000000;    // 1) disable WTIMER1A during setup
  WTIMER1_CFG_R = 0x00000000;    // 2) configure for 64-bit mode
  WTIMER1_TAMR_R = 0x00000010;   // 3) configure for periodic mode, default down-count settings
  WTIMER1_TAILR_R = 0xFFFFFFFF;    // 4) reload value
	WTIMER1_TBILR_R = 0xFFFFFFFF;           // bits 63:32
  WTIMER1_TAPR_R = 0;            // 5) bus clock resolution
  WTIMER1_ICR_R = 0x00000001;    // 6) clear WTIMER1A timeout flag TATORIS
  WTIMER1_IMR_R = 0x00000000;    // 7) arm timeout interrupt
  //NVIC_PRI24_R = (NVIC_PRI24_R&NVIC_PRI24_INTA_M)|(7<<NVIC_PRI24_INTA_S); // priority 
// interrupts enabled in the main program after all devices initialized
// vector number 112, interrupt number 96
  //NVIC_EN3_R |= 1;            // 9) enable IRQ 94 in NVIC
  WTIMER1_CTL_R = 0x00000001;    // 10) enable TIMER1A
}

void WideTimer1A_Handler(void){
  WTIMER0_ICR_R = TIMER_ICR_TATOCINT;// acknowledge WTIMER5A timeout
}
void WideTimer1_Stop(void){
  NVIC_DIS2_R = 1<<30;          // 9) disable interrupt 94 in NVIC
  WTIMER0_CTL_R = 0x00000000;    // 10) disable wtimer0A
}

uint64_t WideTimer1_GetValue(){
  uint64_t lower = WTIMER1_TBV_R;
  uint64_t higher = WTIMER1_TAV_R;
  return lower + (higher << 32);
}
