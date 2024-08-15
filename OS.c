// *************os.c**************
// EE445M/EE380L.6 Labs 1, 2, 3, and 4 
// High-level OS functions
// Students will implement these functions as part of Lab
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 
// Jan 12, 2020, valvano@mail.utexas.edu


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"
#include "../inc/Timer4A.h"
#include "../inc/Timer5A.h"
#include "../inc/Timer.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/WideTimer1.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/sensor.h"
#include "../RTOS_Labs_common/esp8266.h"


// Additional timers for AddPeriodicThread
#include "../inc/Timer1A.h"
#include "../inc/Timer2A.h"

extern void ContextSwitch();
extern void StartOS();
extern void PendSV_Handler();
extern void HalfSwitch();
extern int GetIPSRMasked();
extern int StartCriticalASM();
extern void EndCriticalASM(long tmp);
extern int GetPRIMASK();

volatile uint64_t captureStartTime = 0;
volatile uint64_t criticalTime = 0;
volatile uint64_t disabledTimestamp = 0;

/* 

  IMPORTANT POINTS

  Background threads CANNOT block, wait, loop, sleep, or kill and thus can't context switch or reschedule

  Thus, when a thread sleeps it should EnableInterrupts() before ContextSwitch() not only to 
  allow the context switch to work but to also allow any background threads to run before switching.
  Those interrupts WILL NOT CAUSE A CONTEXT SWITCH OR RESCHEDULE

*/

// Performance Measurements 

void LogDisableInterrupts(){
  
  if (captureStartTime != 0 && disabledTimestamp == 0){
    disabledTimestamp = OS_Time();
  }
  
}

void LogEnableInterrupts(){
  
  if (disabledTimestamp != 0){
    criticalTime += OS_TimeDifference(disabledTimestamp, OS_Time());
    disabledTimestamp = 0;
  }
  
}

uint64_t OS_CaptureTime(){
  int tmp = StartCritical();
  uint64_t out = OS_TimeDifference(captureStartTime, OS_Time());
  EndCritical(tmp);
  return out;
}

volatile uint64_t IdleCount;
uint64_t IdleCountRef = 99537;

void OS_ResetStats(){
  int tmp = StartCritical();
  criticalTime = 0;
  IdleCount = 0;
  captureStartTime = OS_Time();
  disabledTimestamp = 0;
  EndCritical(tmp);
}

uint64_t OS_GetCriticalTime(){
  int tmp = StartCritical();
  uint64_t out = criticalTime;
  EndCritical(tmp);
  return out;
}

#define PD1  (*((volatile uint32_t *)0x40007008))

void Idle(void){
	EnableInterrupts();
  while (1) {
    //PD1 ^= 0x2;
    IdleCount++;  // measure of CPU idle time
    //PD1 ^= 0x2;
  }
}

#define UTIL_AVERAGE 10

volatile uint32_t CPUUtil[UTIL_AVERAGE];
volatile uint32_t CPUUtilIndex = 0;

uint32_t OS_GetCPUUtil(){
  // fixed point X.2 base 10
  // IdleRef is number of executions in uninterrupted 100ms
  int tmp = StartCritical();
  uint32_t sum = 0;
  for (int i = 0; i < UTIL_AVERAGE; i++){
    sum += CPUUtil[i];
  }
  EndCritical(tmp);
  return sum / 10;
}

void OS_UpdateCPUUtil(){
  int tmp = StartCritical();
  // remove freak errors
  if (IdleCount < IdleCountRef * 100){
    CPUUtil[CPUUtilIndex++ % UTIL_AVERAGE] = 10000 - (IdleCount * 10000 / IdleCountRef / 100);
  }
  IdleCount = 0;
  EndCritical(tmp);
}

uint32_t OSTimeMs;

int16_t tcb_id_count = 0;
int16_t pcb_id_count = 0;

void (*SW1Task)(void); // user function executed on press of Sw1
void (*SW2Task)(void); // user function executed on press of Sw2

#define ROOT_PROCESS_PRIORITY 7

// only one process can exist at a priority level at a time
PCB* processTable[PRIORITY_LEVELS];
PCB* currentProcess;

struct MSPeriodicThread_struct{
  void(*task)(void);
  int32_t period;
  int32_t currentCountdown;
}; typedef struct MSPeriodicThread_struct MSPeriodicThread;

#define MS_THREAD_NUM 10
MSPeriodicThread MSThreads[MS_THREAD_NUM];

TCB* currTCB;
TCB* nextTCB;

void UpdateCSGlobals(){
	currTCB = currentProcess->currTCB;
	nextTCB = currentProcess->nextTCB;
}

