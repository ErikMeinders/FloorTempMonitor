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
bool appendIniFile(int8_t index, char* devID)
{
  char    fixedRec[100];
  int16_t bytesWritten;
  
  DebugTf("appendIniFile(/sensors.ini) .. [%d] - sensorID[%s]\n", index, devID);

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
  sprintf(sensorArray[index].sensorID, "%s", devID);
  sensorArray[index].position = 90;
  sprintf(sensorArray[index].name, "new Sensor");
  sensorArray[index].tempOffset = 0.00000;
  sensorArray[index].tempFactor = 1.00000;
  sensorArray[index].servoNr    = 0;
  sensorArray[index].deltaTemp  = 20.0;
  sensorArray[index].loopTime   = 30;
  sprintf(fixedRec, "%s; %d; %-15.15s; %8.6f; %8.6f; %2d; %4.1f; %3d;"
                                                , sensorArray[index].sensorID
                                                , sensorArray[index].position
                                                , sensorArray[index].name
                                                , sensorArray[index].tempOffset
                                                , sensorArray[index].tempFactor
                                                , sensorArray[index].servoNr
                                                , sensorArray[index].deltaTemp
                                                , sensorArray[index].loopTime
                                          );
  yield();
  fixRecLen(fixedRec, _FIX_SETTINGSREC_LEN);
  bytesWritten = dataFile.print(fixedRec);
  if (bytesWritten != _FIX_SETTINGSREC_LEN) {
    DebugTf("ERROR!! written [%d] bytes but should have been [%d] for Label[%s]\r\n"
                                            , bytesWritten, _FIX_SETTINGSREC_LEN, fixedRec);
  }

  dataFile.close();

  Debugln(" .. Done\r");

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  return true;

} // appendIniFile()


//===========================================================================================
bool updateIniRec(sensorStruct newRec)
{
  char    fixedRec[150];
  int16_t recNr;
  bool    foundSensor = false;
  int16_t bytesWritten;
  
  DebugTf("updateIniRec(/sensors.ini) - sensorID[%s]\n", newRec.sensorID);

  if (!SPIFFSmounted) {
    Debugln("No SPIFFS filesystem..ABORT!!!\r");
    return false;
  }
  String  tmpS;

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  File dataFile = SPIFFS.open("/sensors.ini", "r+"); // open for reading and writing
  recNr = 0;
  foundSensor = false;
  while (dataFile.available() > 0 && !foundSensor) {
    tmpS     = dataFile.readStringUntil(';'); // read sensorIF
    tmpS.trim();
    if (String(newRec.sensorID) == String(tmpS)) {
      foundSensor = true;
    } else {
      String n = dataFile.readStringUntil('\n');  // goto next record
      recNr++;
    }
  }

  if (!foundSensor) {
    dataFile.close();
    DebugTf("sensorID [%s] not found. Bailing out!\n", newRec.sensorID);
    return false;
  }

  yield();
  sprintf(fixedRec, "%s; %d; %-15.15s; %8.6f; %8.6f; %2d; %4.1f; %3d;"
                                                , newRec.sensorID
                                                , newRec.position
                                                , newRec.name
                                                , newRec.tempOffset
                                                , newRec.tempFactor
                                                , newRec.servoNr
                                                , newRec.deltaTemp
                                                , newRec.loopTime
                                          );
  yield();
  fixRecLen(fixedRec, _FIX_SETTINGSREC_LEN);
  dataFile.seek((recNr * _FIX_SETTINGSREC_LEN), SeekSet);
  bytesWritten = dataFile.print(fixedRec);
  if (bytesWritten != _FIX_SETTINGSREC_LEN) {
    DebugTf("ERROR!! written [%d] bytes but should have been [%d] for Label[%s]\r\n"
                                            , bytesWritten, _FIX_SETTINGSREC_LEN, fixedRec);
  }

  dataFile.close();

  Debugln(" .. Done\r");

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  return true;

} // updateIniRec()


