/*
***************************************************************************  
**  Program  : spiffsStuff, part of FloorTempMonitor
**  Version  : v1.0.3
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

static      FSInfo SPIFFSinfo;

//===========================================================================================
int32_t freeSpace() {
//===========================================================================================
  int32_t space;
  
  SPIFFS.info(SPIFFSinfo);

  space = (int32_t)(SPIFFSinfo.totalBytes - SPIFFSinfo.usedBytes);

  return space;
  
} // freeSpace()

//===========================================================================================
void listSPIFFS() {
//===========================================================================================
  Dir dir = SPIFFS.openDir("/");

  DebugTln("\r\n");
  while (dir.next()) {
    File f = dir.openFile("r");
    Debugf("%-25s %6d \r\n", dir.fileName().c_str(), f.size());
    yield();
  }

  SPIFFS.info(SPIFFSinfo);

  Debugln("\r");
  if (freeSpace() < (10 * SPIFFSinfo.blockSize))
        Debugf("Available SPIFFS space [%6d]kB (LOW ON SPACE!!!)\r\n", (freeSpace() / 1024));
  else  Debugf("Available SPIFFS space [%6d]kB\r\n", (freeSpace() / 1024));
  Debugf("           SPIFFS Size [%6d]kB\r\n", (SPIFFSinfo.totalBytes / 1024));
  Debugf("     SPIFFS block Size [%6d]bytes\r\n", SPIFFSinfo.blockSize);
  Debugf("      SPIFFS page Size [%6d]bytes\r\n", SPIFFSinfo.pageSize);
  Debugf(" SPIFFS max.Open Files [%6d]\r\n\r\n", SPIFFSinfo.maxOpenFiles);


} // listSPIFFS()


//===========================================================================================
bool appendIniFile(int8_t index, char* devAddr) {
//===========================================================================================

  DebugTf("appendIniFile(/sensors.ini) .. [%d] - Address[%s] ", index, devAddr);

  if (!SPIFFSmounted) {
    Debugln("No SPIFFS filesystem..ABORT!!!\r");
    return false;
  }
  
  // --- check if the file exists and can be opened ---
  File dataFile  = SPIFFS.open("/sensors.ini", "a");    // open for append writing
  if (!dataFile) {
    Debugln("Some error opening [sensors.ini] .. bailing out!");
    return false;
  } // if (!dataFile)
  
  yield();
  sensorArray[index].index = index;
  sprintf(sensorArray[index].address, "%s", devAddr);
  sensorArray[index].position = 90;
  sprintf(sensorArray[index].name, "unKnown sensor");
  sensorArray[index].tempOffset = 0.00000;
  sensorArray[index].tempFactor = 1.00000;
 
  yield();
  dataFile.print(sensorArray[index].address);       dataFile.print("; ");
  dataFile.print(sensorArray[index].position);      dataFile.print("; ");
  dataFile.print(sensorArray[index].name);          dataFile.print("; ");
  dataFile.print(sensorArray[index].tempOffset, 6); dataFile.print("; ");
  dataFile.print(sensorArray[index].tempFactor, 6); dataFile.print("; ");
  dataFile.println();

  dataFile.close();  

  Debugln(" .. Done\r");

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  return true;
  
} // appendIniFile()


//===========================================================================================
bool readIniFile(int8_t index, char *devAddr) {
//===========================================================================================
  File    dataFile;
  String  tmpS;
  
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  DebugTf("readIniFile(/sensors.ini) .. Address[%s] \r\n", devAddr);

  if (!SPIFFSmounted) {
    DebugTln("No SPIFFS filesystem..\r");
    return false;
  }

  dataFile = SPIFFS.open("/sensors.ini", "r");
  while (dataFile.available() > 0) {
    tmpS     = dataFile.readStringUntil(';');
    tmpS.trim();
    if (String(devAddr) == String(tmpS)) {
      sensorArray[index].index = index;
      sprintf(sensorArray[index].address, "%s", tmpS.c_str());
      sensorArray[index].position    = dataFile.readStringUntil(';').toInt();
      tmpS     = dataFile.readStringUntil(';');
      tmpS.trim();
      sprintf(sensorArray[index].name, "%s", tmpS.c_str());
      sensorArray[index].tempOffset  = dataFile.readStringUntil(';').toFloat();
      sensorArray[index].tempFactor  = dataFile.readStringUntil(';').toFloat();
      String n = dataFile.readStringUntil('\n');
      dataFile.close();  
      return true;
    }
    String n = dataFile.readStringUntil('\n');  // goto next record
  }
  
  dataFile.close();  

  DebugTln(" .. Done not found!\r");
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  return false;

} // readIniFile()

//=======================================================================
bool lookupSensor(int8_t index, char* devAddr)
{
  int8_t s;
  //DebugTf("Testing for [%s]\n", devAddr);
  for(s=0; s<noSensors; s++) {
    //DebugTf("[%d] => Testing devAddr [%s] == [%s]", s, devAddr,  sensorArray[s].address);
    if( String(sensorArray[s].address) == String(devAddr) ) {
      Debugf("-> found [%s]\n", sensorArray[s].address);
      sensorArray[s].index = index;
      return true;
    }
    //Debugln();
    if (sensorArray[s].address[0] == '\0') {
      //Debugln(" --> slot is empty!");
      //DebugTln("--> new sensor!"); 
      sensorArray[s].index = index;
      sprintf(sensorArray[s].address, "%s", devAddr);
      sensorArray[s].position   = (s + 1);
      sprintf(sensorArray[s].name, "unKnown");
      sensorArray[s].tempOffset = 0.000000;
      sensorArray[s].tempFactor = 1.000000;
      return false;
    }
  }

  return false;
  
} // lookupSensor()

//=======================================================================
void printSensorArray()
{
  for(int8_t s=0; s<noSensors; s++) {
    Debugf("[%2d] => [%2d], [%02d], [%s], [%-20.20s], [%7.6f], [%7.6f]\n", s
                      , sensorArray[s].index
                      , sensorArray[s].position
                      , sensorArray[s].address
                      , sensorArray[s].name
                      , sensorArray[s].tempOffset
                      , sensorArray[s].tempFactor);
  }
} // printSensorArray()

//=======================================================================
void sortSensors() 
{
  int x, y;
  
    for (int8_t y = 0; y < noSensors; y++) {
        yield();
        for (int8_t x = y + 1; x < noSensors; x++)  {
            //DebugTf("y[%d], x[%d] => seq[x][%d] ", y, x, sensorArray[x].position);
            if (sensorArray[x].position < sensorArray[y].position)  {
                sensorStruct temp = sensorArray[y];
                sensorArray[y] = sensorArray[x];
                sensorArray[x] = temp;
            } /* end if */
            //Debugln();
        } /* end for */
    } /* end for */
 
} // sortSensors()



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
