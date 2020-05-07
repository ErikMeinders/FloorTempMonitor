/*
***************************************************************************
**  Program  : servoStuff, part of FloorTempMonitor
**  Version  : v0.6.9
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

//#include <Wire.h>         // Include the I2C library (required)

#include "I2C_MuxLib.h"

#define CLOSE_SERVO         HIGH
#define OPEN_SERVO          LOW
#define I2C_MUX_ADDRESS     0x48    // the 7-bit address 
#define HEAT_RELAIS         14

I2CMUX    I2cExpander;   //Create instance of the I2CMUX object

static uint32_t scanTimer;          // 
//static uint8_t  globalClosedServosCount=0;

DECLARE_TIMER(servoAlignment,60);
DECLARE_TIMER(servoLoop,60);

void servosInit()
{
  for(uint8_t i=0 ; i < _MAX_SERVOS ; i++)
  {
    servoArray[i].servoState = SERVO_IS_OPEN;
    servoArray[i].closeCount = 0;
    servoArray[i].servoStateLock = 0;
    servoArray[i].closeReason = 0;
  }
  // checkDeltaTemps();
}

void servosLoop()
{
  if (DUE(servoLoop))
    checkDeltaTemps();
}

void demandForHeatCheck()
{
  /* as long as there is any OPEN (not opening!) servo ... */
  
  int openLoop=0;

  // SERVO[0] does not exist!!

  for(uint8_t i=1 ; i < _MAX_SERVOS ; i++)
    if(servoArray[i].servoState == SERVO_IS_OPEN)
      openLoop++;
  
  DebugTf("Open loop count now is %d\n", openLoop);

  I2cExpander.digitalWrite(HEAT_RELAIS, openLoop > 0); 
}

// servoOpen & servoClose with reason parameter do NOT actually open/close servo
// only used to register desired state

void servoClose(uint8_t s, uint8_t reason)
{
  servoArray[s].closeReason |= reason; 
}

static void _servoClose(uint8_t s)
{
  I2cExpander.digitalWrite(s, CLOSE_SERVO); 
  delay(100); 
  servoArray[s].servoState = SERVO_IS_CLOSING;
  servoArray[s].servoStateLock = SERVO_IS_CLOSING_LOCK;
  servoArray[s].closeCount++;

  DebugTf("[%2d] change to IS_CLOSING state\n", s);
  //globalClosedServosCount++;
  //demandForHeatCheck();

}

void servoOpen(uint8_t s, uint8_t reason)
{
  if(servoArray[s].closeReason & reason)
    servoArray[s].closeReason ^= reason;

  //if(servoArray[s].closeReason == 0 && servoArray[s].servoState == SERVO_IS_CLOSED)
  //  servoOpen(s); 
}

static void _servoOpen(uint8_t s)
{
  // check closeReason

  I2cExpander.digitalWrite(s, OPEN_SERVO); 
  delay(100); 
  servoArray[s].servoState = SERVO_IS_OPENING;
  servoArray[s].servoStateLock = SERVO_IS_OPENING_LOCK;
  DebugTf("[%2d] change to OPENING state\n", s);
  //globalClosedServosCount--;
  //demandForHeatCheck();
}

void servoAlign(uint8_t s)
{
  // compare servo state with administrated servo state
  
  bool actualOpen = (I2cExpander.digitalRead(s) ==  OPEN_SERVO);
  bool desiredOpen = false;

  if (servoArray[s].servoState == SERVO_IS_OPEN || servoArray[s].servoState == SERVO_IS_OPENING)
    desiredOpen=true;
  
  if (actualOpen != desiredOpen) {
    DebugTf("Misalignment in servo %d [actual=%c, desired=%c]\n", s, actualOpen ? 'T':'F', desiredOpen?'T':'F');
    I2cExpander.digitalWrite(s, desiredOpen ? OPEN_SERVO : CLOSE_SERVO); 
  }
}