//===========================================================================================
bool readIniFile(int8_t index, char* devID)
{
  File    dataFile;
  String  tmpS;

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  DebugTf("readIniFile(/sensors.ini) .. sensorID[%s] \r\n", devID);

  if (!SPIFFSmounted) {
    DebugTln("No SPIFFS filesystem..\r");
    return false;
  }

  dataFile = SPIFFS.open("/sensors.ini", "r");
  while (dataFile.available() > 0) {
    tmpS     = dataFile.readStringUntil(';'); // first field is sensorID
    tmpS.trim();
    DebugTf("Processing [%s]\n", tmpS.c_str());
    if (String(devID) == String(tmpS)) {
      sensorArray[index].index = index;
      sensorArray[index].tempC = -99;
      sprintf(sensorArray[index].sensorID, "%s", tmpS.c_str());
      sensorArray[index].position    = dataFile.readStringUntil(';').toInt();
      tmpS     = dataFile.readStringUntil(';');
      tmpS.trim();
      sprintf(sensorArray[index].name, "%s", tmpS.c_str());
      sensorArray[index].tempOffset  = dataFile.readStringUntil(';').toFloat();
      sensorArray[index].tempFactor  = dataFile.readStringUntil(';').toFloat();
      sensorArray[index].servoNr     = dataFile.readStringUntil(';').toInt();
      sensorArray[index].deltaTemp   = dataFile.readStringUntil(';').toFloat();
      sensorArray[index].loopTime    = dataFile.readStringUntil(';').toInt();
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
sensorStruct readIniFileByRecNr(int8_t &recNr)
{
  File          dataFile;
  sensorStruct  tmpRec;
  String        tmpS;
  int8_t        recsInFile;
  
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  if (recNr < 0)  recNr = 0;

  DebugTf("From '/sensors.ini' .. read recNr[%d] \r\n", recNr);
  
  sprintf(tmpRec.sensorID, "%s", "-");

  if (!SPIFFSmounted) {
    DebugTln("No SPIFFS filesystem..\r");
    return tmpRec;
  }

  dataFile = SPIFFS.open("/sensors.ini", "r");
  //--- don't read byond last record!
  recsInFile  = ((dataFile.size() +1) / _FIX_SETTINGSREC_LEN);  // records in file
  DebugTf("fileSize[%d] bytes, records in file[%d]\n", dataFile.size()
                                                     , recsInFile);
  while ((recNr > 0 ) && (recNr > (recsInFile -1))) {
    recNr--;
    DebugTf("step down to recNr[%d]\n", recNr);
  }
  
  dataFile.seek((recNr * _FIX_SETTINGSREC_LEN), SeekSet);
  while (dataFile.available() > 0) {
    tmpS                = dataFile.readStringUntil(';'); // first field is sensorID
    tmpS.trim();
    DebugTf("Processing [%s]\n", tmpS.c_str());
    sprintf(tmpRec.sensorID, "%s", tmpS.c_str());
    tmpRec.position     = dataFile.readStringUntil(';').toInt();
    tmpS                = dataFile.readStringUntil(';');
    tmpS.trim();
    sprintf(tmpRec.name, "%s", tmpS.c_str());
    tmpRec.tempOffset   = dataFile.readStringUntil(';').toFloat();
    tmpRec.tempFactor   = dataFile.readStringUntil(';').toFloat();
    tmpRec.servoNr      = dataFile.readStringUntil(';').toInt();
    tmpRec.deltaTemp    = dataFile.readStringUntil(';').toFloat();
    tmpRec.loopTime     = dataFile.readStringUntil(';').toInt();
    dataFile.close();
    return tmpRec;
  }

  dataFile.close();

  DebugTln(" .. Done not found!\r");
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  return tmpRec;

} // readIniFileByRecNr()


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


//===========================================================================================
void fixRecLen(char *record, int8_t len) 
{
  DebugTf("record[%s] fix to [%d] bytes\r\n", String(record).c_str(), len);
  int8_t s = 0, l = 0;
  while (record[s] != '\0' && record[s]  != '\n') {s++;}
  DebugTf("Length of record is [%d] bytes\r\n", s);
  for (l = s; l < (len - 2); l++) {
    record[l] = ' ';
  }
  record[l]   = ';';
  record[l+1] = '\n';
  record[len] = '\0';

  while (record[l] != '\0') {l++;}
  DebugTf("Length of record is now [%d] bytes\r\n", l);
  
} // fixRecLen()



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
