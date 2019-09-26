/*
***************************************************************************
**  Program  : helperStuff, part of FloorTempMonitor
**  Version  : v0.6.1
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/


// function to print a device address
//===========================================================================================
void getSensorID(DeviceAddress deviceAddress, char* devID)
{
  sprintf(devID, "0x%02x%02x%02x%02x%02x%02x%02x%02x", deviceAddress[0], deviceAddress[1]
          , deviceAddress[2], deviceAddress[3]
          , deviceAddress[4], deviceAddress[5]
          , deviceAddress[6], deviceAddress[7]);
} // getSensorID()

// function to print the temperature for a device
//===========================================================================================
void sendSensorData(int8_t devNr)
{
  //-- time to shift datapoints?
  if ( ((now() / _PLOT_INTERVAL) > lastPlotTime) && (devNr == 0)) {
    lastPlotTime = (now() / _PLOT_INTERVAL);
    dataStore[(_MAX_DATAPOINTS -1)].timestamp = now();
    shiftUpDatapoints();
    publishDatapoints();
    //-- no point in wear out the flash memory
    if (hour() != lastSaveHour) {
      lastSaveHour = hour();
      writeDataPoints();
    }
  }

  float tempR = sensors.getTempCByIndex(_S[devNr].index);
  float tempC = tempR;

  if (tempR < 5.0 || tempR > 100.0) {
    Debugln("invalid reading");
    return;
  }

  if (readRaw) {
    _S[devNr].tempC = tempR;

  } else if (_S[devNr].tempC == -99) { // first reading
    //--- https://www.letscontrolit.com/wiki/index.php/Basics:_Calibration_and_Accuracy
    tempC = ( (tempR + _S[devNr].tempOffset) * _S[devNr].tempFactor );

  } else {  // compensated Temp
    //--- realTemp/sensorTemp = tempFactor
    //--- https://www.letscontrolit.com/wiki/index.php/Basics:_Calibration_and_Accuracy
    tempC = ( (tempR + _S[devNr].tempOffset) * _S[devNr].tempFactor );
    tempC = ((_S[devNr].tempC * 3.0) + tempC) / 4.0;
  }
  _S[devNr].tempC = tempC;

  // &deg;    // degrees
  // &Delta;  // Delta
  Debugf("Temp R/C: %6.3f *C / %6.3f *C \n", tempC, tempR);
  sprintf(cMsg, "tempC%d=%6.3f&#176;C", devNr, tempC);
  webSocket.sendTXT(wsClientID, cMsg);

  //--- hier iets slims om de temperatuur grafisch weer te geven ---
  //--- bijvoorbeeld de afwijking t.o.v. ATAG uit temp en ATAG retour temp
  sprintf(cMsg, "barRange=low %d&deg;C - high %d&deg;C", 10, 60);
  webSocket.sendTXT(wsClientID, cMsg);
  int8_t mapTemp = map(tempC, 10, 60, 0, 100); // mappen naar 0-100% van de bar
  sprintf(cMsg, "tempBar%dB=%d", devNr, mapTemp);
  webSocket.sendTXT(wsClientID, cMsg);

  int32_t   stateTime = 0;
  char      cStateTime[15];
  
  switch (_S[devNr].servoState ) {
    case SERVO_IS_OPEN:
            sprintf(cMsg, "servoState%dS=OPEN (&Delta;T %.2f&deg;C)", devNr, (_S[0].tempC - _S[devNr].tempC));
            break;
    case SERVO_IS_CLOSED:
            stateTime = (_S[0].loopTime * _MIN) - (millis() - (_S[devNr].servoTimer * _MIN));
            if (stateTime > 0) {
              DebugTf("CLOSE time [%d] > 0\n", (stateTime / _MIN) +1);
              if (stateTime > _MIN)
                    sprintf(cStateTime, "(%d min.)", (stateTime + _MIN) / _MIN); // '+ _MIN' voor afronding
              else  sprintf(cStateTime, "(%d sec.)",  stateTime / 1000);
            } else {
              DebugTf("CLOSE time [%d] < 0\n", stateTime / _MIN);
              cStateTime[0] = '.';
              cStateTime[1] = '.';
              cStateTime[2] = '.';
              cStateTime[3] = '\0';
            }
            sprintf(cMsg, "servoState%dS=CLOSED %s", devNr, cStateTime);
            break;
    case SERVO_IN_LOOP:
            stateTime = (_S[devNr].loopTime * _MIN) - (millis() - (_S[devNr].servoTimer * _MIN));
            if (stateTime > 0) {
              DebugTf("LOOP time [%d] > 0\n", (stateTime / _MIN) +1);
              if (stateTime > _MIN)
                    sprintf(cStateTime, " (%d min.)", (stateTime + _MIN) / _MIN);  // '+ _MIN' voor afronding ...
              else  sprintf(cStateTime, " (%d sec.)",  stateTime / 1000);
            } else {
              DebugTf("LOOP time [%d] < 0\n", stateTime / _MIN);
              cStateTime[0] = '.';
              cStateTime[1] = '.';
              cStateTime[2] = '.';
              cStateTime[3] = '\0';
            }
            sprintf(cMsg, "servoState%dS=LOOP %s", devNr, cStateTime);
            break;
  }
//if (devNr > 1) {  // devNr 0 and 1 are the the heater SKIP!
  if (devNr > 0) {  // for testing (I have only 2 sensors)
    webSocket.sendTXT(wsClientID, cMsg);
  }
  
  dataStore[(_MAX_DATAPOINTS -1)].timestamp    = now();
  dataStore[(_MAX_DATAPOINTS -1)].tempC[devNr] = tempC;

} // sendSensorData


//===========================================================================================
void handleSensors()
{
//char devID[20];

  for(int sensorNr = 0; sensorNr < noSensors; sensorNr++) {
    Debugf("SensorID: [%s] [%-30.30s] ", _S[sensorNr].sensorID, _S[sensorNr].name);
    sendSensorData(sensorNr);
  }

} // handleSensors()


//=======================================================================
void printSensorArray()
{
  for(int8_t s=0; s<noSensors; s++) {
    Debugf("[%2d] => [%2d], [%02d], [%s], [%-20.20s], [%7.6f], [%7.6f]\n", s
           , _S[s].index
           , _S[s].position
           , _S[s].sensorID
           , _S[s].name
           , _S[s].tempOffset
           , _S[s].tempFactor);
  }
} // printSensorArray()

//=======================================================================
void sortSensors()
{
  int x, y;

  for (int8_t y = 0; y < noSensors; y++) {
    yield();
    for (int8_t x = y + 1; x < noSensors; x++)  {
      //DebugTf("y[%d], x[%d] => seq[x][%d] ", y, x, _S[x].position);
      if (_S[x].position < _S[y].position)  {
        sensorStruct temp = _S[y];
        _S[y] = _S[x];
        _S[x] = temp;
      } /* end if */
      //Debugln();
    } /* end for */
  } /* end for */

} // sortSensors()

