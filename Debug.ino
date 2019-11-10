/*
 * Debug Beginning Of Line (BOL)
 * 
 * Outputs the first part of a DebugT* line
 * as per macro's in Debug.h
 * 
 * Includes times when USE_NTP_TIME is configured
 * 
 */

#include "Debug.h"

char _bol[128];
void _debugBOL(const char *fn, int line)
{
   
#ifdef USE_NTP_TIME
  sprintf(_bol, "[%02d:%02d:%02d] [%8u|%2d|%8u] %-12.12s(%4d): ", \
                hour(), minute(), second(), \
                ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize(),\
                fn, line);
#else
  sprintf(_bol, "[%8u] %-12.12s(%4d): ", \
                ESP.getFreeHeap(), fn, line); 
#endif
                 
   Serial.print (_bol);
   TelnetStream.print (_bol);
}
