/*
***************************************************************************
**  Program  : helperStuff, part of FloorTempMonitor
**  Version  : v0.6.9
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

// made changes to store raw data in dataStore

// apply calibration when storing in struct (used by API and controller)
// and when displaying

// function to print a device address
//===========================================================================================
void getSensorID(DeviceAddress deviceAddress, char* devID)
{
  sprintf(devID, "0x%02x%02x%02x%02x%02x%02x%02x%02x", deviceAddress[0], deviceAddress[1]
          , deviceAddress[2], deviceAddress[3]
          , deviceAddress[4], deviceAddress[5]
          , deviceAddress[6], deviceAddress[7]);
} // getSensorID()


//===========================================================================================
// getRawTemp: gets real, uncalibrated temp from sensor or random temp in TESTDATA scenario 
float getRawTemp(int8_t devNr)
{
  float tempR;
    
#ifdef TESTDATA       
  static float tempR0;
  if (devNr == 0 ) {
    tempR0 = (tempR0 + (random(273.6, 591.2) / 10.01)) /2.0;   // hjm: 25.0 - 29.0 ?  
    tempR = tempR0;          
  } else  
    tempR = (random(201.20, 308.8) / 10.02);               
#else
  tempR = sensors.getTempCByIndex(_SA[devNr].index);
#endif

  if (tempR < -2.0 || tempR > 102.0) {
    DebugTf("Sensor [%d][%s] invalid reading [%.3f*C]\n", devNr, _SA[devNr].name, tempR);
    return 99.9;                               
  }

  return tempR;  
}

//===========================================================================================
float _calibrated(float tempR, int8_t devNr)
{
  return (tempR + _SA[devNr].tempOffset) * _SA[devNr].tempFactor;
}

// updateSensorData updates _S as well as dataStore

//===========================================================================================
void updateSensorData(int8_t devNr)
{
  float tempR = getRawTemp(devNr);
  float tempC = _calibrated(tempR, devNr);
  
  // store calibrated temp in struct _S

  _SA[devNr].tempC = tempC;

  // overwrite _LAST_DATAPOINT data in dataStore

  dataStore[_LAST_DATAPOINT].timestamp = now();
  dataStore[_LAST_DATAPOINT].tempC[devNr] = tempR;

  if (_SA[devNr].servoNr > 0) {
    switch(_SA[devNr].servoState) {
      case SERVO_IS_OPEN:       dataStore[_LAST_DATAPOINT].servoStateV[devNr] =  30 +devNr;
                                break;
      case SERVO_IN_LOOP:       dataStore[_LAST_DATAPOINT].servoStateV[devNr] =   5 +devNr;
                                break;
      case SERVO_COUNT0_CLOSE:  dataStore[_LAST_DATAPOINT].servoStateV[devNr] = -20 +devNr;
                                break;
      case SERVO_IS_CLOSED:     dataStore[_LAST_DATAPOINT].servoStateV[devNr] = -25 +devNr;
                                break;
    } // switch() 

  } else {
    dataStore[_LAST_DATAPOINT].servoStateV[devNr] = 0;
  } // switch()
  
} // updateSensorData()


// function to print the temperature for a device

//===========================================================================================
void handleDatapoints()
{
  // time to refresh display of datapoints 
  
  if ( DUE(graphUpdate) ) {
        
    // after displaying, shift all points
    
    timeThis(shiftUpDatapoints());
  }

  // time for hourly writing datapoints
  
  if (hour() != lastSaveHour) {
      lastSaveHour = hour();
      writeDataPoints();
  }
}


//===========================================================================================
boolean _needToPoll()
{
  boolean toReturn = false;
   
  if ( DUE (sensorPoll) ) {
    toReturn = true;
 
    // let LED flash on during probing of sensors
    
    digitalWrite(LED_BUILTIN, LED_BUILTIN_ON);
    
    // call sensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
   
    DebugT("Requesting temperatures...\n");
    timeThis(sensors.requestTemperatures());
    yield();
    digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
  } 
    
  return toReturn;
}

//===========================================================================================
void handleSensors()
{
  if (_needToPoll())
  {
    for(int sensorNr = 0; sensorNr < noSensors; sensorNr++) 
    {
      timeThis(updateSensorData(sensorNr));
      yield();
    }
      
  }
} // handleSensors()


//=======================================================================
void printSensorArray()
{
  for(int8_t s=0; s<noSensors; s++) {
    Debugf("[%2d] => [%2d], [%02d], [%s], [%-20.20s], [%7.6f], [%7.6f]\n", s
           , _SA[s].index
           , _SA[s].position
           , _SA[s].sensorID
           , _SA[s].name
           , _SA[s].tempOffset
           , _SA[s].tempFactor);
     _SA[s].servoState = SERVO_IS_OPEN; // initial state
  }
} // printSensorArray()

//=======================================================================
void sortSensors()
{
  for (int8_t y = 0; y < noSensors; y++) {
    yield();
    for (int8_t x = y + 1; x < noSensors; x++)  {
      //DebugTf("y[%d], x[%d] => seq[x][%d] ", y, x, _SA[x].position);
      if (_SA[x].position < _SA[y].position)  {
        sensorStruct temp = _SA[y];
        _SA[y] = _SA[x];
        _SA[x] = temp;
      } /* end if */
      //Debugln();
    } /* end for */
  } /* end for */

} // sortSensors()

