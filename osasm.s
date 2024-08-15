;/*****************************************************************************/
;/* OSasm.s: low-level OS commands, written in assembly                       */
;/* derived from uCOS-II                                                      */
;/*****************************************************************************/
;Jonathan Valvano, OS Lab2/3/4/5, 1/12/20
;Students will implement these functions as part of EE445M/EE380L.12 Lab

        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  currTCB            ; currently running thread
        EXTERN  nextTCB            ; next thread selected by the scheduler
		EXTERN 	OS_Scheduler

        EXPORT  StartOS
        EXPORT  ContextSwitch
        EXPORT  PendSV_Handler
        EXPORT  SVC_Handler
		EXPORT 	SysTick_Handler
		EXPORT  HalfSwitch
        EXPORT  GetIPSRMasked
			
NVIC_INT_CTRL   EQU     0xE000ED04                              ; Interrupt control state register.
NVIC_SYSPRI14   EQU     0xE000ED22                              ; PendSV priority register (position 14).
NVIC_SYSPRI15   EQU     0xE000ED23                              ; Systick priority register (position 15).
NVIC_LEVEL14    EQU           0xEF                              ; Systick priority value (second lowest).
NVIC_LEVEL15    EQU           0xFF                              ; PendSV priority value (lowest).
NVIC_PENDSVSET  EQU     0x10000000                              ; Value to trigger PendSV exception.
MAGIC			EQU		0x12345678
RETURN_VAL		EQU		0xFFFFFFF9

GetIPSRMasked
    MRS R0, IPSR
    MOV R1, #0x1FF
    AND R0, R0, R1
    BX LR


StartOS
; Load first thread defined at nextTCB
	B 		HalfSwitch
    BX      LR ; 10) restore R0-R3,R12,LR,PC,PSR

OSStartHang
    B       OSStartHang        ; Should never get here


;********************************************************************************************************
;                               PERFORM A CONTEXT SWITCH (From task level)
;                                           void ContextSwitch(void)
;
; Note(s) : 1) ContextSwitch() is called when OS wants to perform a task context switch.  This function
;              triggers the PendSV exception which is where the real work is done.
;********************************************************************************************************

HalfSwitch
	LDR 	R2, =MAGIC

ContextSwitch
    LDR     R0, =NVIC_INT_CTRL 
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]
    BX      LR
    
    

;********************************************************************************************************
;                                         HANDLE PendSV EXCEPTION
;                                     void OS_CPU_PendSVHandler(void)
;
; Note(s) : 1) PendSV is used to cause a context switch.  This is a recommended method for performing
;              context switches with Cortex-M.  This is because the Cortex-M3 auto-saves half of the
;              processor context on any exception, and restores same on return from exception.  So only
;              saving of R4-R11 is required and fixing up the stack pointers.  Using the PendSV exception
;              this way means that context saving and restoring is identical whether it is initiated from
;              a thread or occurs due to an interrupt or exception.
;
;           2) Pseudo-code is:
;              a) Get the process SP, if 0 then skip (goto d) the saving part (first context switch);
;              b) Save remaining regs r4-r11 on process stack;
;              c) Save the process SP in its TCB, OSTCBCur->OSTCBStkPtr = SP;
;              d) Call OSTaskSwHook();
;              e) Get current high priority, OSPrioCur = OSPrioHighRdy;
;              f) Get current ready thread TCB, OSTCBCur = OSTCBHighRdy;
;              g) Get new process SP from TCB, SP = OSTCBHighRdy->OSTCBStkPtr;
;              h) Restore R4-R11 from new process stack;
;              i) Perform exception return which will restore remaining context.
;
;           3) On entry into PendSV handler:
;              a) The following have been saved on the process stack (by processor):
;                 xPSR, PC, LR, R12, R0-R3
;              b) Processor mode is switched to Handler mode (from Thread mode)
;              c) Stack is Main stack (switched from Process stack)
;              d) OSTCBCur      points to the OS_TCB of the task to suspend
;                 OSTCBHighRdy  points to the OS_TCB of the task to resume
;
;           4) Since PendSV is set to lowest priority in the system (by OSStartHighRdy() above), we
;              know that it will only be run when no other exception or interrupt is active, and
;              therefore safe to assume that context being switched out was using the process stack (PSP).
;********************************************************************************************************

SysTick_Handler
	PUSH {R0, LR}
	MOV R0, #0 ; Tell The scheduler the currTCB has not been removed
	BL OS_Scheduler
	POP {R0, LR}

PendSV_Handler ; 1) Saves R0-R3,R12,LR,PC,PSR
    CPSID   I ; 2) Make atomic
	LDR R0, =MAGIC
	SUB R1, R2, R0
	CBZ R1, SkipSave
    PUSH    {R4-R11} ; 3) Save remaining regs r4-11
	LDR		R0, =currTCB
    LDR     R1, [R0] ; R1 = currTCB
    STR     SP, [R1, #4] ; 5) Save SP into TCB
SkipSave
	LDR     R0, =currTCB ; 4) R0=pointer to currTCB
    LDR     R1, =nextTCB
	LDR		R1, [R1]
    STR     R1, [R0] ; currTCB = nextTCB
    LDR     SP, [R1, #4] ; 7) new thread SP; SP=currTCB->sp;
    POP {R4-R11} ; 8) restore regs r4-11
	LDR LR, =RETURN_VAL
    CPSIE   I ; 9) tasks run enabled
    BX      LR ; 10) restore R0-R3,R12,LR,PC,PSR

;********************************************************************************************************
;                                         HANDLE SVC EXCEPTION
;                                     void OS_CPU_SVCHandler(void)
;
; Note(s) : SVC is a software-triggered exception to make OS kernel calls from user land. 
;           The function ID to call is encoded in the instruction itself, the location of which can be
;           found relative to the return address saved on the stack on exception entry.
;           Function-call paramters in R0..R3 are also auto-saved on stack on exception entry.
;********************************************************************************************************

        IMPORT    OS_Id         ; Vector 0
        IMPORT    OS_Kill       ; Vector 1
        IMPORT    OS_Sleep      ; Vector 2
        IMPORT    OS_Time       ; Vector 3
        IMPORT    OS_AddThread  ; Vector 4

SVC_Handler
    LDR R12,[SP,#24] ; Return address
		LDM SP,{R0-R3} ; Get any parameters
    PUSH {R12, LR} ; store magic LR
    LDRH R12,[R12,#-2] ; SVC instruction is 2 bytes
    BIC R12,#0xFF00 ; Extract ID in R12
    
    
    ; check for each OS call
    
OS_VECT_0_CHECK
    CMP R12, #0
    BNE OS_VECT_1_CHECK
    BL OS_Id
    B SVC_Exit
    
OS_VECT_1_CHECK
    CMP R12, #1
    BNE OS_VECT_2_CHECK
    BL OS_Kill
		CPSIE I
		LDR 	R2, =MAGIC
		B PendSV_Handler
    
OS_VECT_2_CHECK
    CMP R12, #2
    BNE OS_VECT_3_CHECK
    BL OS_Sleep
		B SVC_Exit
    
OS_VECT_3_CHECK
    CMP R12, #3
    BNE OS_VECT_4_CHECK
    BL OS_Time
    B SVC_Exit
    
OS_VECT_4_CHECK
    CMP R12, #4
    BNE SVC_Exit
    BL OS_AddThread
    B SVC_Exit
    
SVC_Exit
    POP   {R12, LR} ; grab magic LR
		STR R0, [SP]
    BX LR ; Return from exception

    ALIGN
    END
