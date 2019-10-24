/*
***************************************************************************
**  Program  : webSocketEvent, part of FloorTempMonitor
**  Version  : v0.6.6
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/



//===========================================================================================
void webSocketEvent(uint8_t wsClient, WStype_t WStype, uint8_t * payload, size_t length)
{
  String  wsPayload = String((char *) &payload[0]);
  //char *  wsPayloadC = (char *) &payload[0];
  String  wsString;

  wsClientID = wsClient;  // only the last connected Client gets updates

  switch (WStype) {
    
    case WStype_ERROR: 
      DebugTf("Some ERROR occured .. [%d] get binary length: %d\n", wsClient, length);
      break;

    case WStype_DISCONNECTED:
      DebugTf("[%u] Disconnected!\r\n", wsClient);
      connectedToWebsocket = false;
      break;

    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(wsClient);
        if (!connectedToWebsocket) {
          DebugTf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", wsClient, ip[0], ip[1], ip[2], ip[3], payload);
          connectedToWebsocket = true;
          if (connectedToMux) {
            webSocket.sendTXT(wsClient, "state=Connected");
          } else {
            webSocket.sendTXT(wsClient, "state=No I2CMUX Module");
          }
          readRaw = false;
          sprintf(cMsg, "noSensors=%d", noSensors);
          webSocket.sendTXT(wsClient, cMsg);
          onlyUpdateLastPoint = false;
        }

      }
      break;
        
    case WStype_TEXT:
      DebugTf("[%u] Got message: [%s]\r\n", wsClient, payload);
      String FWversion = String(_FW_VERSION);
      
      if (wsPayload.indexOf("updateDOM") > -1) {
        DebugTln("now updateDOM()!");
        delay(100); // give it some time to load chartJS library and stuff
        updateDOM();
        //onlyUpdateLastPoint = false;
      } else
      if (wsPayload.indexOf("DOMloaded") > -1) {
        DebugTln("received DOMloaded, send some datapoints");
        delay(100);
        readRaw = false;
        forceUpdateSensorsDisplay();
        updateDatapointsDisplay();
      } else
      if (wsPayload.indexOf("rawMode") > -1) {
        DebugTln("set readRaw = true;");
        readRaw = true;
        forceUpdateSensorsDisplay();
        onlyUpdateLastPoint = false;
        updateDatapointsDisplay();
      } else 
      if (wsPayload.indexOf("chartType=N") > -1) {
        DebugTln("set chartType = N");
        onlyUpdateLastPoint       = false;
        onlyUpdateSensors    = false;
        onlyUpdateServos  = false;
        updateDatapointsDisplay();
      } else 
      if (wsPayload.indexOf("chartType=T") > -1) {
        DebugTln("set chartType = T;");
        onlyUpdateLastPoint       = false;
        onlyUpdateSensors    = true;
        onlyUpdateServos  = false;
        updateDatapointsDisplay();
      } else 
      if (wsPayload.indexOf("chartType=S") > -1) {
        DebugTln("set chartType = T;");
        onlyUpdateLastPoint       = false;
        onlyUpdateSensors    = false;
        onlyUpdateServos  = true;
        updateDatapointsDisplay();
      } else 
      if (wsPayload.indexOf("calibratedMode") > -1) {
        DebugTln("set readRaw = false;");
        readRaw = false;
        forceUpdateSensorsDisplay();
        onlyUpdateLastPoint = false;
        onlyUpdateSensors   = true;
        updateDatapointsDisplay();
      } else
      //---- sensorEdit.html ---------------------------------
      if (wsPayload.indexOf("getFirstSensor") > -1) {
        DebugTln("getFirstSensor()!");
        editSensor(0);
      } else
      if (wsPayload.indexOf("editSensorNr") > -1) {
        wsPayload.replace("editSensorNr=", "");
        DebugTf("editSensorNr(%d)!\n", wsPayload.toInt());
        editSensor(wsPayload.toInt());
      } else 
      if (wsPayload.indexOf("updSensorNr") > -1) {
        //wsPayload.replace("updSensorNr=", "");
        //DebugTf("updSensorNr(%d) .. \n", wsPayload.toInt());
        char data[200];
        sprintf(data, "%s", wsPayload.c_str());
        updSensor(data);
      }
      break;

  } // switch(WStype)

} // webSocketEvent()


//===========================================================================================
void handlePing()
{
  if ( DUE (pingCheck) ) {
    for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
      webSocket.sendPing(i);
    }
  } 

} // handlePing()

//===========================================================================================
void handleWebSockRefresh()
{
  if (DUE(screenClockRefresh)) {
    String DT  = buildDateTimeString();
    sprintf(cMsg, "clock=[I2C disconnects %d] | [upTime:%15.15s] | [WiFi RSSI: %4ddBm] | [%s]"
                                                        , connectionMuxLostCount
                                                        , upTime().c_str()
                                                        , WiFi.RSSI()
                                                        , DT.c_str());
    DebugTln(cMsg);
    webSocket.sendTXT(wsClientID, cMsg);
    if (connectedToMux) {
      sprintf(cMsg, "state=Connected");
    } else {
      sprintf(cMsg, "state=No I2CMUX Module");
    }
    DebugTln(cMsg);
    webSocket.sendTXT(wsClientID, cMsg);
  }

} // handleWebSockRefresh()

/********
//===========================================================================================
void updateSysInfo(uint8_t wsClient)
{
  String wsString;

  //-Device Info-----------------------------------------------------------------
  wsString += ",SysAuth=Willem Aandewiel";
  wsString += ",SysFwV="            + String( _FW_VERSION );
  wsString += ",Compiled="          + String( __DATE__ )
              + String( "  " )
              + String( __TIME__ );
  wsString += ",FreeHeap="          + String( ESP.getFreeHeap() );
  wsString += ",ChipID="            + String( ESP.getChipId(), HEX );
  wsString += ",CoreVersion="       + String( ESP.getCoreVersion() );
  wsString += ",SdkVersion="        + String( ESP.getSdkVersion() );
  wsString += ",CpuFreqMHz="        + String( ESP.getCpuFreqMHz() );
  wsString += ",SketchSize="        + String( (ESP.getSketchSize() / 1024.0), 3) + "kB";
  wsString += ",FreeSketchSpace="   + String( (ESP.getFreeSketchSpace() / 1024.0), 3 ) + "kB";

  if ((ESP.getFlashChipId() & 0x000000ff) == 0x85)
    sprintf(cMsg, "%08X (PUYA)", ESP.getFlashChipId());
  else  sprintf(cMsg, "%08X", ESP.getFlashChipId());
  wsString += ",FlashChipID="       + String(cMsg);  // flashChipId
  wsString += ",FlashChipSize="     + String( (float)(ESP.getFlashChipSize() / 1024.0 / 1024.0), 3 ) + "MB";
  wsString += ",FlashChipRealSize=" + String( (float)(ESP.getFlashChipRealSize() / 1024.0 / 1024.0), 3 ) + "MB";
  wsString += ",FlashChipSpeed="    + String( (float)(ESP.getFlashChipSpeed() / 1000.0 / 1000.0) ) + "MHz";

  FlashMode_t ideMode = ESP.getFlashChipMode();
  wsString += ",FlashChipMode="     + String( flashMode[ideMode] );
  wsString += ",BoardType=";
#ifdef ARDUINO_ESP8266_NODEMCU
  wsString += String("ESP8266_NODEMCU");
#endif
#ifdef ARDUINO_ESP8266_GENERIC
  wsString += String("ESP8266_GENERIC");
#endif
#ifdef ESP8266_ESP01
  wsString += String("ESP8266_ESP01");
#endif
#ifdef ESP8266_ESP12
  wsString += String("ESP8266_ESP12");
#endif
#ifdef ARDUINO_ESP8266_WEMOS_D1R1
  wsString += String("Wemos D1 R1");
#endif

  wsString += ",SSID="              + String( WiFi.SSID() );
#ifdef SHOW_PASSWRDS
  wsString += ",PskKey="            + String( WiFi.psk() );
#else
  wsString += ",PskKey=*********";
#endif
  wsString += ",IpAddress="         + WiFi.localIP().toString() ;
  wsString += ",WiFiRSSI="          + String( WiFi.RSSI() );
  wsString += ",Hostname="          + String( _HOSTNAME );
  wsString += ",upTime="            + String( upTime() );

  webSocket.sendTXT(wsClientID, "msgType=sysInfo" + wsString);

} // updateSysInfo()
********/



//===========================================================================================
void updateDOM()
{
  for (int i = 0; i < noSensors; i++) {
    DebugTf("Add Sensor[%2d]: sensorID[%s] name[%s] relay[%d]\n", i
            , _SA[i].sensorID
            , _SA[i].name
            , _SA[i].servoNr);

    sprintf(cMsg, "index%d=%d:sensorID%d=%s:name%d=%s:tempC%d=-:tempBar%d=0:relay%d=%d:servoState%d=-"
            , i, i                // index, index
            , i, _SA[i].sensorID  // index, sensorID
            , i, _SA[i].name      // index, name
            , i, i                // temp, tempBar
            , i, _SA[i].servoNr   // index, servoNr
            , i);                 // servoState
    webSocket.sendTXT(wsClientID, "updateDOM:" + String(cMsg));
  }

} // updateDOM()


/***************************************************************************

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to permit
  persons to whom the Software is furnished to do so, subject to the
  following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
  OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
  THE USE OR OTHER DEALINGS IN THE SOFTWARE.

***************************************************************************/