//=======================================================================
void shiftUpDatapoints()
{
  for (int p = 0; p < _LAST_DATAPOINT; p++) {
    dataStore[p] = dataStore[(p+1)];
  }
  
} // shiftUpDatapoints()


//=======================================================================
int8_t editSensor(int8_t recNr)
{
  sensorStruct tmpRec = readIniFileByRecNr(recNr);
  if (tmpRec.sensorID[0] == '-') {
    DebugTf("Error: something wrong reading record [%d] from [/sensor.ini]\n", recNr);
    return -1;
  }
  sprintf(cMsg, "msgType=editSensor,sensorNr=%d,sID=%s,sName=%s,sPosition=%d,sTempOffset=%.6f,sTempFactor=%.6f,sServo=%d,sDeltaTemp=%.1f"
                              , recNr
                              , tmpRec.sensorID
                              , tmpRec.name
                              , tmpRec.position
                              , tmpRec.tempOffset
                              , tmpRec.tempFactor
                              , tmpRec.servoNr
                              , tmpRec.deltaTemp);
   DebugTln(cMsg);                           
   return recNr;
  
} // editSensor()

//=======================================================================
void updSensor(char *payload)
{
  char*   pch;
  int8_t  recNr;
  sensorStruct tmpRec;
  
  DebugTf("payload[%s] \n", payload);

  pch = strtok (payload, ",");
  while (pch != NULL)  {
    DebugTf ("%s ==> \n",pch);
    if (strncmp(pch, "updSensorNr", 11) == 0) { 
        sprintf(cMsg, "%s", splitFldVal(pch, '='));
        recNr = String(cMsg).toInt();
        tmpRec = readIniFileByRecNr(recNr);
    } else if (strncmp(pch, "sensorID", 8) == 0) { 
        sprintf(cMsg, "%s", splitFldVal(pch, '='));
        memcpy(tmpRec.sensorID, cMsg, sizeof(cMsg));
    } else if (strncmp(pch, "name", 4) == 0) { 
        sprintf(cMsg, "%s", splitFldVal(pch, '='));
        memcpy(tmpRec.name, cMsg, sizeof(cMsg));
        String(tmpRec.name).trim();
        if (String(tmpRec.name).length() < 2) {
          sprintf(tmpRec.name, "%s", "unKnown");
        }
    } else if (strncmp(pch, "position", 8) == 0) { 
        sprintf(cMsg, "%s", splitFldVal(pch, '='));
        tmpRec.position = String(cMsg).toInt();
      //if (tmpRec.position <  0) tmpRec.position = 0;
        if (tmpRec.position > 99) tmpRec.position = 99;
    } else if (strncmp(pch, "tempOffset", 10) == 0) { 
        sprintf(cMsg, "%s", splitFldVal(pch, '='));
        tmpRec.tempOffset = String(cMsg).toFloat();
    } else if (strncmp(pch, "tempFactor", 10) == 0) { 
        sprintf(cMsg, "%s", splitFldVal(pch, '='));
        tmpRec.tempFactor = String(cMsg).toFloat();
    } else if (strncmp(pch, "servoNr", 7) == 0) { 
        sprintf(cMsg, "%s", splitFldVal(pch, '='));
        tmpRec.servoNr = String(cMsg).toInt();
        if (tmpRec.servoNr < -1)  tmpRec.servoNr = -1;
        if (tmpRec.servoNr > 15)  tmpRec.servoNr = 15;
    } else if (strncmp(pch, "deltaTemp", 9) == 0) { 
        sprintf(cMsg, "%s", splitFldVal(pch, '='));
        tmpRec.deltaTemp = String(cMsg).toFloat();
        if (tmpRec.deltaTemp < 0.0) tmpRec.deltaTemp = 20.0;
    } else {
        Debugf("Don't know what to do with[%s]\n", pch);
    }
    pch = strtok (NULL, ",");
  }

  Debugf("tmpRec ID[%s], name[%s], position[%d]\n   tempOffset[%.6f], tempFactor[%.6f]\n   Servo[%d], deltaTemp[%.1f]\n"
                                                , tmpRec.sensorID
                                                , tmpRec.name
                                                , tmpRec.position
                                                , tmpRec.tempOffset
                                                , tmpRec.tempFactor
                                                , tmpRec.servoNr
                                                , tmpRec.deltaTemp);

  recNr = updateIniRec(tmpRec);
  if (recNr >= 0) {
    editSensor(recNr);
  }
  
} // updSensor()

