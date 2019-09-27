/*
***************************************************************************
**  Program  : servoStuff, part of FloorTempMonitor
**  Version  : v0.6.2
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/
/*
SparkFun SX1509 I/O Expander Example: digital out (digitalWrite)
Jim Lindblom @ SparkFun Electronics
Original Creation Date: September 21, 2015
https://github.com/sparkfun/SparkFun_SX1509_Arduino_Library

This simple example demonstrates the SX1509's digital output 
functionality. Attach an LED to SX1509 IO 15, or just look at
it with a multimeter. We're gonna blink it!

Hardware Hookup:
  SX1509 Breakout ------ ESP8266 -------- 
        GND -------------- GND
        3V3 -------------- 3.3V
        SDA -------------- SDA (A4)
        SCL -------------- SCL (A5)

Development environment specifics:
  IDE: Arduino 1.6.5
  Hardware Platform: Arduino Uno
  SX1509 Breakout Version: v2.0

This code is beerware; if you see me (or any other SparkFun 
employee) at the local, and you've found our code helpful, 
please buy us a round!
*/

#include <Wire.h> // Include the I2C library (required)
#include <SparkFunSX1509.h> // Include SX1509 library

#define CLOSE_SERVO   HIGH
#define OPEN_SERVO    LOW

// SX1509 I2C address (set by ADDR1 and ADDR0 (00 by default):
const byte SX1509_ADDRESS = 0x3E;   // SX1509 I2C address
SX1509     ioExpander;              // Create an SX1509 object to be used throughout

static uint32_t scanTimer;          // 

//=======================================================================
void checkDeltaTemps() {
  if ((millis() - scanTimer) > (_DELTATEMP_CHECK * _MIN)) {
    scanTimer = millis();
  } else return;

  //--- pulseTime is the time a Servo/Valve ones closed stayes closed ----------
  //--- this time is set @ the loopTime of the heater-out (position 0 sensor) --
  uint32_t pulseTime = _PULSE_TIME * _MIN;  // loopTime van Sensor 'S0' is pulseTime

  DebugTf("Check for delta Temps!! pulseTime[%d]min. from [%s]\n"
                                                            , _PULSE_TIME
                                                            , _S[0].name);
  // Sensor 0 is output from heater (heater-out)
  // Sensor 1 is retour to heater
  // deltaTemp is the difference from heater-out and sensor
  for (int s=1; s < noSensors; s++) {
    DebugTf("[%2d] servoNr of [%s] ==> [%d]", s, _S[s].name
                                               , _S[s].servoNr);
    if (_S[s].servoNr < 0) {
      Debugln(" *SKIP*");
      continue;
    }
    Debugln();
    DebugTf("[%2d] checking on [%s], loopTime[%d]\n", s, _S[s].name
                                                       , _S[s].loopTime);
    switch(_S[s].servoState) {
      case SERVO_IS_OPEN:  
                  DebugTf("[%2d] tempC-flux[%.1f] -/- tempC[%.1f] = [%.1f] < deltaTemp[%.1f]?\n"
                                                      , s
                                                      , _S[0].tempC
                                                      , _S[s].tempC
                                                      , (_S[0].tempC - _S[s].tempC)
                                                      , _S[s].deltaTemp);
                  if (_S[0].tempC > 40.0) {
                    //--- only if heater is heating -----
                    DebugTf("[%2d] heater is On! deltaTemp[%.1f] > [%.1f]?\n"
                                                      , s
                                                      , _S[s].deltaTemp
                                                      , (_S[0].tempC - _S[s].tempC) );
                    if ( _S[s].deltaTemp > (_S[0].tempC - _S[s].tempC)) {
                      ioExpander.digitalWrite(_S[s].servoNr, CLOSE_SERVO);  
                      _S[s].servoState = SERVO_IS_CLOSED;
                      _S[s].servoTimer = millis() / _MIN;
                      DebugTf("[%2d] change to CLOSED state for [%d] minutes\n", s, (pulseTime / _MIN));
                    }
                    //--- heater is not heating ... -----
                  } else {  //--- open Servo/Valve ------
                    DebugTln("Flux temp < 40*C");
                    ioExpander.digitalWrite(_S[s].servoNr, OPEN_SERVO);  
                    _S[s].servoState = SERVO_IS_OPEN;
                    _S[s].servoTimer = 0;
                  }
                  break;
                  
      case SERVO_IS_CLOSED: 
                  if ((millis() - (_S[s].servoTimer * _MIN)) > pulseTime) {
                    ioExpander.digitalWrite(_S[s].servoNr, OPEN_SERVO);  
                    _S[s].servoState = SERVO_IN_LOOP;
                    _S[s].servoTimer = millis() / _MIN;
                    DebugTf("[%2d] change to LOOP state for [%d] minutes\n", s, _S[s].loopTime);
                  }
                  break;
                  
      case SERVO_IN_LOOP:
                  if ((millis() - (_S[s].loopTime * _MIN)) > (_S[s].servoTimer * _MIN)) {
                    _S[s].servoState = SERVO_IS_OPEN;
                    _S[s].servoTimer = 0;
                    DebugTf("[%2d] change to normal operation (OPEN state)\n", s);
                  }
                  break;
    } // switch
  } // for s ...

} // checkDeltaTemps()

//=======================================================================
void setupSX1509() 
{
  // Call io.begin(<address>) to initialize the SX1509. If it
  // successfully communicates, it'll return 1.
  if (!ioExpander.begin(SX1509_ADDRESS))
  {
    connectToSX1509 = false;
    //while (1) ; // If we fail to communicate, loop forever.
  } else {
    connectToSX1509 = true;
  }
  
  // Call io.pinMode(<pin>, <mode>) to set all SX1509 pin's 
  // as output:
  ioExpander.pinMode( 0, OUTPUT);
  ioExpander.pinMode( 1, OUTPUT);
  ioExpander.pinMode( 2, OUTPUT);
  ioExpander.pinMode( 3, OUTPUT);
  ioExpander.pinMode( 4, OUTPUT);
  ioExpander.pinMode( 5, OUTPUT);
  ioExpander.pinMode( 6, OUTPUT);
  ioExpander.pinMode( 7, OUTPUT);
  ioExpander.pinMode( 8, OUTPUT);
  ioExpander.pinMode( 9, OUTPUT);
  ioExpander.pinMode(10, OUTPUT);
  ioExpander.pinMode(11, OUTPUT);
  ioExpander.pinMode(12, OUTPUT);
  ioExpander.pinMode(13, OUTPUT);
  ioExpander.pinMode(14, OUTPUT);
  ioExpander.pinMode(15, OUTPUT);

} // setupSX1509()



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