// adds tcb to list pointed to by listHead, if appendToFront is false the tcb is appended to the back
// returns new list head to be assigned if it has changed, if not then returns old list head
TCB* appendTCBToList(TCB* listHead, TCB* tcb, bool appendtoFront){
  int tmp = StartCritical();
  if (listHead == NULL){
    // Bucket is empty
    tcb->next = tcb;
    tcb->prev = tcb;
    EndCritical(tmp);
    return tcb;
  } else {
    // Bucket has things
    if (appendtoFront){
      listHead->prev->next = tcb;
      tcb->next = listHead;
      tcb->prev = listHead->prev;
      listHead->prev = tcb;
      EndCritical(tmp);
      return tcb;
    } else {
      // append to back
      listHead->prev->next = tcb;
      tcb->prev = listHead->prev;
      listHead->prev = tcb;
      tcb->next = listHead;
      EndCritical(tmp);
      return listHead;
    }
  }
}


// removes the TCB from it's list. 
// standard usage should be tcbListHeadPtr = removeTCBFromList(tcbToBeRemovedPtr, tcbListHeadPtr )
// if the list is now empty, returns a nullptr
// if the list head needs to be reassigned, returns the new assignment
// if the list head does not need to changed, returns the list head
TCB* removeTCBFromList(TCB* tcbPtr, TCB* listHead){
  if (tcbPtr->next == tcbPtr){
    // only one tcb in list
    return NULL;
  } else if (tcbPtr == listHead){
    // removing head
    TCB* next = tcbPtr->next;
    tcbPtr->prev->next = tcbPtr->next; 
    tcbPtr->next->prev = tcbPtr->prev;
    return next;
  } else {
    // standard removal
    tcbPtr->prev->next = tcbPtr->next; 
    tcbPtr->next->prev = tcbPtr->prev;
    return listHead;
  }
}

TCB* getNextPrioTCBFromTableAndRemove(TCB* threadTable[PRIORITY_LEVELS]){
  int tmp = StartCritical();
  for (int i = 0; i < PRIORITY_LEVELS; i++){
    if (threadTable[i] != NULL){
      TCB* nextTCB = threadTable[i];
      if ((intptr_t)nextTCB < 0x100){
        while (1){}
      }
      threadTable[i] = removeTCBFromList(nextTCB, threadTable[i]);

      //nextTCB->parentProcess->threadTable[nextTCB->priority] = appendTCBToList(nextTCB->parentProcess->threadTable[nextTCB->priority], nextTCB, false);
      EndCritical(tmp);
      return nextTCB;
    }
  }
  EndCritical(tmp);
  return NULL;
}

//void(*jitter1TaskPtr)(void) ;
//void(*jitter2TaskPtr)(void) ;

//Jitter_t* OS_GetJitter(void(*task)(void)){
//  int tmp = StartCritical();
//  if (task == jitter1TaskPtr){
//    EndCritical(tmp);
//    return &jitter1;
//  } else if (task == jitter2TaskPtr){
//    EndCritical(tmp);
//    return &jitter2;
//  } else {
//    return NULL;
//  }
//}

uint32_t OS_Id(void){
  return currTCB->id;
};

uint32_t OS_Priority(void){
  return currTCB->priority;
}



void WalkSleepList(PCB* process){
  int tmp = StartCritical();
  if (process->sleepList != NULL){
    TCB* tcb = process->sleepList;
    do {
      tcb->sleepTime--;
      TCB* next = tcb->next;
      if (tcb->sleepTime <= 0){
        tcb->sleepTime = 0;
        
        process->sleepList = removeTCBFromList(tcb, process->sleepList);
        
        process->threadTable[tcb->priority] = appendTCBToList(process->threadTable[tcb->priority], tcb, false);
      }
      
      tcb = next;
    } while (tcb != process->sleepList && process->sleepList != NULL);
  }
  EndCritical(tmp);
}

void MsTime_TimerHandler(){
  int tmp = StartCritical();
  if (OSTimeMs++ % 1000 == 999){
    OS_UpdateCPUUtil();
  }
	
	// walk the sleep list of every process
	for (int i = 0; i < PRIORITY_LEVELS; i++){
		WalkSleepList(processTable[i]);
	}
  
  // process MS perodic threads
  for (int i = 0; i < MS_THREAD_NUM; i++){
    if (MSThreads[i].task != 0){
      MSThreads[i].currentCountdown--;
      if (MSThreads[i].currentCountdown <= 0){
        MSThreads[i].currentCountdown = MSThreads[i].period;
				EnableInterrupts();
        MSThreads[i].task();
				DisableInterrupts();
      }
    }
  }
  EndCritical(tmp);
}

