/*
 * profiling/timing
 * 
 * timeThis(FN) 
 *  will profile any function FN given as argument
 *  
 * when PROFILING is not defined, it just executes the FN
 * when PROFILING is defined, it times the duration
 * when duration in above PROFILING_THRESHOLD it prints duration
 * 
 * PROFILING_THRESHOLD default to 3ms
 * 
 * defing PROFILING and PROFILING_THRESHOLD before including timing.h
 *  
 */
 
#ifdef PROFILING

#ifndef PROFILING_THRESHOLD
#define PROFILING_THRESHOLD 3
#endif

#define timeThis(FN)    ({ unsigned long st=millis(); \
                           FN; \
                           yield(); \
                           unsigned long duration=millis() - st; \
                           if (duration >= PROFILING_THRESHOLD) \
                            Debugf("function %s [called from %s:%d] took %ld ms\n",\
                                    #FN, __FUNCTION__, __LINE__, duration); \
                        })
#else

#define timeThis(FN)    FN ;

#endif

/*
 * DECLARE_TIMER(timername, interval)
 *  Declares two unsigned longs: 
 *    <timername>_last for last execution
 *    <timername>_interval for interval in seconds
 *    
 *    
 * DECLARE_TIMERms is same as DECLARE_TIMER **but** uses milliseconds!
 *    
 * DUE(timername) 
 *  returns false (0) if interval hasn't elapsed since last DUE-time
 *          true (current millis) if it has
 *  updates <timername>_last
 *  
 *  Usage example:
 *  
 *  DECLARE_TIMER(screenUpdate, 200) // update screen every 200 ms
 *  ...
 *  loop()
 *  {
 *  ..
 *    if ( DUE(screenUpdate) ) {
 *      // update screen
 *    }
 *  }
 */
 
#define DECLARE_TIMER(timerName, timerTime)    unsigned long timerName##_last = millis(), timerName##_interval = timerTime * 1000;
#define DECLARE_TIMERms(timerName, timerTime)  unsigned long timerName##_last = millis(), timerName##_interval = timerTime ;

#define DUE(timerName) (( (millis() - timerName##_last) < timerName##_interval) ? 0 : (timerName##_last=millis()))

//void waitAmSec(unsigned long ms)
//{
//  DECLARE_TIMERms ( theWait, ms )
//  while ( !DUE( theWait ))
//    yield();
//}