void servosAlign()
{
  if (DUE(servoAlignment))
  {
    DebugTf("Aligning servos ...\n");
    
    for(uint8_t i=1 ; i < _MAX_SERVOS ; i++)
    {
      yield();
      servoAlign(i);
    }
  }
}
//=======================================================================
void checkDeltaTemps() {
  
  // servo is locked in new state while state_lock > 0
  // state_lock is reduced by 1 while > 0

  // update HOT WATER LED_OFF

  if (_SA[0].tempC > HEATER_ON_TEMP) {
    // heater is On
    digitalWrite(LED_WHITE, LED_ON);
  } else {
    digitalWrite(LED_WHITE, LED_OFF);
  }
  // Sensor 0 is output from heater (heater-out)
  // Sensor 1 is retour to heater
  // deltaTemp is the difference from heater-out and sensor

  // LOOP over all Sensors to find all servos 

  for (int8_t s=2; s < noSensors; s++) {
    
    // DebugTf("[%2d] servoNr of [%s] ==> [%d]\n", s, _SA[s].name, _SA[s].servoNr);

    if (_SA[s].servoNr < 0) {
      Debugln(" *SKIP*");
      continue;
    }
    
    int8_t servo=_SA[s].servoNr;

    if (servoArray[servo].servoStateLock != 0)
    {
      DebugTf("%d has %d ticks to go before state change allowed\n", servo, servoArray[servo].servoStateLock);
      servoArray[servo].servoStateLock--;
      continue;
    }

    // open or close based on return temp 

    if (_SA[0].tempC > HEATER_ON_TEMP && _SA[s].tempC > HOTTEST_RETURN)
    {
      servoClose(servo, WATER_HOT);    
    }
    if ( _SA[s].tempC <= HOTTEST_RETURN )
    {
      servoOpen(servo, WATER_HOT);
    }

    // by this time this servo is free to move...

    switch(servoArray[servo].servoState) {
      case SERVO_IS_OPEN:
        if (servoArray[servo].closeReason != 0)
          _servoClose(servo);
        break;
      case SERVO_IS_CLOSING:
        servoArray[servo].servoState = SERVO_IS_CLOSED;
        break;         
      case SERVO_IS_CLOSED: 
        if (servoArray[servo].closeReason == 0)
          _servoOpen(servo);
        break;
      case SERVO_IS_OPENING:
        servoArray[servo].servoState = SERVO_IS_OPEN;
        break;         
    } // switch
  } // for s ...
  demandForHeatCheck();
} // checkDeltaTemps()

//=======================================================================
void cycleAllNotUsedServos(int8_t &cycleNr) 
{
  DebugTf("lubricate starting at %d?\n", cycleNr);

  if( servoArray[cycleNr].servoState == SERVO_IS_OPENING || 
      servoArray[cycleNr].servoState == SERVO_IS_CLOSING ) {
      DebugTf("servo in motion\n");
      return;
  }

  for ( ; cycleNr <  _MAX_SERVOS  ; cycleNr++) {
    DebugTf("what about %d\n", cycleNr);

    if (servoArray[cycleNr].closeCount == 0) {
      _servoClose(cycleNr);
      DebugTf("--> yep, never closed\n");
      return;
    } 

    if (servoArray[cycleNr].closeCount == 1 && servoArray[cycleNr].servoState == SERVO_IS_CLOSED) {
      _servoOpen(cycleNr);
      DebugTf("--> yep, closed once & never opened again\n");
      return;
    }

  } 

  DebugTln("Done Cycling through all Servo's! => Reset closeCount'ers");
  for (int8_t s=0; s<noSensors; s++) {
      servoArray[_SA[s].servoNr].closeCount = 0;
  }
  cycleAllSensors = false; // stop for today
  
} // cycleAllNotUsedServos()