uint32_t OSTime_Overflows;

void OSTime_Handler(){
  OSTime_Overflows++;
}

// if currRemoved is true, the currTCB is no longer in the activeThreadTable
void OS_Scheduler(bool currRemoved){
  int tmp = StartCritical();
	currentProcess->currTCB = currTCB;
	currentProcess->nextTCB = nextTCB;
  if (currentProcess->currTCB == NULL) {
    // called from OS_Launch
    for (int i = 0; i < PRIORITY_LEVELS; i++){
      if (currentProcess->threadTable[i] != NULL){
        currentProcess->nextTCB = currentProcess->threadTable[i];
				currentProcess->currTCB = currentProcess->nextTCB;
				UpdateCSGlobals();
        EndCritical(tmp);
        return;
      }
    }
  } else {
    // iterate through process table to find highest priority process
		
		for (int i = 0; i < PRIORITY_LEVELS; i++){
			if (processTable[i] != 0){
				PCB* nextProcess = processTable[i];
				
				if (!currRemoved){
					// put currTCB at end of bucket
					currentProcess->threadTable[currTCB->priority] = removeTCBFromList(currTCB, currentProcess->threadTable[OS_Priority()]);
					currentProcess->threadTable[currTCB->priority] = appendTCBToList(currentProcess->threadTable[OS_Priority()], currTCB, false);
				}
				
				if (currentProcess != nextProcess){
					// switching processes
					
					// grab next TCB from new process 
					for (int i = 0; i < PRIORITY_LEVELS; i++){
						if (nextProcess->threadTable[i] != NULL){
							nextTCB = nextProcess->threadTable[i];
							nextProcess->currTCB = nextTCB;
							nextProcess->nextTCB = nextTCB;
							currentProcess = nextProcess;
							EndCritical(tmp);
							return;
						}
					}
					// no TCB in current process, go to next highest process
				} else {
					// same process
					
					// grab next TCB
					for (int i = 0; i < PRIORITY_LEVELS; i++){
						if (currentProcess->threadTable[i] != NULL){
							currentProcess->nextTCB = currentProcess->threadTable[i];
							UpdateCSGlobals();
							EndCritical(tmp);
							return;
						}
					}
					// no TCB left, go to next highest priority process
				}
			}
		}
		// could not find a next thread, this should not happen
		DisableInterrupts();
		while(1){}
  }
}

unsigned long OS_LockScheduler(void){
  // lab 4 might need this for disk formating
  return 0;// replace with solution
}
void OS_UnLockScheduler(unsigned long previous){
  // lab 4 might need this for disk formating
}

// From ValvanoWare
void SysTick_Init(unsigned long period){
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_RELOAD_R = period-1;// reload value
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
                              // enable SysTick with core clock and interrupts
  NVIC_ST_CTRL_R = 0x07;
}

/**
 * @details  Initialize operating system, disable interrupts until OS_Launch.
 * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
 * Interrupts not yet enabled.
 * @param  none
 * @return none
 * @brief  Initialize OS
 */
