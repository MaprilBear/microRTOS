
#include "Jitter.h"
#include "OS.h"

Jitter_t Jitter_Create(uint32_t period){
  Jitter_t j;
  
  j.LastTime = 0;
  j.jitterSize = JITTERSIZE;
  j.period = period;
  j.MaxJitter = 0;

  return j;
}

void Jitter_Log(Jitter_t* j){
  long jitter;
  long thisTime = OS_Time();       // current time, 12.5 ns
    if(j->LastTime != 0){    // ignore timing of first interrupt
      uint32_t diff = OS_TimeDifference(j->LastTime,thisTime);
      if(diff > j->period){
        jitter = (diff- j->period +4)/8;  // in 0.1 usec
      }else{
        jitter = (j->period - diff+4)/8;  // in 0.1 usec
      }
      if(jitter > j->MaxJitter){
        j->MaxJitter = jitter; // in usec
      }       // jitter should be 0
      if(jitter >= j->jitterSize){
        jitter = j->jitterSize-1;
      }
      j->JitterHistogram[jitter]++; 
    }
    j->LastTime = thisTime;
}