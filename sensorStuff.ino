/*
***************************************************************************  
**  Program  : helperStuff, part of FloorTempMonitor
**  Version  : v0.4.0
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/


// function to print a device address
//===========================================================================================
void getSensorID(DeviceAddress deviceAddress, char* devAddr)
{
  sprintf(devAddr, "0x%02x%02x%02x%02x%02x%02x%02x%02x", deviceAddress[0], deviceAddress[1]
                                                       , deviceAddress[2], deviceAddress[3]
                                                       , deviceAddress[4], deviceAddress[5]
                                                       , deviceAddress[6], deviceAddress[7]);
} // getSensorID()

// function to print the temperature for a device
//===========================================================================================
void printTemperature(int8_t devNr)
{
  //-- time to shift datapoints?
  if ( ((now() / _PLOT_INTERVAL) > lastPlotTime) && (devNr == 0)) {
    lastPlotTime = (now() / _PLOT_INTERVAL);
    shiftUpDatapoints();
    printDatapoints();
    writeDataPoints();
  }

  float tempR = sensors.getTempCByIndex(sensorArray[devNr].index);
  float tempC = tempR;
  //--- https://www.letscontrolit.com/wiki/index.php/Basics:_Calibration_and_Accuracy

  if (tempR < -10.0 || tempR > 100.0) {
    Debug("invalid reading");
    return;
  }
  
  if (readRaw) {
    sensorArray[devNr].tempC = tempR;
    
  } else if (sensorArray[devNr].tempC == -99) { // first reading
    tempC = ( (tempR + sensorArray[devNr].tempOffset) * sensorArray[devNr].tempFactor );  
    
  } else {  // compensated Temp
    //---- realTemp/sensorTemp = tempFactor
    tempC = ( (tempR + sensorArray[devNr].tempOffset) * sensorArray[devNr].tempFactor );
    tempC = ((sensorArray[devNr].tempC * 3.0) + tempC) / 4.0;
  }
  sensorArray[devNr].tempC = tempC;
  
  Debugf("Temp R/C: %6.3f ℃ / %6.3f ℃", tempC, tempR);
  sprintf(cMsg, "tempC%d=%6.3f℃", devNr, tempC);
  webSocket.sendTXT(wsClientID, cMsg);

  //--- hier iets slims om de temperatuur grafisch weer te geven ---
  //--- bijvoorbeeld de afwijking t.o.v. ATAG uit temp en ATAG retour temp
  sprintf(cMsg, "barRange=low %d℃ - high %d℃", 10, 60);
  webSocket.sendTXT(wsClientID, cMsg);
  int8_t mapTemp = map(tempC, 10, 60, 0, 100); // mappen naar 0-100% van de bar
  sprintf(cMsg, "tempBar%dB=%d", devNr, mapTemp);
  webSocket.sendTXT(wsClientID, cMsg);
    
  dataStore[(_MAX_DATAPOINTS -1)].timestamp = now();
  dataStore[(_MAX_DATAPOINTS -1)].tempC[devNr] = tempC;

} // printTemperature


// function to print information about a device
//===========================================================================================
void printData(int8_t devNr)
{
  char devAddr[20];

  //getSensorID(deviceAddress, devAddr);
  Debugf("SensorID: [%s] [%-30.30s] ", sensorArray[devNr].sensorID, sensorArray[devNr].name);
  //printTemperature(sensorArray[devNr].sensorID);
  printTemperature(devNr);
  Debugln();
  
} // printData()


//=======================================================================
void printSensorArray()
{
  for(int8_t s=0; s<noSensors; s++) {
    Debugf("[%2d] => [%2d], [%02d], [%s], [%-20.20s], [%7.6f], [%7.6f]\n", s
                      , sensorArray[s].index
                      , sensorArray[s].position
                      , sensorArray[s].sensorID
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
void printDatapoints() 
{
  char cPoints[(sizeof(char) * _MAX_SENSORS * 15)];
  char cLine[(sizeof(cPoints) + 20)];

  Debugln();
  for (int p=0; p < (_MAX_DATAPOINTS -1); p++) {  // last dataPoint is alway's zero
    sprintf(cMsg, "plotPoint=%d:TS=%d", p, dataStore[p].timestamp);
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
      DebugTf("sendTX(%d, [%s]\n", wsClientID, cLine);
      webSocket.sendTXT(wsClientID, cLine);
    }
  }
  
} // printDatapoints()

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