void OS_Init(void){
  // put Lab 2 (and beyond) solution here
  int tmp = StartCritical();
	Heap_Init();
	// allocate root process
	OS_AddProcess(NULL, NULL, NULL, NULL, ROOT_PROCESS_PRIORITY);
	// grab root process and update currentProcess
	currentProcess = processTable[ROOT_PROCESS_PRIORITY];
	
  Timer2A_Init(MsTime_TimerHandler, 80000, 6);
  Timer1A_Init(OSTime_Handler, 0xFFFFFFFF, 6);

  PLL_Init(Bus80MHz);
  OS_ClearMsTime();
  
  ST7735_InitR(INITR_REDTAB);
  UART_Init();
  
  for (int i = 0; i < UTIL_AVERAGE; i++){
    CPUUtil[i] = 0;
  }
  
  OS_UpdateCPUUtil();
  OS_AddThread(Idle, 2048, 7);
  EndCritical(tmp);
}; 

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
  // put Lab 2 (and beyond) solution here
  int tmp = StartCritical();
  semaPt->Value = value;
  for (int i = 0; i < PRIORITY_LEVELS; i++){
    semaPt->blockedThreadTable[i] = 0;
  }
  EndCritical(tmp);
}; 

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
  int tmp = StartCritical();
  semaPt->Value--;

  if (semaPt->Value >= 0){
    // We did not block, continue!
    EndCritical(tmp);
    return;
  } else {
    // We blocked :(

    // Thread mode is IPSR[8:0] == 0x0
    if (GetIPSRMasked() == 0x0){
      // foreground, block!
      
      
      currentProcess->threadTable[currTCB->priority] = removeTCBFromList(currTCB, currentProcess->threadTable[currTCB->priority]);

      // append to back of blocked bucket
      semaPt->blockedThreadTable[currTCB->priority] = appendTCBToList(semaPt->blockedThreadTable[currTCB->priority], currTCB, false);
      OS_Scheduler(true);
      EnableInterrupts();
      ContextSwitch();
      EndCritical(tmp);
      return;
    } else {
      // background thread, this is an illegal operation
      DisableInterrupts();
      while(1) {}
    }
  }
}

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
  int tmp = StartCritical();
  semaPt->Value++;

  if (semaPt->Value <= 0){
    // was negative before, time to wake up!
    TCB* wokenTCB = getNextPrioTCBFromTableAndRemove(semaPt->blockedThreadTable);
    
    if (wokenTCB == NULL){
      // This should never happen
      DisableInterrupts();
      while(1) {}
    } else {
      wokenTCB->parentProcess->threadTable[wokenTCB->priority] = appendTCBToList(wokenTCB->parentProcess->threadTable[wokenTCB->priority], wokenTCB, false);
      
      // Thread mode is IPSR[8:0] == 0x0
      if (GetIPSRMasked() == 0x0){
        // foreground, switch!
        OS_Scheduler(false);
        EnableInterrupts();
        ContextSwitch();
        EndCritical(tmp);
        return;
      }
    }
  }
  EndCritical(tmp);
}

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
  int tmp = StartCritical();

  if (semaPt->Value == 1){
    semaPt->Value = 0;
    EndCritical(tmp);
    return;
  } else {
    // Thread mode is IPSR[8:0] == 0x0
    if (GetIPSRMasked() == 0x0){
      // foreground, block!
      
      currentProcess->threadTable[currTCB->priority] = removeTCBFromList(currTCB, currentProcess->threadTable[currTCB->priority]);

      // append to back of blocked bucket
      semaPt->blockedThreadTable[currTCB->priority] = appendTCBToList(semaPt->blockedThreadTable[currTCB->priority], currTCB, false);
      OS_Scheduler(true);
			EnableInterrupts();
      ContextSwitch();
      EndCritical(tmp);
      return;
    } else {
      // background thread, this is an illegal operation
      DisableInterrupts();
      while(1) {}
    }
  }
}

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
  int tmp = StartCritical();

  // was negative before, time to wake up!
  TCB* wokenTCB = getNextPrioTCBFromTableAndRemove(semaPt->blockedThreadTable);
  if (wokenTCB == NULL){
    semaPt->Value = 1;
    EndCritical(tmp);
    return;
  } else {
    wokenTCB->parentProcess->threadTable[wokenTCB->priority] = appendTCBToList(wokenTCB->parentProcess->threadTable[wokenTCB->priority], wokenTCB, false);
    
    // Thread mode is IPSR[8:0] == 0x0
    if (GetIPSRMasked() == 0x0){
      // foreground, switch!
      OS_Scheduler(false);
      EnableInterrupts();
      ContextSwitch();
      EndCritical(tmp);
    } else {
      // Interrupt handler, just return
      EndCritical(tmp);
      return;
    }
  }
}

// int32_t getNextAvailTCB(){
//   int32_t tempVector = tcbVectorMask;
//   int32_t index = 0;
  
//   while ((tempVector & 0x1) != 1 && index < TCB_VECTOR_SIZE){ index++; tempVector = tempVector >> 1; }
  
//   if (index > TCB_VECTOR_SIZE){
//     return -1;
//   } else {
//     return index;
//   }
    
// }


int AddThreadHelper(PCB* process, void(*task)(void), 
   uint32_t stackSize, uint32_t priority){
long tmp = StartCritical();

  // int32_t nextTCBVector = getNextAvailTCB();
  // if (nextTCBVector == -1){
  //   EndCritical(tmp);
  //   return 0;
  // }
     
  uint32_t stackSizeDWords = stackSize / 8;
	
  TCB* newTCB = (TCB*) Heap_Calloc(sizeof(TCB));
	
  if (newTCB == NULL){EndCritical(tmp); return 0;}
	
	int32_t* newStack = (int32_t*) Heap_Calloc(stackSize);
	
	if (newStack == NULL){Heap_Free(newTCB); EndCritical(tmp); return 0;}
  
	newTCB->stack = newStack;
	newTCB->parentProcess = process;
  newTCB->id = tcb_id_count++;
  newTCB->sp = &(newTCB->stack[stackSizeDWords - 16]); // intialize stack pointer
	newTCB->stack[stackSizeDWords - 11] = (uintptr_t) process->dataSegment; //R9 = start of data segment
  newTCB->stack[stackSizeDWords - 2] = (uintptr_t)task; // set PC on stack
  newTCB->stack[stackSizeDWords - 1] = 1 << 24; // set PSR on stack with thumb bit
  newTCB->sleepTime = 0;
  newTCB->priority = priority;
  
  process->threadTable[priority] = appendTCBToList(process->threadTable[priority], newTCB, false);
 
  
  EndCritical(tmp);
  return 1;
}

