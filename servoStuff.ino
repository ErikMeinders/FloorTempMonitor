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

I2CMUX    I2cExpander;   //Create instance of the I2CMUX object

static uint32_t scanTimer;          // 

DECLARE_TIMER(servoAlignment,60);

void servoInit()
{
  for(uint8_t i=0 ; i < _MAX_SERVOS ; i++)
  {
    servoArray[i].servoState = SERVO_IS_OPEN;
    servoArray[i].closeCount = 0;
    servoArray[i].servoTimer = 0;
    servoArray[i].closeReason = 0;
  }
}
void servoClose(uint8_t s, uint8_t reason)
{
  servoArray[s].closeReason |= reason; 
  if(servoArray[s].servoState == SERVO_IS_OPEN)
   servoClose(s);
}

void servoClose(uint8_t s)
{

  I2cExpander.digitalWrite(s, CLOSE_SERVO); 
  delay(100); 
  servoArray[s].servoState = SERVO_IS_CLOSED;
  servoArray[s].servoTimer = millis();
  DebugTf("[%2d] change to CLOSED state for [%d] minutes\n", s, _PULSE_TIME);
}

void servoOpen(uint8_t s, uint8_t reason)
{
  if(servoArray[s].closeReason & reason)
    servoArray[s].closeReason ^= reason;

  if(servoArray[s].closeReason == 0 && servoArray[s].servoState == SERVO_IS_CLOSED)
    servoOpen(s); 

}

void servoOpen(uint8_t s)
{
  // check closeReason

  I2cExpander.digitalWrite(s, OPEN_SERVO); 
  delay(100); 
  servoArray[s].servoState = SERVO_IN_LOOP;
  servoArray[s].closeCount++;
  servoArray[s].servoTimer = millis();
  DebugTf("[%2d] change to LOOP state for [%d] minutes\n", s, (_REFLOW_TIME / _MIN));
}

void servoAlign(uint8_t s)
{
  // compare servo state with administrated servo state
  
  bool actualOpen = (I2cExpander.digitalRead(s) ==  OPEN_SERVO);
  bool desiredOpen = false;

  // dirty trick for servo 15

  if (s > _MAX_SERVOS)
    desiredOpen=true;
  else if (servoArray[s].servoState == SERVO_IS_OPEN || servoArray[s].servoState == SERVO_IN_LOOP)
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
  
    for(uint8_t i=0 ; i < _MAX_SERVOS ; i++)
    {
      yield();
      servoAlign(i);
    }
    //servoAlign(15);
  }
}
//=======================================================================
void checkDeltaTemps() {
  if ((millis() - scanTimer) > (_DELTATEMP_CHECK * _MIN)) {
    scanTimer = millis();
  } else return;

  //--- pulseTime is the time a Servo/Valve ones closed stayes closed ----------
  //--- this time is set @ the deltaTemp of the heater-out (position 0 sensor) --
  uint32_t pulseTime = _PULSE_TIME * _MIN;  // deltaTemp van Sensor 'S0' is pulseTime

  DebugTf("Check for delta Temps!! pulseTime[%d]min. from [%s]\n", 
    (int)_PULSE_TIME,
    _SA[0].name);

  if (_SA[0].tempC > HEATER_ON_TEMP) {
    // heater is On
    digitalWrite(LED_WHITE, LED_ON);
  } else {
    digitalWrite(LED_WHITE, LED_OFF);
  }
  // Sensor 0 is output from heater (heater-out)
  // Sensor 1 is retour to heater
  // deltaTemp is the difference from heater-out and sensor
  for (int8_t s=2; s < noSensors; s++) {
    DebugTf("[%2d] servoNr of [%s] ==> [%d]", s, _SA[s].name
                                               , _SA[s].servoNr);
    if (_SA[s].servoNr < 0) {
      Debugln(" *SKIP*");
      continue;
    }
    int8_t servo=_SA[s].servoNr;

    if (_SA[0].tempC > HEATER_ON_TEMP) 
    {      
      
      if( _SA[s].tempC > HOTTEST_RETURN)
      {
        servoClose(servo, WATER_HOT);
      }
    } 

    Debugln();
    //DebugTf("reflowTime[%d]\n", (_REFLOW_TIME / _MIN));
    switch(servoArray[servo].servoState) {
      
      case SERVO_IS_OPEN:  
        
        break;
                  
      case SERVO_IS_CLOSED: 
        // if closed for more than pulseTime, open (via reflow)
        if ((millis() - servoArray[servo].servoTimer) > pulseTime) {
          servoOpen(servo, WATER_HOT);
        }
        break;
        
      case SERVO_IN_LOOP:
        if ((millis() - _REFLOW_TIME) > servoArray[servo].servoTimer) {
          servoArray[servo].servoState = SERVO_IS_OPEN;
          servoArray[servo].servoTimer = 0;
          DebugTf("[%2d] change to normal operation (OPEN state)\n", s);
        }
        break;

    } // switch
  } // for s ...

} // checkDeltaTemps()

