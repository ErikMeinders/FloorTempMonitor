/*
**  Program   : ESP8266_basic 
*/
#define _FW_VERSION "v0.0.1 (11-09-2019)"
/*
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
  Arduino-IDE settings for this program:

    - Board: "Generic ESP8266 Module"
    - Flash mode: "DIO" | "DOUT"    // if you change from one to the other OTA will fail!
    - Flash size: "4M (1M SPIFFS)"  // ESP-01 "1M (256K SPIFFS)"  // PUYA flash chip won't work
    - DebugT port: "Disabled"
    - DebugT Level: "None"
    - IwIP Variant: "v2 Lower Memory"
    - Reset Method: "none"   // but will depend on the programmer!
    - Crystal Frequency: "26 MHz" 
    - VTables: "Flash"
    - Flash Frequency: "40MHz"
    - CPU Frequency: "80 MHz"
    - Buildin Led: "2"  // ESP-01 (Black) GPIO01 - Pin 2 // "2" for Wemos and ESP-01S
    - Upload Speed: "115200"                                                                                                                                                                                                                                                 
    - Erase Flash: "Only Sketch"
    - Port: <select correct port>
*/
#define _HOSTNAME "FloorTempMonitor"
/******************** compiler options  ********************************************/
#define USE_NTP_TIME
#define USE_UPDATE_SERVER
#define HAS_FSEXPLORER
#define SHOW_PASSWRDS
/******************** don't change anything below this comment **********************/

#include <TimeLib.h>            //  https://github.com/PaulStoffregen/Time
#include "Debug.h"
#include "networkStuff.h"
#include "indexPage.h"
#include <FS.h>                 // part of ESP8266 Core https://github.com/esp8266/Arduino
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into GPIO-00
#define ONE_WIRE_BUS 0
#define TEMPERATURE_PRECISION 12
#define _MAX_SENSORS  25

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
DeviceAddress DS18B20;

const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

typedef struct {
  int8_t  index;
  char    address[20];
  uint8_t position;
  char    name[50];
  float   tempOffset;
  float   tempFactor;
} sensorStruct;

sensorStruct  sensorArray[_MAX_SENSORS];

bool      SPIFFSmounted = false;
char      cMsg[150], fChar[10];
String    pTimestamp;
int8_t    prevNtpHour = 0;
uint64_t  upTimeSeconds;
uint32_t  nextPollTimer;
int8_t    noSensors;
bool      readRaw = false;
uint8_t   wsClientID;



//===========================================================================================
String upTime() 
{
  char    calcUptime[20];

  sprintf(calcUptime, "%d(d):%02d(h):%02d", int((upTimeSeconds / (60 * 60 * 24)) % 365)
                                          , int((upTimeSeconds / (60 * 60)) % 24)
                                          , int((upTimeSeconds / (60)) % 60));

  return calcUptime;

} // upTime()


//===========================================================================================
void handleIndexPage() 
{
  DebugTln("now in handleIndexPage() ..");
  String indexHtml = serverIndex;
  httpServer.send(200, "text/html", indexHtml);
}

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
  if (!readRaw) {
    //---- realTemp/sensorTemp = tempFactor
    tempC = ( (tempC + sensorArray[devNr].tempOffset) * sensorArray[devNr].tempFactor );
  }
  Debugf("Temp C: %6.2f / %6.3f", tempC, tempC);
  sprintf(cMsg, "tempC%d=%6.2f ℃", devNr, tempC);
  webSocket.sendTXT(wsClientID, cMsg);


} // printTemperature

// main function to print information about a device
//===========================================================================================
void printData(int8_t devNr)
{
  char devAddr[20];

  //getDevAddress(deviceAddress, devAddr);
  Debugf("Device Address: [%s] [%-30.30s] ", sensorArray[devNr].address, sensorArray[devNr].name);
  //printTemperature(sensorArray[devNr].address);
  printTemperature(devNr);
  Debugln();
  
} // printData()


//===========================================================================================
void setup() 
{
  Serial.begin(115200);
  Debugln("\nBooting ... \n");

  pinMode(LED_BUILTIN, OUTPUT);

  startWiFi();
  startTelnet();
   
  Debug("Gebruik 'telnet ");
  Debug(WiFi.localIP());
  Debugln("' voor verdere debugging");

  startMDNS(_HOSTNAME);
  MDNS.addService("arduino", "tcp", 81);
  MDNS.port(81);  // webSockets

#if defined(USE_NTP_TIME)                                   //USE_NTP
//================ startNTP =========================================
  if (!startNTP()) {                                        //USE_NTP
    DebugTln("ERROR!!! No NTP server reached!\r\n\r");      //USE_NTP
    delay(2000);                                            //USE_NTP
    ESP.restart();                                          //USE_NTP
    delay(3000);                                            //USE_NTP
  }                                                         //USE_NTP
#endif  //USE_NTP_TIME                                      //USE_NTP

  for (int I = 0; I < 10; I++) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }
