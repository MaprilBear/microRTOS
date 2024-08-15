// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/loader.h"
#include "../RTOS_Labs_common/esp8266.h"

ELFSymbol_t symtab[] = { {"ST7735_Message", ST7735_Message}, {"UART_OutString", UART_OutString}, {"UART_OutChar2", UART_OutChar}, {"UART_InChar2", UART_InChar}, {"UART_OutUDec2", UART_OutUDec}}; 
ELFEnv_t env = { symtab, 5}; // symbol table with one entry

void Delay1ms2(uint32_t n){uint32_t volatile time;
  while(n){
    time = 72724*2/91;  // 1msec, tuned at 80 MHz
    while(time){
      time--;
    }
    n--;
  }
}

// Print jitter histogram
void Jitter(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
  // write this for Lab 3 (the latest)
	
}

// from https://stackoverflow.com/questions/9895216/how-to-remove-all-occurrences-of-a-given-character-from-string-in-c
void remove_all_chars(char* str, char c) {
    char *pr = str, *pw = str;
    while (*pr) {
        *pw = *pr++;
        pw += (*pw != c);
    }
    *pw = '\0';
}


// *********** Command line interpreter (shell) ************
char buff[128];
char buff2[64];
char prompt[64];
void Interpreter(bool useLCD, bool esp, void(*task)(void), Sema4Type* taskSema){ 
  uint8_t lineNumBOT = 0; // variable for line number on bottom half of screen
  uint8_t lineNumTOP = 0; // variable for line number on top half of screen

	if (esp){
		OS_RedirectToESP();
	}
	
  while (1){
		if (esp){
			ESP8266_Send("root:");
			eFile_GetDirectoryString(buff, 128);
			ESP8266_Send(buff);
			ESP8266_Send("$ ");
			memset(buff,0,sizeof(buff));
			ESP8266_Receive(prompt, 64);
			//ESP8266_Send("\n\r");
		} else {
			UART_OutString("root:");
			eFile_GetDirectoryString(buff, 128);
			UART_OutString(buff); UART_OutString("$ ");
			memset(buff,0,sizeof(buff));
			UART_InString(prompt, 64);
			remove_all_chars(prompt, 0x7F); // remove backspaces
			UART_Newline();
		}

    char* token = strtok(prompt, " ");
    
		char* temp; // will hold temporary file/directory names
		int result; // will provide confirmation of returns
		
		if (useLCD) {
			if (token != 0){
				if (strcmp(token, "format") == 0){
//					// uncomment for formatting 
//					result = eFile_Format(); // format
//					if (result == 0) { // format successful
//						ST7735_Message(1, lineNumTOP++, "Format Complete", 0x12345678); // debug tool
//					}
//					else { // otherwise it failed
//						ST7735_Message(1, lineNumTOP++, "Format Failed", 0x12345678); // debug tool
//					}
					ST7735_Message(1, lineNumTOP++, "test1", 0x12345678); // debugg tool
				}	else if (strcmp(token, "ls") == 0){
					ST7735_Message(1, lineNumTOP++, "test2", 0x12345678); // debug tool
					// call list directory function or similar to show list of files
					
					
				} else if (strcmp(token, "cf") == 0){
					ST7735_Message(1, lineNumTOP++, "test3", 0x12345678); // debug tool
					temp = strtok(0, ""); // save file name entered
					//ST7735_Message(1, lineNumTOP++, temp, 0x12345678); // debug tool
					
					// uncomment for file creation
//					result = eFile_Create(temp); // create file with name stored in temp
//					if (result == 0) {
//						ST7735_Message(1, lineNumTOP++, "File Created", 0x12345678); // debug tool
//					}
//					else {
//						ST7735_Message(1, lineNumTOP++, "Failed", 0x12345678); // debug tool
//					}
					
				} else if (strcmp(token, "pf") == 0) {
					ST7735_Message(1, lineNumTOP++, "test4", 0x12345678); // debugg tool
					temp = strtok(0, ""); // save file name entered
					//ST7735_Message(1, lineNumTOP++, temp, 0x12345678); // debug tool
					
					// Open file with eFile_ROpen(temp)) and read first block in to RAM. Perform checks to see if successful/failed.
					// Use eFile_ReadNext to read data from file until 

					
				} else if (strcmp(token, "df") == 0) {
					ST7735_Message(1, lineNumTOP++, "test5", 0x12345678); // debugg tool
					temp = strtok(0, ""); // save file name entered
					//ST7735_Message(1, lineNumTOP++, temp, 0x12345678); // debug tool
					
					// delete file and do checks to confirm whether file was deleted.
//					result = eFile_Delete(temp);
//					if (result == 0) {
//						ST7735_Message(1, lineNumTOP++, "File Deleted", 0x12345678); 
//					}
//					else {
//						ST7735_Message(1, lineNumTOP++, "Delete Failed", 0x12345678); 
//					}
					
				} else if (strcmp(token, "cd") == 0) {
					ST7735_Message(1, lineNumTOP++, "test6", 0x12345678); // debugg tool
					temp = strtok(0, ""); // save directory name entered
					//ST7735_Message(1, lineNumTOP++, temp, 0x12345678); // debug tool
					
					// ideally have a list that includes all possible directories to cd into
					// first check whether temp is one of the elements in that list
					// if it is, then perform the cd
					// if not, ST7735_Message(1, lineNumTOP++, "invalid cd", 0x12345678); 
					
					
				} else if (strcmp(token, "help") == 0){
					// helper list
					ST7735_Message(1, lineNumTOP++, "format", 0x12345678);
					ST7735_Message(1, lineNumTOP++, "ls", 0x12345678);
					ST7735_Message(1, lineNumTOP++, "cf [fname]", 0x12345678);
					ST7735_Message(1, lineNumTOP++, "pf [name]", 0x12345678);
					ST7735_Message(1, lineNumTOP++, "df [name]", 0x12345678);
					ST7735_Message(1, lineNumTOP++, "cd [name]", 0x12345678);
					ST7735_Message(1, lineNumTOP++, "clear [0/1/ ]", 0x12345678);
				} else if (strcmp(token, "clear") == 0){ // this is to clear the screen (clear will wipe entire screen) (clear x will clear a portion of the screen given x)
					token = strtok(NULL, "");
					if (token == NULL){
						for (int i = 0; i < 8; i++){ // loop through all lines and clear
							ST7735_Message(0, i, " ", 0x12345678);
							ST7735_Message(1, i, " ", 0x12345678);
						}
						// reset line variables
						lineNumBOT = 0;
						lineNumTOP = 0;
					} else if (*token == '0'){ // for bottom clear
						for (int i = 0; i < 8; i++){
							ST7735_Message(0, i, " ", 0x12345678);
							lineNumBOT = 0;
						}
					} else if (*token == '1'){ // for top clear
						for (int i = 0; i < 8; i++){
							ST7735_Message(1, i, " ", 0x12345678);
							lineNumTOP = 0;
						}
					} else {
						ST7735_Message(1, lineNumTOP++, "invalid", 0);
					}
				}
				else {
					lineNumTOP = 0;
					ST7735_Message(1, lineNumTOP, "invalid", 0);
				}
			}
		} else if (!esp){ // UART/ESP
			if (strcmp(token, "ping") == 0){
				UART_OutString("pong!"); UART_Newline(); // ping-pong
			} else if (strcmp(token, "lcd_top") == 0){
          ST7735_Message(1, lineNumTOP++, strtok(0, ""), 0x12345678); // output to LCD
			} else if (strcmp(token, "lcd_bot") == 0){ 
          ST7735_Message(0, lineNumBOT++, strtok(0, ""), 0x12345678); // output to LCD
			} else if (strcmp(token, "time") == 0){
					UART_OutUDec(OS_MsTime()); UART_Newline(); // output time
			} else if (strcmp(token, "adc_in") == 0){ // doesn't currently work
          //UART_OutUDec(ADC_In()); UART_Newline();
			//} else if (strcmp(token, "get_num_created") == 0){
          //UART_OutUDec(*numCreated); UART_Newline();
			} else if (strcmp(token, "help") == 0){
          UART_OutString("lcd_top [string]"); UART_Newline();
          UART_OutString("lcd_bot [string]"); UART_Newline();
          UART_OutString("time"); UART_Newline();
					UART_OutString("format"); UART_Newline();
					UART_OutString("ls"); UART_Newline();
					UART_OutString("touch [fname]"); UART_Newline();
					UART_OutString("rm [fname]"); UART_Newline();
					UART_OutString("mkdir [fname]"); UART_Newline();
					UART_OutString("cd [dirname]"); UART_Newline();
          //UART_OutString("adc_in"); UART_Newline();
          UART_OutString("ping"); UART_Newline();
          UART_OutString("clear [opt 0/1]"); UART_Newline();
        } else if (strcmp(token, "critical") == 0){
          UART_OutUDec(OS_GetCriticalTime()); UART_Newline();
        } else if (strcmp(token, "stats") == 0){
          if (strcmp(strtok(0, ""), "reset") == 0){
            OS_ResetStats();
          } else {
            uint32_t CPUUtil = OS_GetCPUUtil();
            uint64_t captureTime = OS_CaptureTime();
            //uint64_t criticalTime = OS_GetCriticalTime();
            //uint64_t percentageTime = (criticalTime * 10000) / captureTime;
            
            UART_OutString("Time Elapsed: "); UART_OutTime(captureTime); UART_Newline();
            UART_OutString("CPU Util: "); UART_OutUDec(CPUUtil / 100); UART_OutString("."); UART_OutUDec(CPUUtil % 100); UART_OutString("%"); UART_Newline();
            //UART_OutString("Critical Time: "); UART_OutUDec(criticalTime / 80000); UART_OutString("ms"); UART_Newline();
            //UART_OutString("Critical %: "); UART_OutUDec(percentageTime / 100); UART_OutString("."); UART_OutUDec(percentageTime % 100); UART_OutString("%"); UART_Newline();
            UART_Newline();
            //UART_OutString("Num Samples: "); UART_OutUDec(*numSamples); UART_Newline();
            //UART_OutString("Data Lost: "); UART_OutUDec(*dataLost); UART_Newline();
            //UART_OutString("DAS Jitter: "); UART_OutUDec(OS_GetJitter(DAS).MaxJitter); UART_Newline();
            //UART_OutString("PID Jitter: "); UART_OutUDec(OS_GetJitter(PID).MaxJitter); UART_Newline();
          }
        } else if (strcmp(token, "clear") == 0){
          UART_Newline();
        } else if (strcmp(token, "sudo") == 0){
          UART_OutString("Bro you're already root"); UART_Newline();
        }
        
        // heap
          else if (strcmp(token, "heap_stats") == 0){
						heap_stats2_t stats = Heap_Stats();
						UART_OutString("Blocks used  = "); UART_OutUDec(stats.blocksUnused); UART_Newline();
            UART_OutString("Blocks unused  = "); UART_OutUDec(stats.blocksUnused); UART_Newline();
            UART_OutString("Words allocated  = "); UART_OutUDec(stats.wordsAllocated); UART_Newline();
            UART_OutString("Words free  = "); UART_OutUDec(stats.wordsAvailable); UART_Newline();
						UART_OutString("Words wasted  = "); UART_OutUDec(stats.wordsOverhead); UART_Newline();
					}
				
				
				// file stuff (revised)
					else if (strcmp(token, "ls") == 0){
					char* fileName;
					unsigned long size;
          eFile_DNavigateReset();
					while (eFile_DirNext(&fileName, &size) == 0){
						UART_OutString(fileName); UART_OutChar(' ');
					}
					//eFile_DNavigateReset();
					UART_Newline();
				} else if (strcmp(token, "rm") == 0){ // delete file
					token = strtok(0, ""); // grab file name
					result = eFile_Delete(token);
					if (result == 0) { // checks
						UART_OutString("File deleted"); UART_Newline();
					}
					else {
						UART_OutString("Delete failed"); UART_Newline();
					}
				} else if (strcmp(token, "format") == 0){ // format disk
					result = eFile_Format();
					if (result == 0){ // checks
						UART_OutString("Format Successful"); UART_Newline();
					}
					else {
						UART_OutString("Format Failed"); UART_Newline();
					}
				} else if (strcmp(token, "touch") == 0){ // create file
					token = strtok(0, ""); // grab file name
					result = eFile_Create(token);
					if (result == 0){
						UART_OutString("File created"); UART_Newline();
					}
					else {
						UART_OutString("File creation failed"); UART_Newline();
					}
				} else if (strcmp(token, "mkdir") == 0){
					token = strtok(0, ""); // grab directory name
          eFile_GetDirectoryString(buff2, 64);
          char slash[2] = {'/', 0};
          strcat(buff2, slash);
          strcat(buff2, token);
          //strcat(buff2, slash);
					result = eFile_DCreate(buff2);
					if (result == 0){
						UART_OutString("Directory created"); UART_Newline();
					}
					else {
						UART_OutString("Directory creation failed"); UART_Newline();
					}
				} else if (strcmp(token, "cd") == 0){
					token = strtok(0, ""); // grab directory name
          eFile_GetDirectoryString(buff2, 64);
          char slash[2] = {'/', 0};
          if (buff2[1] != 0) {strcat(buff2, slash);};
          strcat(buff2, token);
					eFile_DOpen(buff2);
				} else if (strcmp(token, "cat") == 0){
					token = strtok(0, "");
          eFile_GetDirectoryString(buff2, 64);
          char slash[2] = {'/', 0};
          strcat(buff2, slash);
          strcat(buff2, token);
					FIL* file = eFile_ROpen(buff2);
					if (file != NULL){
						char buff;
						while (!eFile_ReadNext(file, &buff)){
							UART_OutChar(buff);
						}
						UART_Newline();
						eFile_RClose(file);
					}
				} else if (strcmp(token, "echo") == 0){
					token = strtok(0, ">");
          strcpy(buff, token); // copy text
          token = strtok(0, "");
          eFile_GetDirectoryString(buff2, 64);
          char slash[2] = {'/', 0};
          strcat(buff2, slash);
					strcat(buff2, token);
					if (token != NULL){
						eFile_Create(buff2);
						FIL* file = eFile_WOpen(buff2);
						if (file != NULL){
							for (int i = 0; i < 64; i++){
                if (buff2[i] == 0){
									break;
								} 
								eFile_Write(file, buff[i]); 
							}
							eFile_WClose(file);
						}
					} else {
						UART_OutString(buff2); UART_Newline();
					}
				} else if (strcmp(token, "exit") == 0){
          eFile_Unmount();
          OS_Kill();
        } else if (strcmp(token, "run") == 0){
          // ELF loading
					token = strtok(0, "");
					eFile_GetDirectoryString(buff2, 64);
					char slash[2] = {'/', 0};
          strcat(buff2, slash);
					strcat(buff2, token);
					if (token != NULL){
						if (exec_elf(buff2, &env)){
							OS_Suspend();
						}
					}
        }
      } else {
				if (strcmp(token, "ping") == 0){
					ESP8266_BufferedString("pong!"); ESP8266_BufferedString(ESP8266_Newline); // ping-pong
				} else if (strcmp(token, "lcd_top") == 0){
						ST7735_Message(1, lineNumTOP++, strtok(0, ""), 0x12345678); // output to LCD
				} else if (strcmp(token, "lcd_bot") == 0){ 
						ST7735_Message(0, lineNumBOT++, strtok(0, ""), 0x12345678); // output to LCD
				} else if (strcmp(token, "time") == 0){
						ESP8266_BufferedUDec(OS_MsTime()); ESP8266_BufferedString(ESP8266_Newline); // output time
				} else if (strcmp(token, "adc_in") == 0){ // doesn't currently work
						//UART_OutUDec(ADC_In()); ESP8266_BufferedString(ESP8266_Newline);
				//} else if (strcmp(token, "get_num_created") == 0){
						//UART_OutUDec(*numCreated); ESP8266_BufferedString(ESP8266_Newline);
				} else if (strcmp(token, "help") == 0){
						ESP8266_BufferedString("lcd_top [string]"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("lcd_bot [string]"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("time"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("format"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("ls"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("touch [fname]"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("rm [fname]"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("mkdir [fname]"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("cd [dirname]"); ESP8266_BufferedString(ESP8266_Newline);
						//ESP8266_BufferedString("adc_in"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("ping"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("clear [opt 0/1]"); ESP8266_BufferedString(ESP8266_Newline);
				} else if (strcmp(token, "critical") == 0){
					UART_OutUDec(OS_GetCriticalTime()); ESP8266_BufferedString(ESP8266_Newline);
				} else if (strcmp(token, "stats") == 0){
					if (strcmp(strtok(0, ""), "reset") == 0){
						OS_ResetStats();
					} else {
						uint32_t CPUUtil = OS_GetCPUUtil();
						uint64_t captureTime = OS_CaptureTime();
						//uint64_t criticalTime = OS_GetCriticalTime();
						//uint64_t percentageTime = (criticalTime * 10000) / captureTime;
						
						ESP8266_BufferedString("Time Elapsed: "); ESP8266_BufferedUDec(captureTime); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("CPU Util: "); ESP8266_BufferedUDec(CPUUtil / 100); ESP8266_BufferedString("."); ESP8266_BufferedUDec(CPUUtil % 100); ESP8266_BufferedString("%"); ESP8266_BufferedString(ESP8266_Newline);
						//ESP8266_BufferedString("Critical Time: "); UART_OutUDec(criticalTime / 80000); ESP8266_BufferedString("ms"); ESP8266_BufferedString(ESP8266_Newline);
						//ESP8266_BufferedString("Critical %: "); UART_OutUDec(percentageTime / 100); ESP8266_BufferedString("."); UART_OutUDec(percentageTime % 100); ESP8266_BufferedString("%"); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString(ESP8266_Newline);
						//ESP8266_BufferedString("Num Samples: "); UART_OutUDec(*numSamples); ESP8266_BufferedString(ESP8266_Newline);
						//ESP8266_BufferedString("Data Lost: "); UART_OutUDec(*dataLost); ESP8266_BufferedString(ESP8266_Newline);
						//ESP8266_BufferedString("DAS Jitter: "); UART_OutUDec(OS_GetJitter(DAS).MaxJitter); ESP8266_BufferedString(ESP8266_Newline);
						//ESP8266_BufferedString("PID Jitter: "); UART_OutUDec(OS_GetJitter(PID).MaxJitter); ESP8266_BufferedString(ESP8266_Newline);
					}
				} else if (strcmp(token, "clear") == 0){
					ESP8266_BufferedString(ESP8266_Newline);
				} else if (strcmp(token, "sudo") == 0){
					ESP8266_BufferedString("Bro you're already root"); ESP8266_BufferedString(ESP8266_Newline);
				}
				
				// heap
					else if (strcmp(token, "heap_stats") == 0){
						heap_stats2_t stats = Heap_Stats();
						ESP8266_BufferedString("Blocks used  = "); ESP8266_BufferedUDec(stats.blocksUnused); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("Blocks unused  = "); ESP8266_BufferedUDec(stats.blocksUnused); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("Words allocated  = "); ESP8266_BufferedUDec(stats.wordsAllocated); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("Words free  = "); ESP8266_BufferedUDec(stats.wordsAvailable); ESP8266_BufferedString(ESP8266_Newline);
						ESP8266_BufferedString("Words wasted  = "); ESP8266_BufferedUDec(stats.wordsOverhead); ESP8266_BufferedString(ESP8266_Newline);
				}
				
				
				// file stuff (revised)
					else if (strcmp(token, "ls") == 0){
					char* fileName;
					unsigned long size;
					eFile_DNavigateReset();
					while (eFile_DirNext(&fileName, &size) == 0){
						ESP8266_BufferedString(fileName); ESP8266_BufferedChar(' ');
					}
					//eFile_DNavigateReset();
					ESP8266_BufferedString(ESP8266_Newline);
				} else if (strcmp(token, "rm") == 0){ // delete file
					token = strtok(0, ""); // grab file name
					result = eFile_Delete(token);
					if (result == 0) { // checks
						ESP8266_BufferedString("File deleted"); ESP8266_BufferedString(ESP8266_Newline);
					}
					else {
						ESP8266_BufferedString("Delete failed"); ESP8266_BufferedString(ESP8266_Newline);
					}
				} else if (strcmp(token, "format") == 0){ // format disk
					result = eFile_Format();
					if (result == 0){ // checks
						ESP8266_BufferedString("Format Successful"); ESP8266_BufferedString(ESP8266_Newline);
					}
					else {
						ESP8266_BufferedString("Format Failed"); ESP8266_BufferedString(ESP8266_Newline);
					}
				} else if (strcmp(token, "touch") == 0){ // create file
					token = strtok(0, ""); // grab file name
					result = eFile_Create(token);
					if (result == 0){
						ESP8266_BufferedString("File created"); ESP8266_BufferedString(ESP8266_Newline);
					}
					else {
						ESP8266_BufferedString("File creation failed"); ESP8266_BufferedString(ESP8266_Newline);
					}
				} else if (strcmp(token, "mkdir") == 0){
					token = strtok(0, ""); // grab directory name
					eFile_GetDirectoryString(buff2, 64);
					char slash[2] = {'/', 0};
					strcat(buff2, slash);
					strcat(buff2, token);
					//strcat(buff2, slash);
					result = eFile_DCreate(buff2);
					if (result == 0){
						ESP8266_BufferedString("Directory created"); ESP8266_BufferedString(ESP8266_Newline);
					}
					else {
						ESP8266_BufferedString("Directory creation failed"); ESP8266_BufferedString(ESP8266_Newline);
					}
				} else if (strcmp(token, "cd") == 0){
					token = strtok(0, ""); // grab directory name
					eFile_GetDirectoryString(buff2, 64);
					char slash[2] = {'/', 0};
					if (buff2[1] != 0) {strcat(buff2, slash);};
					strcat(buff2, token);
					eFile_DOpen(buff2);
				} else if (strcmp(token, "cat") == 0){
					token = strtok(0, "");
					eFile_GetDirectoryString(buff2, 64);
					if (buff2[1] == 0){
						// root directory, dont append slash
					} else {
						char slash[2] = {'/', 0};
						strcat(buff2, slash);
					}
					strcat(buff2, token);
					FIL* file = eFile_ROpen(buff2);
					if (file != NULL){
						char buff;
						while (!eFile_ReadNext(file, &buff)){
							ESP8266_BufferedChar(buff);
						}
						eFile_RClose(file);
						ESP8266_FlushBufferBuffer();
						ESP8266_BufferedString(ESP8266_Newline);
					}
				} else if (strcmp(token, "echo") == 0){
					token = strtok(0, ">");
					strcpy(buff, token); // copy text
					token = strtok(0, "");
					eFile_GetDirectoryString(buff2, 64);
					char slash[2] = {'/', 0};
					strcat(buff2, slash);
					strcat(buff2, token);
					if (token != NULL){
						eFile_Create(buff2);
						FIL* file = eFile_WOpen(buff2);
						if (file != NULL){
							for (int i = 0; i < 64; i++){
								if (buff2[i] == 0){
									break;
								} 
								eFile_Write(file, buff[i]); 
							}
							eFile_WClose(file);
						}
					} else {
						ESP8266_BufferedString(buff); ESP8266_BufferedString(ESP8266_Newline);
					}
				} else if (strcmp(token, "exit") == 0){
					eFile_Unmount();
					OS_Kill();
				} else if (strcmp(token, "run") == 0){
					// ELF loading
					token = strtok(0, "");
					eFile_GetDirectoryString(buff2, 64);
					char slash[2] = {'/', 0};
					strcat(buff2, slash);
					strcat(buff2, token);
					if (token != NULL){
						if (exec_elf(buff2, &env)){
							OS_Suspend();
						}
					}
				}
				ESP8266_FlushBufferBuffer();
			}
		}
	}

//void Interpreter(bool useLCD){ 
//  uint8_t lineNumD0 = 0;
//  uint8_t lineNumD1 = 0;
//  // write this  
//  ST7735_Message(0, 0, "Hello, World!", 0);

//  while (1){
//    char command[64];
//    //UART_OutString("root:/$ ");
//    UART_InString(command, 64);
//    UART_Newline();

//    char* token = strtok(command, " ");
//    
//    if (useLCD){
//      if (token != 0){
//        if (strcmp(token, "ping") == 0){
//        ST7735_Message(1, lineNumD1++, "pong!", 0);
//        } else if (strcmp(token, "lcd_top") == 0){
//          ST7735_Message(1, lineNumD1++, strtok(0, ""), 0);
//        } else if (strcmp(token, "lcd_bot") == 0){
//          ST7735_Message(0, lineNumD0++, strtok(0, ""), 0);
//        } else if (strcmp(token, "time") == 0){
//          ST7735_Message(1, lineNumD1++, 0, OS_MsTime());
//        } else if (strcmp(token, "adc_in") == 0){
//          ST7735_Message(1, lineNumD1++, 0, ADC_In());
//        } else if (strcmp(token, "get_num_created") == 0){
//          //ST7735_Message(1, lineNumD1++, 0, *numCreated);
//        } else if (strcmp(token, "help") == 0){
//          ST7735_Message(1, lineNumD1++, "lcd_top [string]", 0);
//          ST7735_Message(1, lineNumD1++, "lcd_bot [string]", 0);
//          ST7735_Message(1, lineNumD1++, "time", 0);
//          ST7735_Message(1, lineNumD1++, "adc_in", 0);
//          ST7735_Message(1, lineNumD1++, "ping", 0);
//          ST7735_Message(1, lineNumD1++, "clear [0/1/ ]", 0);
//        } else if (strcmp(token, "clear") == 0){
//          token = strtok(NULL, "");
//          if (token == NULL){
//            for (int i = 0; i < 8; i++){
//              ST7735_Message(0, i, " ", 0x12345678);
//              ST7735_Message(1, i, " ", 0x12345678);
//              lineNumD0 = 0;
//              lineNumD1 = 0;
//            }
//          } else if (*token == '0'){
//            for (int i = 0; i < 8; i++){
//              ST7735_Message(0, i, " ", 0x12345678);
//              lineNumD0 = 0;
//            }
//          } else if (*token == '1'){
//            for (int i = 0; i < 8; i++){
//              ST7735_Message(1, i, " ", 0x12345678);
//              lineNumD1 = 0;
//            }
//          } else {
//            ST7735_Message(1, lineNumD1++, "invalid arg", 0);
//          }
//        }
//      }
//    } else {
//      if (strcmp(token, "ping") == 0){
//        UART_OutString("pong!"); UART_Newline();
//        } else if (strcmp(token, "lcd_top") == 0){
//          ST7735_Message(1, lineNumD1++, strtok(0, ""), 0);
//        } else if (strcmp(token, "lcd_bot") == 0){
//          ST7735_Message(0, lineNumD0++, strtok(0, ""), 0);
//        } else if (strcmp(token, "time") == 0){
//          UART_OutUDec(OS_MsTime()); UART_Newline();
//        } else if (strcmp(token, "adc_in") == 0){
//          UART_OutUDec(ADC_In()); UART_Newline();
//        } else if (strcmp(token, "get_num_created") == 0){
//          //UART_OutUDec(*numCreated); UART_Newline();
//        } else if (strcmp(token, "help") == 0){
//          UART_OutString("lcd_top [string]"); UART_Newline();
//          UART_OutString("lcd_bot [string]"); UART_Newline();
//          UART_OutString("time"); UART_Newline();
//          UART_OutString("adc_in"); UART_Newline();
//          UART_OutString("ping"); UART_Newline();
//          UART_OutString("clear [opt 0/1]"); UART_Newline();
//        } else if (strcmp(token, "critical") == 0){
//          UART_OutUDec(OS_GetCriticalTime()); UART_Newline();
//        } else if (strcmp(token, "stats") == 0){
//          if (strcmp(strtok(0, ""), "reset") == 0){
//            OS_ResetStats();
//          } else {
//            uint32_t CPUUtil = OS_GetCPUUtil();
//            uint64_t captureTime = OS_CaptureTime();
//            //uint64_t criticalTime = OS_GetCriticalTime();
//            //uint64_t percentageTime = (criticalTime * 10000) / captureTime;
//            
//            UART_OutString("Time Elapsed: "); UART_OutTime(captureTime); UART_Newline();
//            UART_OutString("CPU Util: "); UART_OutUDec(CPUUtil / 100); UART_OutString("."); UART_OutUDec(CPUUtil % 100); UART_OutString("%"); UART_Newline();
//            //UART_OutString("Critical Time: "); UART_OutUDec(criticalTime / 80000); UART_OutString("ms"); UART_Newline();
//            //UART_OutString("Critical %: "); UART_OutUDec(percentageTime / 100); UART_OutString("."); UART_OutUDec(percentageTime % 100); UART_OutString("%"); UART_Newline();
//            UART_Newline();
//            //UART_OutString("Num Samples: "); UART_OutUDec(*numSamples); UART_Newline();
//            //UART_OutString("Data Lost: "); UART_OutUDec(*dataLost); UART_Newline();
//            //UART_OutString("DAS Jitter: "); UART_OutUDec(OS_GetJitter(DAS).MaxJitter); UART_Newline();
//            //UART_OutString("PID Jitter: "); UART_OutUDec(OS_GetJitter(PID).MaxJitter); UART_Newline();
//          }
//        } else if (strcmp(token, "clear") == 0){
//          UART_Newline();
//        } else if (strcmp(token, "rm") == 0){
//          if (strcmp(strtok(0, ""), "-rf /") == 0){
//            UART_OutString("nuh uh"); UART_Newline();
//          }
//        } else if (strcmp(token, "sudo") == 0){
//          UART_OutString("Bro you're already root"); UART_Newline();
//        }
//      }
//  }
//}