//=======================================================================
void cycleAllNotUsedServos(int8_t &cycleNr) 
{
  int8_t  s;

  if (cycleNr == 0) {
    cycleNr = 1;  // skip Flux In(0)
  } else {
    if ((millis() - servoArray[_SA[cycleNr].servoNr].servoTimer) < _REFLOW_TIME) {
      return;
    }
  }
  
  for (s=cycleNr; s < noSensors; s++) {
    DebugTf("[%2d] servoNr of [%s] ==> servoNr[%d]", s, _SA[s].name
                                                      , _SA[s].servoNr);
    if (_SA[s].servoNr < 0) {
      Debugln(" *SKIP*");
      continue;
    }
    Debugln();
    int8_t servo=_SA[s].servoNr;

    if (servoArray[servo].closeCount == 0) {  // never closed last 24 hours ..
      switch(servoArray[servo].servoState) {  
        case SERVO_IS_CLOSED:
        case SERVO_IN_LOOP:
          DebugTf("[%2d] CYCLE [%s] SKIP!\n", s, _SA[s].name);
          servoArray[servo].closeCount++;
          s++;  // skip this one, is already closed or in loop
          break;
                
        case SERVO_IS_OPEN:
          I2cExpander.digitalWrite(_SA[s].servoNr, CLOSE_SERVO);  
          servoArray[servo].servoState = SERVO_COUNT0_CLOSE;
          servoArray[servo].servoTimer = millis();
          DebugTf("[%2d] CYCLE [%s] to CLOSED state for [%lu] seconds\n", s
                                                        , _SA[s].name
                                                        , (_REFLOW_TIME - (millis() - servoArray[_SA[s].servoNr].servoTimer)) / 1000);
          break;
                
        case SERVO_COUNT0_CLOSE:
          if ((millis() - servoArray[_SA[s].servoNr].servoTimer) > _REFLOW_TIME) {
            I2cExpander.digitalWrite(_SA[s].servoNr, OPEN_SERVO);  
            servoArray[servo].servoState = SERVO_IS_OPEN;
            servoArray[servo].closeCount++;
            DebugTf("[%2d] CYCLE [%s] to OPEN state\n", s, _SA[s].name);
          }
          s++;
          break;
          
        default:  // it has some other state...
          DebugTf("[%2d] CYCLE [%s] has an unknown state\n", s, _SA[s].name);
          // check again after _REFLOW_TIME
          servoArray[servo].servoTimer = millis(); 
                 
      } // switch ..
      cycleNr = s;
      return;  // BREAK!! we don't want all valves closed at the same time
      
    }  // closeCount == 0 ..
    
  } // for s ...

  if (s >= noSensors) {
    DebugTln("Done Cycling through all Servo's! => Reset closeCount'ers");
    for (s=0; s<noSensors; s++) {
      servoArray[_SA[s].servoNr].closeCount = 0;
    }
    cycleAllSensors = false;
  }

} // cycleAllNotUsedServos()


//=======================================================================
void handleCycleServos()
{
  if ( hour() >= _HOUR_CLOSE_ALL) {
    if (cycleAllSensors) {
      cycleAllNotUsedServos(cycleNr);
    }
  } else if ( !cycleAllSensors && hour() < _HOUR_CLOSE_ALL ) {
    cycleAllSensors = true;
    cycleNr = 0; 
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
