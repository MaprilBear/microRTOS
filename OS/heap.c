// filename *************************heap.c ************************
// Implements memory heap for dynamic memory allocation.
// Follows standard malloc/calloc/realloc/free interface
// for allocating/unallocating memory.

// Jacob Egner 2008-07-31
// modified 8/31/08 Jonathan Valvano for style
// modified 12/16/11 Jonathan Valvano for 32-bit machine
// modified August 10, 2014 for C99 syntax

/* This example accompanies the book
   "Embedded Systems: Real Time Operating Systems for ARM Cortex M Microcontrollers",
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains

 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../RTOS_Labs_common/heap.h"
#include "../inc/CortexM.h"

#define HEAP_NEW_SIZE 4096 // heap size in bytes
#define HEAP_NEW_SIZE_WORDS HEAP_NEW_SIZE / 4
#define HEAP_NEW_START Heap_new
#define HEAP_NEW_END Heap_new + HEAP_NEW_SIZE_WORDS
#define abs(x) (x > 0 ? x : -x)
#define blockEnd(x) (x + abs(*x) + 1) 

static int16_t Heap_new[HEAP_NEW_SIZE_WORDS];

heap_stats_t stats_local;

// the implementation is a basic malloc alforithm
// the heap is divided into a series of blocks
// at initialization, the entire heap is one block
// for each block, the tag of either end is the size of the block
// a positive value means the block is used, a negative value means the block is free
// example heap

// location - value
// 0 - -6 - block tag
// 1 - 0 ---
// 2 - 0   |
// 3 - 0   |
// 4 - 0   | one free block 6 words wide
// 5 - 0   |
// 6 - 0 ---
// 7 - -6 - block tag

// now we allocate 3 words

// 0 - 3 - block tag
// 1 - 0 --
// 2 - 0  | one allocated 3 word block
// 3 - 0 --
// 4 - 3 - block tag
// 5 - -1   
// 6 - 0   -- one free block 1 word wide
// 7 - -1 

// to traverse the heap all we need to do is check the first block (at Heap[0])
// if it's free, split the block into a new allocated block and the remaining free space
// if it's allocated, skip over the number of words specified by the tag and check the next block

// this algorithm does result in fragmentation, thus if an allocation attempt fails
// we must take an expensive defragmentation pass
// this pass can either defragment the entire heap or just collect blocks until it reaches the neccessary size
// and then shuffles memory around to make these free blocks contiguous and combine them into one larger free block

void createBlock(int16_t* start, uint16_t size, bool allocated){
  *start = allocated ? size : -size;
  *blockEnd(start) = allocated ? size : -size;
}

void clearBlock(int16_t* start, uint16_t size){
  memset(start + 1, 0, sizeof(int32_t) * size);
}

// splits the block with supplied new block size, does not check for size errors
// returns the first block of the newly created pair
int16_t* splitBlock(int16_t* blockStart, uint16_t size){
  uint32_t currentBlockSize = abs(*blockStart);
  if (currentBlockSize == size){
    // same block size, allocate!
    createBlock(blockStart, size, true);
    return blockStart;
  } else if (currentBlockSize > size + 2){ 
    createBlock(blockStart, size, true);
    createBlock(blockStart + size + 2, currentBlockSize - size - 2, false); // account for the extra 2 tags
  } else if (currentBlockSize > size){
    // not big enough to form a free block, just make the block the whole thing
    createBlock(blockStart, currentBlockSize, true);
    stats_local.free -= currentBlockSize - size + 1;
    return blockStart;
  }
  stats_local.free -= 4;
  return blockStart;

}

// returns the index of the head tag of the next block
int16_t* nextBlock(int16_t* block){
  
  block = block + *block + 2; // skip over tag
  if (block > HEAP_NEW_END){
    block = HEAP_NEW_START;
  }
  return block;
}

// combines two contiguous free blocks
int16_t* combineFreeBlocks(int16_t* block1, int16_t* block2){
  stats_local.free += 4;
	if (block1 < block2){
		// block 1 on top
		uint16_t newSize = abs(*block1) + abs(*block2) + 2; // account for removed tags
		*block1 = -newSize;
		*blockEnd(block2) = -newSize;
		return block1;
	} else {
		// block 2 on top
		int16_t newSize = abs(*block2) + abs(*block1) + 2; // account for removed tags
		*block2 = -newSize;
		*blockEnd(block1) = -newSize;
		return block2;
	}
  
}

int16_t* freeBlock(int16_t* block){
	uint16_t size = abs(*block);
	*block = -size;
	*(block + size + 1) = -size; 
	
  bool changed = true;
  while (changed == true){
    changed = false;
    // try to combine with previous block
    if (block != HEAP_NEW_START && *(block - 1) < 0){
      // also free space, combine!
      block = combineFreeBlocks(block, block - 2 - abs(*(block - 1)));
      changed = true;
      continue;
    }
    
    // try to combine with next block
    if (blockEnd(block) + 1 < HEAP_NEW_END && *(blockEnd(block) + 1) < 0){
      block = combineFreeBlocks(block, blockEnd(block) + 1);
      changed = true;
      continue;
    }
  }
  return block;
}

//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always 0
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int Heap2_Init(void){
  stats_local.size = HEAP_NEW_SIZE / 2- 4;
  stats_local.used = 0;
  stats_local.free = HEAP_NEW_SIZE / 2 - 4;

  // create the first block
  createBlock(HEAP_NEW_START, HEAP_NEW_SIZE_WORDS - 2, false);
  return 0;
}


//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
void* Heap2_Malloc(int32_t desiredBytes){
  int16_t* currentBlock = HEAP_NEW_START;
  
  int16_t* out;
  
  if (stats_local.free < desiredBytes){ // check if we have enough space in HEAP
    return NULL;
  }
  
  do{
    if (*currentBlock < 0){
      // found free block, check size
      if (abs(*currentBlock) >= desiredBytes / 2 + (desiredBytes % 2 > 0 ? 1 : 0)){
        // allocate!
        splitBlock(currentBlock, desiredBytes / 2 + (desiredBytes % 2 > 0 ? 1 : 0));
        out = currentBlock + 1;
        break;
      }
    }
    currentBlock = nextBlock(currentBlock);
  } while (currentBlock != HEAP_NEW_START);
	
	if (out == NULL){
		// defrag, for now stop the OS
		DisableInterrupts();
		while (1) {}
	}
	
	stats_local.free -= desiredBytes + (desiredBytes % 2 > 0 ? 1 : 0);
	stats_local.used += desiredBytes + (desiredBytes % 2 > 0 ? 1 : 0);

  return out;
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Heap2_Calloc(int32_t desiredBytes){  
	int16_t* currentBlock;
  int16_t wordsToInitialize;
  int16_t i;
  
  // malloc a block of desiredBytes (testmain1 sets desiredBytes = 124)
  currentBlock = Heap2_Malloc(desiredBytes);
	
  if(currentBlock == 0){ // check if malloc failed
    return 0; // return NULL if malloc fails
  }
	
	memset(currentBlock, 0, desiredBytes);
	
  return currentBlock; // return pointer to allocated memory block
}

//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int Heap2_Free(void* pointer){
	int16_t* ptr = ((int16_t*) pointer) - 1;
	if (ptr < HEAP_NEW_START || ptr >= HEAP_NEW_END){
		return 1;
	}
	
	stats_local.free += abs(*ptr) * 2;
	stats_local.used -= abs(*ptr) * 2;

	freeBlock(ptr);
	
	return 0;
	
}


//******** Heap_Realloc *************** 
// Reallocate buffer to a new size
//input: 
//  oldBlock: pointer to a block
//  desiredBytes: a desired number of bytes for a new block
// output: void* pointing to the new block or will return NULL
//   if there is any reason the reallocation can't be completed
// notes: the given block may be unallocated and its contents
//   are copied to a new block if growing/shrinking not possible
void* Heap2_Realloc(void* oldBlock, int32_t desiredBytes){
  int16_t* block = (int16_t*) oldBlock; // copy of old block
	int16_t* newBlockPtr;
	int16_t wordsToCopy;
	int16_t i;
	
  if (desiredBytes > abs(*block)){
    // growing
    if (desiredBytes > stats_local.free){ // reallocation bigger than amount of free heap space
      // cannot grow
      return 0;
    } else {
			newBlockPtr = Heap2_Malloc(desiredBytes); // allocate new block
			
			if(newBlockPtr == 0){ // check if malloc failed
				return 0; // NULL
			}
			
			wordsToCopy = *(block); // we are growing so we copy everything that was in oldBlock
			for(i = 0; i < wordsToCopy; i++){
					newBlockPtr[i] = block[i];
			} // everything past will be unallocated space
			
			// unallocate given block
			Heap2_Free(block);
			return newBlockPtr; // return new block
    }
  } else {
    // shrinking
    if (desiredBytes == 0){
      Heap2_Free(block);
      return NULL;
    } else {
      // free contents past desiredBytes
			newBlockPtr = Heap2_Malloc(desiredBytes); // allocate new block
			if(newBlockPtr == 0){ // check if malloc failed
				return 0; // NULL
			}
			wordsToCopy = *(newBlockPtr - 1);
			for(i = 0; i < wordsToCopy; i++){
				newBlockPtr[i] = block[i];
			}
			
			// unallocate given block
			Heap2_Free(block);
			return newBlockPtr; // return new block
    }
  }
}





//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)
int Heap2_Stats(heap_stats_t *stats){
	stats->free = stats_local.free;
	stats->size = stats_local.size;
	stats->used = stats_local.used;
  return 0;   // replace
}