DECLARE_TIMERs(cycleCheck,60)
//=======================================================================
void handleCycleServos()
{
  if (!DUE(cycleCheck))
    return;
    
  if ( hour() >= _HOUR_CLOSE_ALL) {
    if (cycleAllSensors) {
      cycleAllNotUsedServos(cycleNr);
    }
  } else if ( !cycleAllSensors && hour() < _HOUR_CLOSE_ALL ) {
    cycleAllSensors = true;
    cycleNr = 1; 
  }
  
} // handleCycleServos()


//===========================================================================================
void checkI2C_Mux()
{
  static int16_t errorCountUp   = 0;
  static int16_t errorCountDown = 0;
  byte whoAmI /*, majorRelease, minorRelease */ ;

  if (errorCountDown > 0) {
    errorCountDown--;
    delay(20);
    errorCountUp = 0;
    return;
  }
  if (errorCountUp > 5) {
    errorCountDown = 5000;
  }
  
  //DebugTln("getWhoAmI() ..");
  if( I2cExpander.connectedToMux())
  {
    if ( (whoAmI = I2cExpander.getWhoAmI()) == I2C_MUX_ADDRESS)
    {
      if (I2cExpander.digitalRead(0) == CLOSE_SERVO)
        I2cExpander.digitalWrite(0, OPEN_SERVO);
      return;
    } else
      DebugTf("Connected to different I2cExpander %x",whoAmI );
  } else 
      DebugTf("Connection lost to I2cExpander");
  
  connectionMuxLostCount++;
  digitalWrite(LED_RED, LED_ON);

  if (setupI2C_Mux())
    errorCountUp = 0;
  else  
    errorCountUp++;
  
} // checkI2C_MUX();


//===========================================================================================
bool setupI2C_Mux()
{
  byte whoAmI;
  
  DebugT("Setup Wire ..");
//Wire.begin(_SDA, _SCL); // join i2c bus (address optional for master)
  Wire.begin();
  Wire.setClock(100000L); // <-- don't make this 400000. It won't work
  Debugln(".. done");
  
  if (I2cExpander.begin()) {
    DebugTln("Connected to the I2C multiplexer!");
  } else {
    DebugTln("Not Connected to the I2C multiplexer !ERROR!");
    delay(100);
    return false;
  }
  whoAmI       = I2cExpander.getWhoAmI();
  if (whoAmI != I2C_MUX_ADDRESS) {
    digitalWrite(LED_RED, LED_ON);
    return false;
  }
  digitalWrite(LED_RED, LED_OFF);

  DebugT("Close all servo's ..16 ");
  I2cExpander.digitalWrite(0, CLOSE_SERVO);
  delay(300);
  for (int s=15; s>0; s--) {
    I2cExpander.digitalWrite(s, CLOSE_SERVO);
    Debugf("%d ",s);
    delay(150);
  }
  Debugln();
  DebugT("Open all servo's ...");
  for (int s=15; s>0; s--) {
    I2cExpander.digitalWrite(s, OPEN_SERVO);
    Debugf("%d ",s);
    //if (s < noSensors) {                    // init servoState is moved 
    //  _SA[s].servoState = SERVO_IS_OPEN;    // to printSensorArray()
    //}                                       // ..
    delay(150);
  }
  delay(250);
  I2cExpander.digitalWrite(0, OPEN_SERVO);
  Debugln(16);

  //-- reset state of all servo's --------------------------
  for (int s=0; s<noSensors; s++) {
    if (_SA[s].servoNr > 0) {
      if (servoArray[_SA[s].servoNr].servoState == SERVO_IS_CLOSED) {
        I2cExpander.digitalWrite(_SA[s].servoNr, CLOSE_SERVO); 
        DebugTf("(re)Close servoNr[%d] for sensor[%s]\n", _SA[s].servoNr, _SA[s].name);
      } 
    }
  }
  
  return true;
  
} // setupI2C_Mux()


/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
***************************************************************************/
