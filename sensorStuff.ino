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

void sensorsInit()
{

  sensorsRead();
  setupDallasSensors();
  sensorsWrite();
  handleSensors();
}

void sensorsLoop()
{
  if (_needToPoll())
    handleSensors();
}
// function to print a device address
//===========================================================================================
char * sensorIDFormat(DeviceAddress deviceAddress)
{
  static char devID[24];

  sprintf(devID, "0x%02x%02x%02x%02x%02x%02x%02x%02x", deviceAddress[0], deviceAddress[1]
          , deviceAddress[2], deviceAddress[3]
          , deviceAddress[4], deviceAddress[5]
          , deviceAddress[6], deviceAddress[7]);
  return devID;
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
  tempR = sensors.getTempCByIndex(_SA[devNr].sensorIndex);
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

// function to print the temperature for a device

//===========================================================================================

void updateSensorData(int8_t devNr)
{
  float tempR = getRawTemp(devNr);
  float tempC = _calibrated(tempR, devNr);
  
  // store calibrated temp in struct _S

  _SA[devNr].tempC = tempC;
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
  
    cacheEmpty = true; // force cache refresh upon next use

    for(int sensorNr = 0; sensorNr < noSensors; sensorNr++) 
    {
      yield();
      timeThis(updateSensorData(sensorNr));
    }
      
  
} // handleSensors()


//=======================================================================
void printSensorArray()
{
  for(int8_t s=0; s<noSensors; s++) {
    Debugf("[%2d] => [%2d], [%s], [%-20.20s], [%7.6f], [%7.6f]\n", s
           , _SA[s].sensorIndex
           , _SA[s].sensorID
           , _SA[s].name
           , _SA[s].tempOffset
           , _SA[s].tempFactor);
     
  }
} // printSensorArray()

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

  if(noSensors != noSensorRecs)
  {
    DebugTf("Change in sensors detected: got %d records for %d sensors\n", noSensorRecs, noSensors);
  }
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

    DebugTf("ROM = %s", sensorIDFormat(DS18B20));

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
      
      DebugTf("Device [%2d] sensorID: [%s] ..\n", sensorNr, sensorIDFormat(DS18B20));
      
      sensorMatchOrAdd(sensorIDFormat(DS18B20),sensorNr); // add sensor to _SA if not read from .ini file & update .index to sensorNr

    } // CRC is OK
  
  } // for noSensors ...
  
} // setupDallasSensors()


//===========================================================================================
bool sensorMatchOrAdd(char* devID, int sensorNr)
{ 
  // see if sensor detected on bus can be matched with existing record in .ini (_SA)

  for (int8_t i=0 ; i < noSensorRecs ; i++)
  {
    if(!strcmp(_SA[i].sensorID, devID))
    {
      DebugTf("Match found for %s in %s\n", devID, _SA[i].name);
      _SA[i].sensorIndex = sensorNr;
      return true;
    }
  }

  // the sensor found on the bus is new!

  yield();

  _SA[noSensorRecs].sensorIndex = sensorNr;
  
  sprintf(_SA[noSensorRecs].sensorID, "%s", devID);
  sprintf(_SA[noSensorRecs].name, "new Sensor");
  _SA[noSensorRecs].tempOffset = 0.00000;
  _SA[noSensorRecs].tempFactor = 1.00000;
  _SA[noSensorRecs].servoNr    = -1; // not attached to a servo
  _SA[noSensorRecs].deltaTemp  = 20.0;

  noSensorRecs++;

  return false;

} 

void sensorsWrite()
{
    File file = SPIFFS.open("/sensors.ini", "w");
    char buffer[80];

    for (int8_t i=0 ; i < noSensorRecs ; i++)
    {    
      sprintf(buffer,"%s;%s;%f;%f;%d;%f\n",
        _SA[i].sensorID, 
        _SA[i].name,
        _SA[i].tempOffset, 
        _SA[i].tempFactor,
        _SA[i].servoNr,
        _SA[i].deltaTemp);
    
      yield();
      file.write(buffer, strlen(buffer));
        
    }
    file.close(); //

}

//===========================================================================================
void sensorsRead()
{
  File  file;
  char  buffer[80];

  digitalWrite(LED_BUILTIN, LED_BUILTIN_ON);

  DebugTf("readIniFile(/sensors.ini) \r\n");

  if (!SPIFFSmounted) {
    DebugTln("No SPIFFS filesystem..\r");
    return;
  }
  noSensorRecs=0;

  file = SPIFFS.open("/sensors.ini", "r");

  while (file.available() ) {
    int servoNr;
    int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
    buffer[l] = 0;
    
    if(l < 2)
      break;
      
    yield();

    sscanf(buffer,"%[^;];%[^;];%f;%f;%d;%f\n",
       _SA[noSensorRecs].sensorID, 
       _SA[noSensorRecs].name,
      &_SA[noSensorRecs].tempOffset, 
      &_SA[noSensorRecs].tempFactor,
      &servoNr,
      &_SA[noSensorRecs].deltaTemp
    );

    _SA[noSensorRecs].servoNr     = (int8_t) servoNr;
    _SA[noSensorRecs].sensorIndex = noSensorRecs;     // will be overridden by sensorMatchOrAdd !!
    _SA[noSensorRecs].tempC       = -99;

    noSensorRecs++;
  }

  file.close();
  digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);

}

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
