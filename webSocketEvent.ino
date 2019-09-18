/*
***************************************************************************  
**  Program  : webSocketEvent, part of FloorTempMonitor
**  Version  : v0.4.0
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/


static String   prevTimestamp, thisTime;
static bool     graphActual = false;
static int8_t   savMin = 0;
static uint32_t updateClock = millis() + 1000;
static String   wOut[10], wParm[30], wPair[4];
static bool     doUpdateDOM;


//===========================================================================================
void webSocketEvent(uint8_t wsClient, WStype_t type, uint8_t * payload, size_t lenght) {
//===========================================================================================
    String  wsPayload = String((char *) &payload[0]);
    char *  wsPayloadC = (char *) &payload[0];
    String  wsString;

    wsClientID = wsClient;

    switch(type) {
        case WStype_DISCONNECTED:
            DebugTf("[%u] Disconnected!\r\n", wsClient);
            isConnected = false;
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(wsClient);
                if (!isConnected) {
                 DebugTf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", wsClient, ip[0], ip[1], ip[2], ip[3], payload);
                 isConnected = true;
                 webSocket.sendTXT(wsClient, "state=Connected");
                 sprintf(cMsg, "noSensors=%d", noSensors);
                 webSocket.sendTXT(wsClient, cMsg);
                 //updateDOM();
                }
        
            }
            break;
            
        case WStype_TEXT:
            DebugTf("[%u] Got message: [%s]\r\n", wsClient, payload);
            String FWversion = String(_FW_VERSION);

            updateClock = millis();
            
            if (wsPayload.indexOf("updateDOM") > -1) {
              DebugTln("now updateDOM()!");
              updateDOM();
              printDatapoints() ;
            } 
            if (wsPayload.indexOf("DOMloaded") > -1) {
              DebugTln("set doUpdateDOM = false;");
              doUpdateDOM = false;
            } 
            if (wsPayload.indexOf("rawMode") > -1) {
              DebugTln("set readRaw = true;");
              readRaw = true;
            } else if (wsPayload.indexOf("calibratedMode") > -1) {
              DebugTln("set readRaw = false;");
              readRaw = false;
            } 
            break;
                        
    } // switch(type)
  
} // webSocketEvent()


//===========================================================================================
void handleWebSockRefresh() {
//===========================================================================================
  
    if (millis() > updateClock) {
      updateClock = millis() + 5000;
      savMin      = minute();
      String DT   = buildDateTimeString();
      webSocket.sendTXT(wsClientID, "clock=" + DT);
    }

} // handleWebSockRefreshh()


//=======================================================================
void updateSysInfo(uint8_t wsClient) {
//=======================================================================
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



//===========================================================================================
void updateDOM() 
{
  for (int i=0; i<noSensors; i++) {
    DebugTf("Add Sensor[%2d]: sensorID[%d] name[%s]\n", i
                                        , sensorArray[i].sensorID
                                        , sensorArray[i].name);
                                        
    sprintf(cMsg, "index%d=%d:sensorID%d=%s:name%d=%s:tempC%d=-:tempBar%d=0", i, i
                                                     , i, sensorArray[i].sensorID
                                                     , i, sensorArray[i].name
                                                     , i, i);
    webSocket.sendTXT(wsClientID, "updateDOM:" + String(cMsg));
  }

} // updateDOM()

/**
String sensorHTML(uint8_t vlgNr, String address, String name) {
    String devHtml;

//  devHtml  = "<div id='_wrapper'>\n";
//  devHtml += "  <div id='sensor' style='text-align:left;'>\n";
//  devHtml += "    <input type='text' name='index' value='" + String(vlgNr) + "' id='index" + String(vlgNr) + "' maxlength='3' size='3' readonly />\n";
//  devHtml += "    <input type='text' name='address' value='" + String(address) + "' id='address" + String(vlgNr) + "' readonly />\n";
//  devHtml += "    <input type='text' name='name' value='" + String(name) + "' id='name" + String(vlgNr) + "' style='font-size:20pt;' readonly />\n";
//  devHtml += "    <input type='text' name='tempC' value='-' id='tempC" + String(vlgNr) + "' style='font-size:20pt;' readonly />\n";
    devHtml += "    <span class='index'   id='index" + String(vlgNr) + "'>" + String(vlgNr) +"</span>\n";
    devHtml += "    <span class='address' id='address" + String(vlgNr) + "'>"+ String(address) +"</span>\n";
    devHtml += "    <span class='name'    id='name" + String(vlgNr) + "' style='font-size:20pt;'>"+ String(name) + "</span>\n";
    devHtml += "    <span class='tempC'   id='tempC" + String(vlgNr) + "' style='font-size:20pt;'>-</span>\n";
    devHtml += "  </div>\n";
    devHtml += "</div>\n";

    return devHtml;

} // sensorHTML()
**/

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