//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void), 
   uint32_t stackSize, uint32_t priority){
  // put Lab 2 (and beyond) solution here
  return AddThreadHelper(currentProcess, task, stackSize, priority);
};

//******** OS_AddProcess *************** 
// add a process with foregound thread to the scheduler
// Inputs: pointer to a void/void entry point
//         pointer to process text (code) segment
//         pointer to process data segment
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this process can not be added
// This function will be needed for Lab 5
// In Labs 2-4, this function can be ignored
int OS_AddProcess(void(*entry)(void), void *text, void *data, 
  uint32_t stackSize, uint8_t processPriority){
    
  int tmp = StartCritical();
	
	// check if we are overwriting a process priority level
	for (int i = processPriority; i < PRIORITY_LEVELS; i++){
		if (processTable[i] == NULL){
			processPriority = i;
			break;
		}
	}
	if (processPriority == PRIORITY_LEVELS){
		// no available slot
		EndCritical(tmp);
		return 0;
	}
	
  PCB* newPCB = Heap_Calloc(sizeof(PCB));
  if (newPCB == NULL){
    EndCritical(tmp);
    return 0;
  }
  
  newPCB->priority = processPriority;
  newPCB->codeSegment = text;
  newPCB->dataSegment = data;
  newPCB->id = pcb_id_count++;
	
	// add PCB to processTable
	processTable[processPriority] = newPCB;
  
  // create the base thread for the new process
	if (entry == NULL){
		// we are making an empty process
		EndCritical(tmp);
		return 1;
	} 
	
  if (AddThreadHelper(newPCB, entry, stackSize, 7) == 0){
    // failed
    Heap_Free(newPCB);
    EndCritical(tmp);
    return 0;
  }
 
  EndCritical(tmp);
  return 1;
}




//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In lab 1, this command will be called 1 time
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddPeriodicThread(void(*task)(void), 
   uint32_t period, uint32_t priority){
  // put Lab 2 (and beyond) solution here
		 // Timer 3 = 0x04
		 // Timer 4 = 0x08
		 int tmp = StartCritical();
     
		 static int16_t PeriodicTimerStatus = 0;
		 
		 if ((PeriodicTimerStatus & 0x04) == 0) { // check whether timer 4 is being used
			 PeriodicTimerStatus |= 0x04; // Timer 3 activated
			 
			 Timer4A_Init(task, period, priority);
       //jitter1TaskPtr = task;
			 EndCritical(tmp);
			 return 1; // successful
		 }
		 else if ((PeriodicTimerStatus & 0x08) == 0) { // check whether timer 8 is being used
			 PeriodicTimerStatus |= 0x08; // Timer 4 activated
			 
			 Timer5A_Init(task, period, priority);
       //jitter2TaskPtr = task;
			 EndCritical(tmp);
			 return 1; // successful
		 }
		 else {
			EndCritical(tmp);
			 return 0; // unsuccessful, all Hardware timers being used
		 }
};

int OS_AddMSPeriodicThread(void(*task)(void), uint32_t period){
  // iterate through slots to find empty
  for (int i = 0; i < MS_THREAD_NUM; i++){
    if (MSThreads[i].task == 0){
      MSThreads[i].task = task;
      MSThreads[i].period = period;
      MSThreads[i].currentCountdown = period;
      return 1;
    }
  }
  return 0;
}


uint8_t highPriority = 8;
uint8_t sw1priority = 8;
uint8_t sw2priority = 8;

/*----------------------------------------------------------------------------
  PF0,4 Interrupt Handler
 *----------------------------------------------------------------------------*/
