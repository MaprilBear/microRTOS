#include <stdint.h>

// ***************** WideTimer0A_Init ****************
// Activate Wide Timer0 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
//          priority 0 (highest) to 7 (lowest)
// Outputs: none
void WideTimer1_Init(void(*task)(void), uint32_t period, uint32_t priority);

void WideTimer1_OneShotInit();

void WideTimer1_Stop(void);

uint64_t WideTimer1_GetValue();