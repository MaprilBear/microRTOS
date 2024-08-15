#pragma once
#include <stdint.h>

#define JITTERSIZE 64

struct Jitter_Struct{
  uint32_t taskID;
  int32_t MaxJitter;             // largest time jitter between interrupts in usec
  uint64_t LastTime;
  uint32_t period;
  uint32_t jitterSize;
  uint32_t JitterHistogram[JITTERSIZE];
}; typedef struct Jitter_Struct Jitter_t;

Jitter_t Jitter_Create(uint32_t period);

void Jitter_Log(Jitter_t* j);