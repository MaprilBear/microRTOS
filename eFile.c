// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 
// Students implement these functions in Lab 4
// Jonathan W. Valvano 1/12/20
#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/ff.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../inc/CortexM.h"
#include "RTOS_Labs_common/OS.h"

#include <stdio.h>

#define FAT_TABLE_SIZE 1
#define BLOCK_NUM (512 * FAT_TABLE_SIZE / sizeof(FAT_Entry))
#define FILE_NUM 16
#define OPEN_FILE_LIMIT 2
#define DIRECTORY_LEVEL_LIMIT 8

// Display semaphore
extern Sema4Type LCDFree;  // this should really be handled in eDisk.c, not eFile.c

// Static file system objects
static FATFS g_sFatFs;
static DIR d; 
static FILINFO fi;

static FIL openFiles[OPEN_FILE_LIMIT];

void eFile_GetDirectoryString(char* buff, uint32_t len){
	f_getcwd(&d, buff, len);
}

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system

	// do nothing for FAT
  return 0;
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format
	OS_bWait(&LCDFree);
  if(f_mkfs("", 0, 0)){
		OS_bSignal(&LCDFree);
    return 1;
  }
	OS_bSignal(&LCDFree);  
  return 0;
}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // mount disk
	OS_bWait(&LCDFree);
  if(f_mount(&g_sFatFs, "", 0)){
		OS_bSignal(&LCDFree);
    return 1;
  }
	OS_bSignal(&LCDFree);  
  return 0;
}

FIL* grabNextAvailFile(){
	// do we have an available file handler?
	for (int i = 0; i < OPEN_FILE_LIMIT; i++){
		if (openFiles[i].fs == NULL){
			return &openFiles[i];
		}
	}
	return NULL;
}

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 
	OS_bWait(&LCDFree);
	FIL f;
  if(f_open(&f, name, FA_CREATE_NEW)){
		OS_bSignal(&LCDFree);
    return 1;
  }
  OS_bSignal(&LCDFree);
  return 0;   
}

int eFile_DCreate(const char name[NAME_CHAR_LIMIT - 4]){
	OS_bWait(&LCDFree);
  if(f_mkdir(name)){
		OS_bSignal(&LCDFree);
    return 1;
  }
  OS_bSignal(&LCDFree);
  return 0;  
}



//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
FIL* eFile_WOpen(const char name[NAME_CHAR_LIMIT]){      // open a file for writing 
	OS_bWait(&LCDFree);
	FIL* f = grabNextAvailFile();
	if (f == NULL) {
		OS_bSignal(&LCDFree);
    return NULL;
	}
  if(f_open(f, name, FA_WRITE)){
		OS_bSignal(&LCDFree);
    return NULL;
  }
  OS_bSignal(&LCDFree);
  return f; 
}


//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write(FIL* file, const char data){
		unsigned written;
  OS_bWait(&LCDFree);
  if(f_write(file, &data, 1, &written) || (written != 1)){
    OS_bSignal(&LCDFree);
    return 1;
  }
  OS_bSignal(&LCDFree);
  return 0;  
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(FIL* file){ // close the file for writing
	OS_bWait(&LCDFree);
  if(f_close(file)){
		OS_bSignal(&LCDFree);
    return 1;
  }
  OS_bSignal(&LCDFree);
  return 0; 
}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
FIL* eFile_ROpen(const char name[NAME_CHAR_LIMIT]){      // open a file for reading 
	OS_bWait(&LCDFree);
	FIL* f = grabNextAvailFile();
	if (f == NULL) {
		OS_bSignal(&LCDFree);
    return NULL;
	}
  if(f_open(f, name, FA_READ)){
		OS_bSignal(&LCDFree);
    return NULL;
  }
  OS_bSignal(&LCDFree);
  return f;
}
 
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext(FIL* file, char *pt){       // get next byte 
	unsigned read;
  OS_bWait(&LCDFree);
  if(f_read(file, pt, 1, &read) || (read != 1)){
    OS_bSignal(&LCDFree);
    return 1;
  }
  OS_bSignal(&LCDFree);
  return 0; 
}
    
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(FIL* file){ // close the file for writing
	OS_bWait(&LCDFree);
  if(f_close(file)){
		OS_bSignal(&LCDFree);
    return 1;
  }
  OS_bSignal(&LCDFree);
  return 0;
}


//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[NAME_CHAR_LIMIT]){  // remove this file 
	OS_bWait(&LCDFree);
  if(f_unlink(name)){
    OS_bSignal(&LCDFree);
    return 1;
  }
  OS_bSignal(&LCDFree);
  return 0;
}


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)

// directories are files but with a .dir file extension
// directories are shown to the user and searched for without the extension (it's hidden)

uint8_t dirindex = 0;

int eFile_DOpen( const char name[NAME_CHAR_LIMIT-4]){ // open directory
  OS_bWait(&LCDFree);
  if(f_opendir(&d, name)) {
		OS_bSignal(&LCDFree);
		return 1;
  }
	OS_bSignal(&LCDFree);
  return 0;
}

char buff3[64];
int eFile_DNavigateReset(){
	f_readdir(&d, NULL); // rewind directory
	return 0;
}

// returns the info of the next file in the directory
int eFile_DirNext(char *name[], unsigned long *size){
	OS_bWait(&LCDFree);
	if(f_readdir(&d, &fi) || !fi.fname[0]) {
		OS_bSignal(&LCDFree);
		return 1;
  }
  *name = fi.fname;
  *size = fi.fsize;
	OS_bSignal(&LCDFree);
  return 0;
}

//---------- eFile_DClose-----------------
// Close the directory
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_DClose(void){ // close the directory
  OS_bWait(&LCDFree);
  if(f_closedir(&d)){
		OS_bSignal(&LCDFree);
    return 1;
  }
  OS_bSignal(&LCDFree);
  return 0;
}


//---------- eFile_Unmount-----------------
// Unmount and deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently mounted)
int eFile_Unmount(void){ 
  OS_bWait(&LCDFree);
  if(f_mount(NULL, "", 0)){
		OS_bSignal(&LCDFree);
    return 1;
  }
	OS_bSignal(&LCDFree);  
  return 0;  
}