void GPIOPortF_Handler(void){
	// SW1
	int tmp = StartCritical();

  if (SW1Task != NULL){
    if (SW2Task != NULL){
      // both active
      if (sw1priority < sw2priority){
        // sw1 is higher prio
        if (GPIO_PORTF_RIS_R & (1 << 4)){
          SW1Task();
          GPIO_PORTF_ICR_R |= (1 << 4); // acknowledge flag
        }
        if (GPIO_PORTF_RIS_R & (1 << 0)){
          SW2Task();
          GPIO_PORTF_ICR_R |= (1 << 0); // acknowledge flag
        }
      } else {
        // sw2 is higher prio
        if (GPIO_PORTF_RIS_R & (1 << 0)){
          SW2Task();
          GPIO_PORTF_ICR_R |= (1 << 0); // acknowledge flag
        }
        if (GPIO_PORTF_RIS_R & (1 << 4)){
          SW1Task();
          GPIO_PORTF_ICR_R |= (1 << 4); // acknowledge flag
        }
      }
    } else {
      // sw1 is active
      if (GPIO_PORTF_RIS_R & (1 << 4)){
          SW1Task();
          GPIO_PORTF_ICR_R |= (1 << 4); // acknowledge flag
        }
    }
  } else {
    if (SW2Task != NULL){
      // sw2 is active
      if (GPIO_PORTF_RIS_R & (1 << 0)){
          SW2Task();
          GPIO_PORTF_ICR_R |= (1 << 0); // acknowledge flag
        }
    }
  }
  EndCritical(tmp);
}


//******** OS_AddSW1Task *************** 
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW2Task(void(*task)(void), uint8_t priority){
  // put Lab 2 (and beyond) solution here
  int tmp = StartCritical();
	SYSCTL_RCGCGPIO_R |= 0x20; // activate clock for Port F
	while ((SYSCTL_RCGCGPIO_R & 0x20) == 0) {} // allow time for clock to stabilize
	SW2Task  = task;
	
	GPIO_PORTF_LOCK_R = 0x4C4F434B; // unlock GPIO Port F
	GPIO_PORTF_CR_R = 0x1F; // allow changes to PF4-0
		
	GPIO_PORTF_AMSEL_R &= ~(1 << 0); // disable analog PF4
	GPIO_PORTF_PCTL_R &= ~(1 << 0); // configure PF4 as GPIO
		
	GPIO_PORTF_DIR_R &= ~(1 << 0); // make PF4 in
	GPIO_PORTF_AFSEL_R &= ~(1 << 0); // disable alt function on PF4
	GPIO_PORTF_PUR_R |= (1 << 0); // pullup on PF4 PF4
	GPIO_PORTF_DEN_R |= (1 << 0); // enable digital I/O on PF4
		
	GPIO_PORTF_IS_R &= ~(1 << 0);
	GPIO_PORTF_IBE_R &= (1 << 0); // PF4,PF0 is not both edges
	GPIO_PORTF_IEV_R &= (1 << 0); // Falling edge event
	GPIO_PORTF_ICR_R = (1 << 0); // clear flags
	GPIO_PORTF_IM_R |= (1 << 0); // arm interrupts on PF4,PF0
	
  if (priority < highPriority){
    NVIC_PRI7_R = (NVIC_PRI7_R & ~NVIC_PRI7_INT30_M)|(priority << NVIC_PRI7_INT30_S);
    highPriority = priority;
    sw2priority = priority;
  }
	NVIC_EN0_R = (1 << 30); // enable interrupt 30 in NVIC
  EndCritical(tmp);
	return 1;
};

//******** OS_AddSW2Task *************** 
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), uint8_t priority){
  // put Lab 2 (and beyond) solution here
    
  int tmp = StartCritical();
	SYSCTL_RCGCGPIO_R |= 0x20; // activate clock for Port F
	while ((SYSCTL_RCGCGPIO_R & 0x20) == 0) {} // allow time for clock to stabilize
	SW1Task  = task;
	
	GPIO_PORTF_LOCK_R = 0x4C4F434B; // unlock GPIO Port F
	GPIO_PORTF_CR_R = 0x1F; // allow changes to PF4-0
		
	GPIO_PORTF_AMSEL_R &= ~(1 << 4); // disable analog PF4
	GPIO_PORTF_PCTL_R &= ~(1 << 4); // configure PF4 as GPIO
		
	GPIO_PORTF_DIR_R &= ~(1 << 4); // make PF4 in
	GPIO_PORTF_AFSEL_R &= ~(1 << 4); // disable alt function on PF4
	GPIO_PORTF_PUR_R |= (1 << 4); // pullup on PF4 PF4
	GPIO_PORTF_DEN_R |= (1 << 4); // enable digital I/O on PF4
		
	GPIO_PORTF_IS_R &= ~(1 << 4);
	GPIO_PORTF_IBE_R &= (1 << 4); // PF4,PF0 is not both edges
	GPIO_PORTF_IEV_R &= (1 << 4); // Falling edge event
	GPIO_PORTF_ICR_R = (1 << 4); // clear flags
	GPIO_PORTF_IM_R |= (1 << 4); // arm interrupts on PF4,PF0
	
	if (priority < highPriority){
    NVIC_PRI7_R = (NVIC_PRI7_R & ~NVIC_PRI7_INT30_M)|(priority << NVIC_PRI7_INT30_S);
    highPriority = priority;
    sw1priority = priority;
  }

	NVIC_EN0_R = (1 << 30); // enable interrupt 30 in NVIC
  EndCritical(tmp);
	return 1;


};


// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
  int temp = StartCritical();
  
  // remove from active thread list
  currTCB->parentProcess->threadTable[currTCB->priority] = removeTCBFromList(currTCB, currTCB->parentProcess->threadTable[currTCB->priority]);

  
  // add to sleep list
  currTCB->parentProcess->sleepList = appendTCBToList(currTCB->parentProcess->sleepList, currTCB, false);
  
	currTCB->sleepTime = sleepTime;
	
  // get next TCB
  OS_Scheduler(true);
 

  EnableInterrupts(); // let any background thread interrupt and allows context switch to work
  ContextSwitch();
  EndCritical(temp);
};  

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
  // put Lab 2 (and beyond) solution here
  int temp = StartCritical();
 
  currentProcess->threadTable[OS_Priority()] = removeTCBFromList(currTCB, currentProcess->threadTable[OS_Priority()]);
	
	// check if this process has any remaining threads
	bool hasThreads = false;
	hasThreads |= currentProcess->sleepList != NULL;
	for (int i = 0; i < PRIORITY_LEVELS; i++){
		hasThreads |= currentProcess->threadTable[i] != NULL;
	}
	
	if (!hasThreads){
		// process has no more threads, remove it from the process table and free its memory
		processTable[currentProcess->priority] = NULL;
		Heap_Free(currentProcess->codeSegment);
		Heap_Free(currentProcess->dataSegment);
		Heap_Free(currentProcess);
	}
  OS_Scheduler(true);
	
	Heap_Free(currTCB->stack);
  Heap_Free(currTCB);


  /**
  for (int i = 0; i < TCB_VECTOR_SIZE; i++){
    if (&tcbVector[i] == currTCB){
      tcbVectorMask |= 1 << i;
      break;
    }
  }
  **/
	
	// bug fix for SVC_OS_Kill not triggering PendSV, let the SVC handle PendSV
	if (GetIPSRMasked() != 0){
		return;
	}

  EnableInterrupts(); // let any background thread interrupt and allows context switch to work
  HalfSwitch();
  for(;;){};        // can not return
    
}; 

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
  // put Lab 2 (and beyond) solution here
  int tmp = StartCritical();
  OS_Scheduler(false);
  EnableInterrupts();
  ContextSwitch();
  EndCritical(tmp);
};

  
// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(FIFO* fifo, uint32_t size){
  int tmp = StartCritical();
	// TODO CHANGE BACK
	fifo->buffer = Heap_Calloc(size * sizeof(uint32_t));
  OS_InitSemaphore(&fifo->fifoRemainingCapacity, size);
  OS_InitSemaphore(&fifo->fifoAvailElements, 0);
  OS_InitSemaphore(&fifo->fifoInUse, 1);
  fifo->fifoHead = 0;
  fifo->fifoTail = 0;
  fifo->fifoUserSize = size;
  EndCritical(tmp);
};

void OS_Fifo_Reset(FIFO* fifo){
  fifo->fifoHead = 0;
  fifo->fifoTail = 0;
  fifo->fifoAvailElements.Value = 0;
  fifo->fifoRemainingCapacity.Value = fifo->fifoUserSize;
}

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
uint32_t OS_Fifo_Size(FIFO* fifo){
  int tmp = StartCritical();
  uint32_t out = fifo->fifoAvailElements.Value;
  EndCritical(tmp);
  return out;
};

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
bool OS_Fifo_Put(FIFO* fifo, uint32_t data){
  int tmp = StartCritical();
  
  if (fifo->fifoRemainingCapacity.Value <= 0){
    EndCritical(tmp);
    return false;
  } else {
    fifo->buffer[fifo->fifoHead++ % fifo->fifoUserSize] = data;
    fifo->fifoRemainingCapacity.Value--;
    OS_Signal(&fifo->fifoAvailElements);
    EndCritical(tmp);
    return true;
  }
};  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
volatile uint32_t errors = 0;
uint32_t OS_Fifo_Get(FIFO* fifo){
  int tmp = StartCritical();
  
  OS_Wait(&fifo->fifoAvailElements);
  //OS_bWait(&fifo->fifoInUse);

  uint32_t out = fifo->buffer[fifo->fifoTail++ % fifo->fifoUserSize];
  
  //OS_bSignal(&fifo->fifoInUse);
  OS_Signal(&fifo->fifoRemainingCapacity);
  
  EndCritical(tmp);
  return out;
};