//================ SPIFFS ===========================================
  if (!SPIFFS.begin()) {
    DebugTln("SPIFFS Mount failed\r");   // Serious problem with SPIFFS 
    SPIFFSmounted = false;
    
  } else { 
    DebugTln("SPIFFS Mount succesfull\r");
    SPIFFSmounted = true;
  }
//=============end SPIFFS =========================================

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  httpServer.on("/",           HTTP_GET, handleIndexPage);
  httpServer.on("/index.html", HTTP_GET, handleIndexPage);
  httpServer.serveStatic("/FSexplorer.png",   SPIFFS, "/FSexplorer.png");

  //httpServer.on("/restAPI", HTTP_GET, restAPI);
  //httpServer.on("/restapi", HTTP_GET, restAPI);
  httpServer.on("/ReBoot", HTTP_POST, handleReBoot);

#if defined (HAS_FSEXPLORER)
  httpServer.on("/FSexplorer", HTTP_POST, handleFileDelete);
  httpServer.on("/FSexplorer", handleRoot);
  httpServer.on("/FSexplorer/upload", HTTP_POST, []() {
    httpServer.send(200, "text/plain", "");
  }, handleFileUpload);
#else
  httpServer.on("/FSexplorer", HTTP_GET, handleIndexPage);
#endif
  httpServer.onNotFound([]() {
    if (httpServer.uri() == "/update") {
      httpServer.send(200, "text/html", "/update" );
    } else {
      DebugTf("onNotFound(%s)\r\n", httpServer.uri().c_str());
      if (httpServer.uri() == "/" || httpServer.uri() == "/index.html") {
        handleIndexPage();
      }
    }
    if (!handleFileRead(httpServer.uri())) {
      httpServer.send(404, "text/plain", "FileNotFound");
    }
  });

  httpServer.begin();
  DebugTln( "HTTP server gestart\r" );

  //--- Start up the library for the DS18B20 sensors
  sensors.begin();

  //--- locate devices on the bus
  noSensors = sensors.getDeviceCount();
  DebugTf("Locating devices...found %d devices\n", noSensors);

  //--- report parasite power requirements
  DebugT("Parasite power is: ");
  if (sensors.isParasitePowerMode()) 
        Debugln("ON");
  else  Debugln("OFF");

  // Search for devices on the bus and assign based on an index. Ideally,
  // you would do this to initially discover addresses on the bus and then
  // use those addresses and manually assign them (see above) once you know
  // the devices on your bus (and assuming they don't change).
  //
  // search() looks for the next device. Returns 1 if a new address has been
  // returned. A zero might mean that the bus is shorted, there are no devices,
  // or you have already retrieved all of them. It might be a good idea to
  // check the CRC to make sure you didn't get garbage. The order is
  // deterministic. You will always get the same devices in the same order
  //
  // Must be called before search()
  oneWire.reset_search();
  for(int sensorNr = 0; sensorNr < noSensors; sensorNr++) {
    oneWire.search(DS18B20);
    getDevAddress(DS18B20, cMsg);
    DebugTf("Device %2d Address: [%s] ..\n", sensorNr, cMsg);
    if (!readIniFile(sensorNr, cMsg)) {
      appendIniFile(sensorNr, cMsg);
    }
    // set the resolution to 9 bit per device
    sensors.setResolution(DS18B20, TEMPERATURE_PRECISION);
    DebugTf("Device %2d Resolution: %d\n", sensorNr, sensors.getResolution(DS18B20));
  } // for sensorNr ..

//  writeIniFile();
  Debugln("========================================================================================");
  sortSensors();
  printSensorArray();
  Debugln("========================================================================================");

  String DT = buildDateTimeString();
  DebugTf("Startup complete! @[%s]\r\n\n", DT.c_str());  

} // setup()


void loop() 
{
  httpServer.handleClient();
  webSocket.loop();
  MDNS.update();
  handleWebSockRefresh();

  if ((millis() - nextPollTimer) > 5000) {
    nextPollTimer = millis();

    // call sensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
    DebugT("Requesting temperatures...");
    sensors.requestTemperatures();
    Debugln("DONE");

    for(int sensorNr = 0; sensorNr < noSensors; sensorNr++) {
      printData(sensorNr);
    }
  } // pollTimer ..

#if defined(USE_NTP_TIME)                                                         //USE_NTP
  if (timeStatus() == timeNeedsSync || prevNtpHour != hour()) {                   //USE_NTP
    prevNtpHour = hour();                                                         //USE_NTP
    setSyncProvider(getNtpTime);                                                  //USE_NTP
    setSyncInterval(600);                                                         //USE_NTP
  }                                                                               //USE_NTP
#endif                                                                            //USE_NTP

} // loop()