//=======================================================================
void shiftUpDatapoints()
{
  for (int p = 0; p < (_MAX_DATAPOINTS -1); p++) {
    dataStore[p] = dataStore[(p+1)];
  }
  for (int s=0; s < _MAX_SENSORS; s++) {
    dataStore[(_MAX_DATAPOINTS -1)].tempC[s] = 0;
  }

} // shiftUpDatapoints()

//=======================================================================
void publishDatapoints()
{
  char cPoints[(sizeof(char) * _MAX_SENSORS * 15)];
  char cLine[(sizeof(cPoints) + 20)];

  Debugln();
  for (int p=0; p < (_MAX_DATAPOINTS -1); p++) {  // last dataPoint is alway's zero
    yield();
    sprintf(cMsg, "plotPoint=%d:TS=(%d) %02d-%02d", p, day(dataStore[p].timestamp)
            , hour(dataStore[p].timestamp)
            , minute(dataStore[p].timestamp));
    //DebugTf("[%s]\n", cMsg);
    cPoints[0] = '\0';
    for(int s=0; s < noSensors; s++) {
      if (s == 0)
            sprintf(cPoints, "S%d=%f",             s, dataStore[p].tempC[s]);
      else  sprintf(cPoints, "%s:S%d=%f", cPoints, s, dataStore[p].tempC[s]);
      //DebugTf("-->[%s]\n", cPoints);
    }
    sprintf(cLine, "%s:%s", cMsg, cPoints);
    if (dataStore[p].timestamp > 0) {
      //DebugTf("sendTX(%d, [%s]\n", wsClientID, cLine);
      webSocket.sendTXT(wsClientID, cLine);
      delay(10);  //-- give it some time to finish
    }
  }

} // publishDatapoints()

//=======================================================================
int8_t editSensor(int8_t recNr)
{
  sensorStruct tmpRec = readIniFileByRecNr(recNr);
  if (tmpRec.sensorID == "-") {
    DebugTf("Error: something wrong reading record [%d] from [/sensor.ini]\n", recNr);
    return -1;
  }
  sprintf(cMsg, "msgType=editSensor,sensorNr=%d,sID=%s,sName=%s,sPosition=%d,sTempOffset=%.6f,sTempFactor=%.6f,sServo=%d,sDeltaTemp=%.1f,sLoopTime=%d"
                              , recNr
                              , tmpRec.sensorID
                              , tmpRec.name
                              , tmpRec.position
                              , tmpRec.tempOffset
                              , tmpRec.tempFactor
                              , tmpRec.servoNr
                              , tmpRec.deltaTemp
                              , tmpRec.loopTime);
   DebugTln(cMsg);                           
   webSocket.sendTXT(wsClientID, cMsg);
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
        if (tmpRec.position <  0) tmpRec.position = 0;
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
    } else if (strncmp(pch, "loopTime", 8) == 0) { 
        sprintf(cMsg, "%s", splitFldVal(pch, '='));
        tmpRec.loopTime = String(cMsg).toInt();
        if (tmpRec.loopTime <   0) tmpRec.loopTime =   0;
        if (tmpRec.loopTime > 120) tmpRec.loopTime = 120;
    } else {
        Debugf("Don't know what to do with[%s]\n", pch);
    }
    pch = strtok (NULL, ",");
  }

  Debugf("tmpRec ID[%s], name[%s], position[%d]\n   tempOffset[%.6f], tempFactor[%.6f]\n   Servo[%d], deltaTemp[%.1f], loopTime[%d]\n"
                                                , tmpRec.sensorID
                                                , tmpRec.name
                                                , tmpRec.position
                                                , tmpRec.tempOffset
                                                , tmpRec.tempFactor
                                                , tmpRec.servoNr
                                                , tmpRec.deltaTemp
                                                , tmpRec.loopTime);

  recNr = updateIniRec(tmpRec);
  if (recNr >= 0) {
    editSensor(recNr);
  }
  
} // updSensor()

//=======================================================================
char* splitFldVal(char *pair, char del)
{
  bool    isVal = false;
  char    Value[50];
  int8_t  inxVal = 0;

  DebugTf("pair[%s] (length[%d]) del[%c] \n", pair, strlen(pair), del);

  for(int i=0; i<strlen(pair); i++) {
    if (isVal) {
      Value[inxVal++] = pair[i];
      Value[inxVal]   = '\0';
    } else if (pair[i] == del) {
      isVal   = true;
      inxVal  = 0;
    }
  }
  DebugTf("Value [%s]\n", Value);
  return Value;
  
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
