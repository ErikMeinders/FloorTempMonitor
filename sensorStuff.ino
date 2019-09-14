/*
***************************************************************************  
**  Program  : helperStuff, part of FloorTempMonitor
**  Version  : v1.0.3
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/


// function to print a device address
//===========================================================================================
void getDevAddress(DeviceAddress deviceAddress, char* devAddr)
{
  sprintf(devAddr, "0x%02x%02x%02x%02x%02x%02x%02x%02x", deviceAddress[0], deviceAddress[1]
                                                       , deviceAddress[2], deviceAddress[3]
                                                       , deviceAddress[4], deviceAddress[5]
                                                       , deviceAddress[6], deviceAddress[7]);
} // getDevAddress()

// function to print the temperature for a device
//===========================================================================================
void printTemperature(int8_t devNr)
{
  float tempC = sensors.getTempCByIndex(sensorArray[devNr].index);
  //--- https://www.letscontrolit.com/wiki/index.php/Basics:_Calibration_and_Accuracy

  if (tempC < -10.0 || tempC > 100.0) return;
  
  if (readRaw) {
    sensorArray[devNr].tempC = tempC;
    
  } else if (sensorArray[devNr].tempC == -99) { // first reading
    tempC = ( (tempC + sensorArray[devNr].tempOffset) * sensorArray[devNr].tempFactor );  
    
  } else {  // compensated Temp
    //---- realTemp/sensorTemp = tempFactor
    tempC = ( (tempC + sensorArray[devNr].tempOffset) * sensorArray[devNr].tempFactor );
    tempC = ((sensorArray[devNr].tempC * 3.0) + tempC) / 4.0;
  }
  sensorArray[devNr].tempC = tempC;
  
  Debugf("Temp C: %6.2f / %6.3f", tempC, tempC);
  sprintf(cMsg, "tempC%d=%6.2f ℃", devNr, tempC);
  webSocket.sendTXT(wsClientID, cMsg);


} // printTemperature

// function to print information about a device
//===========================================================================================
void printData(int8_t devNr)
{
  char devAddr[20];

  //getDevAddress(deviceAddress, devAddr);
  Debugf("Device Address: [%s] [%-30.30s] ", sensorArray[devNr].sensorID, sensorArray[devNr].name);
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