uint32_t OS_Fifo_Get_Nonblock(FIFO* fifo){
  int tmp = StartCritical();
  if (fifo->fifoAvailElements.Value <= 0){
    EndCritical(tmp);
    return NULL;
  } else {
    uint32_t out = fifo->buffer[fifo->fifoTail++ % fifo->fifoUserSize];
    fifo->fifoAvailElements.Value--;
    OS_Signal(&fifo->fifoRemainingCapacity);
    //fifo->fifoRemainingCapacity.Value++;
    EndCritical(tmp);
    return out;
  }
}

void OS_Fifo_Put_Block(FIFO* fifo, uint32_t data){
  int tmp = StartCritical();
  
  OS_Wait(&fifo->fifoRemainingCapacity);
  //OS_bWait(&fifo->fifoInUse);
  
  fifo->buffer[fifo->fifoHead++ % fifo->fifoUserSize] = data;
  
  //OS_bSignal(&fifo->fifoInUse);
  OS_Signal(&fifo->fifoAvailElements);
  
  EndCritical(tmp);
}

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: nones
void OS_MailBox_Init(Mailbox* mailbox){
  int tmp = StartCritical();
  mailbox->dataValid.Value = 0;
  mailbox->boxFree.Value = 1;
  EndCritical(tmp);
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(Mailbox* mailbox, uint32_t data){
  int tmp = StartCritical();
  OS_bWait(&mailbox->boxFree);
  mailbox->data = data;
  OS_bSignal(&mailbox->dataValid);
  EndCritical(tmp);
};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(Mailbox* mailbox){
  int tmp = StartCritical();
  OS_bWait(&mailbox->dataValid);
  uint32_t data = mailbox->data;
  OS_bSignal(&mailbox->boxFree);
  EndCritical(tmp);
  return data;
};

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint64_t OS_Time(void){
  int tmp = StartCritical();
  uint64_t out = ((uint64_t)(OSTime_Overflows) << 32) + (uint64_t)(0xFFFFFFFF - Timer1A_GetValue());
  EndCritical(tmp);
  return out;
};

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
uint64_t OS_TimeDifference(uint64_t start, uint64_t stop){
  return stop - start;
};


// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
  OSTimeMs = 0;
};

// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
  return OSTimeMs;
};


//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(uint32_t theTimeSlice){
    OS_Scheduler(false);
    SysTick_Init(theTimeSlice);
    OS_ResetStats();
    EnableInterrupts();
    StartOS();
};

//************** I/O Redirection *************** 
// redirect terminal I/O to UART or file (Lab 4)

int StreamToDevice=0;                // 0=UART, 1=stream to file (Lab 4), 2=ESP

FIL* putc_handler;

int fputc (int ch, FILE *f) { 
  if(StreamToDevice==1){  // Lab 4
    if(eFile_Write(putc_handler, ch)){          // close file on error
       OS_EndRedirectToFile(); // cannot write to file
       return 1;                  // failure
    }
    return 0; // success writing
  } else if (StreamToDevice == 2){
		ESP8266_BufferedChar(ch);
		return 0;
	} else {
		// default UART output
		UART_OutChar(ch);
		return 0; 
	}
}

int fgetc (FILE *f){
  char ch = UART_InChar();  // receive from keyboard
  UART_OutChar(ch);         // echo
  return ch;
  //return 0;
}

int OS_RedirectToFile(const char *name){  // Lab 4
  eFile_Create(name);              // ignore error if file already exists
	putc_handler = eFile_WOpen(name);
  if(putc_handler == NULL) return 1;  // cannot open file
  StreamToDevice = 1;
  return 0;
}

int OS_EndRedirectToFile(void){  // Lab 4
  StreamToDevice = 0;
  if(eFile_WClose(putc_handler)) return 1;    // cannot close file
  return 0;
}

int OS_RedirectToUART(void){
  StreamToDevice = 0;
  return 0;
}

int OS_RedirectToESP(){
	StreamToDevice = 2;
	return 0;
}

int OS_RedirectToST7735(void){
  
  return 1;
}