//===========================================================================================
void setupDallasSensors()
{
  byte i;
  byte present =  0;
  byte type_s  = -1;
  byte data[12];
  float celsius, fahrenheit;

  noSensors = sensors.getDeviceCount();
  DebugTf("Locating devices...found %d devices\n", noSensors);

  oneWire.reset_search();
  for(int sensorNr = 0; sensorNr <= noSensors; sensorNr++) {
    Debugln();
    DebugTf("Checking device [%2d]\n", sensorNr);
    
    if ( !oneWire.search(DS18B20)) {
      DebugTln("No more addresses.");
      Debugln();
      oneWire.reset_search();
      delay(250);
      break;
    }

    DebugT("ROM =");
    for( i = 0; i < 8; i++) {
      Debug(' ');
      Debug(DS18B20[i], HEX);
    }

    if (OneWire::crc8(DS18B20, 7) != DS18B20[7]) {
      Debugln(" => CRC is not valid!");
      sensorNr--;
    }
    else {
      Debugln();
 
      // the first ROM byte indicates which chip
      switch (DS18B20[0]) {
        case 0x10:
          DebugTln("  Chip = DS18S20");  // or old DS1820
          type_s = 1;
          break;
        case 0x28:
          DebugTln("  Chip = DS18B20");
          type_s = 0;
          break;
        case 0x22:
          DebugTln("  Chip = DS1822");
          type_s = 0;
          break;
        default:
          DebugTln("Device is not a DS18x20 family device.");
          type_s = -1;
          break;
      } // switch(addr[0]) 

      oneWire.reset();
      oneWire.select(DS18B20);
      //oneWire.write(0x44, 1);        // start conversion, with parasite power on at the end
  
      delay(1000);     // maybe 750ms is enough, maybe not
      // we might do a oneWire.depower() here, but the reset will take care of it.
  
      present = oneWire.reset();
      oneWire.select(DS18B20);    
      oneWire.write(0xBE);         // Read Scratchpad

      DebugT("  Data = ");
      Debug(present, HEX);
      Debug(" ");
      for ( i = 0; i < 9; i++) {           // we need 9 bytes
        data[i] = oneWire.read();
        Debug(data[i], HEX);
        Debug(" ");
      }
      Debug(" CRC=");
      Debug(OneWire::crc8(data, 8), HEX);
      Debugln();

      // Convert the data to actual temperature
      // because the result is a 16 bit signed integer, it should
      // be stored to an "int16_t" type, which is always 16 bits
      // even when compiled on a 32 bit processor.
      int16_t raw = (data[1] << 8) | data[0];
      if (type_s) {
        raw = raw << 3; // 9 bit resolution default
        if (data[7] == 0x10) {
          // "count remain" gives full 12 bit resolution
          raw = (raw & 0xFFF0) + 12 - data[6];
        }
      } else {
        byte cfg = (data[4] & 0x60);
        // at lower res, the low bits are undefined, so let's zero them
        if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
        else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
        else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
        //// default is 12 bit resolution, 750 ms conversion time
      }
      celsius = (float)raw / 16.0;
      fahrenheit = celsius * 1.8 + 32.0;
      DebugT("  Temperature = ");
      Debug(celsius);
      Debug(" Celsius, ");
      Debug(fahrenheit);
      Debugln(" Fahrenheit");
      
      getSensorID(DS18B20, cMsg);
      DebugTf("Device [%2d] sensorID: [%s] ..\n", sensorNr, cMsg);
      if (!readIniFile(sensorNr, cMsg)) {
        appendIniFile(sensorNr, cMsg);
      }
    } // CRC is OK
  
  } // for noSensors ...
  
} // setupDallasSensors()

//=======================================================================
static char splitValue[50];

char* splitFldVal(char *pair, char del)
{
  bool    isVal = false;
  int8_t  inxVal = 0;

  DebugTf("pair[%s] (length[%d]) del[%c] \n", pair, strlen(pair), del);

  for(int i=0; i<(int)strlen(pair); i++) {
    if (isVal) {
      splitValue[inxVal++] = pair[i];
      splitValue[inxVal]   = '\0';
    } else if (pair[i] == del) {
      isVal   = true;
      inxVal  = 0;
    }
  }
  DebugTf("Value [%s]\n", splitValue);
  return splitValue;
  
} // splitFldVal()

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
