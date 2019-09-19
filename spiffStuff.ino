/*
***************************************************************************
**  Program  : spiffsStuff, part of FloorTempMonitor
**  Version  : v0.5.0
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

static      FSInfo SPIFFSinfo;

//===========================================================================================
int32_t freeSpace()
{
  int32_t space;

  SPIFFS.info(SPIFFSinfo);

  space = (int32_t)(SPIFFSinfo.totalBytes - SPIFFSinfo.usedBytes);

  return space;

} // freeSpace()

//===========================================================================================
void listSPIFFS()
{
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
bool appendIniFile(int8_t index, char* devAddr)
{
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
  sprintf(sensorArray[index].sensorID, "%s", devAddr);
  sensorArray[index].position = 90;
  sprintf(sensorArray[index].name, "unKnown sensor");
  sensorArray[index].tempOffset = 0.00000;
  sensorArray[index].tempFactor = 1.00000;

  yield();
  dataFile.print(sensorArray[index].sensorID);
  dataFile.print("; ");
  dataFile.print(sensorArray[index].position);
  dataFile.print("; ");
  dataFile.print(sensorArray[index].name);
  dataFile.print("; ");
  dataFile.print(sensorArray[index].tempOffset, 6);
  dataFile.print("; ");
  dataFile.print(sensorArray[index].tempFactor, 6);
  dataFile.print("; ");
  dataFile.println();

  dataFile.close();

  Debugln(" .. Done\r");

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  return true;

} // appendIniFile()


//===========================================================================================
bool readIniFile(int8_t index, char *devAddr)
{
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
      sensorArray[index].tempC = -99;
      sprintf(sensorArray[index].sensorID, "%s", tmpS.c_str());
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


//===========================================================================================
bool writeDataPoints()
{
  DebugTln("writeDataPoints(/dataPoints.csv) ..");

  if (!SPIFFSmounted) {
    Debugln("No SPIFFS filesystem..ABORT!!!\r");
    return false;
  }

  // --- check if the file exists and can be opened ---
  File dataFile  = SPIFFS.open("/dataPoints.csv", "w");    // open for writing
  if (!dataFile) {
    Debugln("Some error opening [dataPoints.csv] .. bailing out!");
    return false;
  } // if (!dataFile)

  yield();
  for (int p=(_MAX_DATAPOINTS -1); p >= 0 ; p--) {  // last dataPoint is alway's zero
    //dataFile.print(p);                        dataFile.print(";");
    dataFile.print(dataStore[p].timestamp);
    dataFile.print(";");
    for(int s=0; s < noSensors; s++) {
      dataFile.print(s);
      dataFile.print(";");
      dataFile.print(dataStore[p].tempC[s]);
      dataFile.print(";");
    }
    dataFile.println();
  }
  dataFile.println("EOF");

  dataFile.close();

  Debugln(" .. Done\r");

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  return true;

} // writeDataPoints()


//===========================================================================================
bool readDataPoints()
{
  String tmpS;
  float pf, sf, tempC, tmpf;
  int p, s;
  int16_t plotNr;

  DebugTln("readDataPoints(/dataPoints.csv) ..");

  if (!SPIFFSmounted) {
    Debugln("No SPIFFS filesystem..ABORT!!!\r");
    return false;
  }

  // --- check if the file exists and can be opened ---
  File dataFile  = SPIFFS.open("/dataPoints.csv", "r");    // open for reading
  if (!dataFile) {
    Debugln("Some error opening [dataPoints.csv] .. bailing out!");
    return false;
  } // if (!dataFile)

  plotNr = _MAX_DATAPOINTS;
  while ((dataFile.available() > 0) && (plotNr > 0)) {
    plotNr--;
    yield();
    tmpS     = dataFile.readStringUntil('\n');
    //Debugf("Record[%d][%s]\n", plotNr, tmpS.c_str());
    if (tmpS == "EOF") break;

    extractFieldFromCsv(tmpS, pf);
    dataStore[plotNr].timestamp = (int)pf;
    while (extractFieldFromCsv(tmpS, sf)) {
      yield();
      s = (int)sf;
      extractFieldFromCsv(tmpS, tempC);
      dataStore[plotNr].tempC[s] = tempC;
    }
    //dataFile.print(dataStore[p].tempC[s]);  dataFile.print(";");
  }

  dataFile.close();

  Debugln(" .. Done \r");
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  return true;

} // readDataPoints()


//===========================================================================================
bool extractFieldFromCsv(String &in, float &f)
{
  char  field[in.length()];
  int   p = 0;
  bool  gotChar = false;

  //DebugTf("in is [%s]\n", in.c_str());
  for(int i = 0; i < in.length(); i++) {
    //--- copy chars up until ; \0 \r \n
    if (in[i] != ';' && in[i] != '\r' && in[i] != '\n' && in[i] != '\0') {
      field[p++] = in[i];
      gotChar = true;
    } else {
      field[p++] = '\0';
      break;
    }
  }
  //--- nothing copied ---
  if (!gotChar) {
    f = 0.0;
    return false;
  }
  String out = in.substring(p);
  in = out;
  //DebugTf("p[%d], in is now[%s]\n", p, in.c_str());

  f = String(field).toFloat();
  return true;

} // extractFieldFromCsv()

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
