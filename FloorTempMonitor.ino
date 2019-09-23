/*
**  Program   : ESP8266_basic
*/
#define _FW_VERSION "v0.5.0 (23-09-2019)"
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
    - Buildin Led: "2"  //  "2" for Wemos and ESP-12
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

#include <Timezone.h>           // https://github.com/JChristensen/Timezone
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include "Debug.h"
#include "networkStuff.h"
#include <FS.h>                 // part of ESP8266 Core https://github.com/esp8266/Arduino
#include <OneWire.h>            // https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>  // https://github.com/milesburton/Arduino-Temperature-Control-Library

#define ONE_WIRE_BUS          0     // Data wire is plugged into GPIO-00
#define TEMPERATURE_PRECISION 12
#define _MAX_SENSORS          20
#define _MAX_NAME_LEN         12
#define _FIX_SETTINGSREC_LEN  85
#define _MAX_DATAPOINTS       100   // 24 hours every 15 minites - more will crash the gui
#define _POLL_INTERVAL        10000 // in milli-seconds - every 10 seconds
#define _PLOT_INTERVAL        900   // in seconds - 600 = 10min, 900 = 15min

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
DeviceAddress DS18B20;

// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};   // Central European Summer Time
TimeChangeRule CET  = {"CET ", Last, Sun, Oct, 3, 60};    // Central European Standard Time
Timezone CE(CEST, CET);
TimeChangeRule *tcr;         // pointer to the time change rule, use to get TZ abbrev

const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

typedef struct {
  int8_t    index;
  char      sensorID[20];
  uint8_t   position;
  char      name[_MAX_NAME_LEN];
  float     tempOffset;
  float     tempFactor;
  int8_t    servoNr;
  float     deltaTemp;
  uint16_t  loopTime;
  float     tempC;
} sensorStruct;

typedef struct {
  uint32_t  timestamp;
  float     tempC[_MAX_SENSORS];
} dataStruct;

sensorStruct  sensorArray[_MAX_SENSORS];
dataStruct    dataStore[_MAX_DATAPOINTS];

bool      SPIFFSmounted = false;
char      cMsg[150], fChar[10];
String    pTimestamp;
int8_t    prevNtpHour   = 0;
uint64_t  upTimeSeconds;
uint32_t  nextPollTimer;
int8_t    noSensors;
bool      readRaw = false;
uint8_t   wsClientID;
uint32_t  lastPlotTime  = 0;
int8_t    lastSaveHour  = 0;



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
void setup()
{
  Serial.begin(115200);
  Debugln("\nBooting ... \n");
  Debugf("[%s] %s  compiled [%s %s]\n", _HOSTNAME, _FW_VERSION, __DATE__, __TIME__);

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

  httpServer.begin(); // before .ons

  // httpServer.on("/",           handleIndexPage);
  // httpServer.on("/index.html", handleIndexPage);

  httpServer.on("/api",  nothingAtAll);
  httpServer.on("/api/list_sensors", handleAPI_list_sensors);
  httpServer.on("/api/describe_sensor", handleAPI_describe_sensor);
 
  
  httpServer.serveStatic("/FSexplorer.png",   SPIFFS, "/FSexplorer.png");
  httpServer.serveStatic("/",                 SPIFFS, "/index.html");
  httpServer.serveStatic("/index.html",       SPIFFS, "/index.html");
  httpServer.serveStatic("/floortempmon.js",  SPIFFS, "/floortempmon.js");
  httpServer.serveStatic("/sensorEdit.html",  SPIFFS, "/sensorEdit.html");
  httpServer.serveStatic("/sensorEdit.js",    SPIFFS, "/sensorEdit.js");
  
  httpServer.on("/ReBoot", handleReBoot);

#if defined (HAS_FSEXPLORER)
  httpServer.on("/FSexplorer", HTTP_POST, handleFileDelete);
  httpServer.on("/FSexplorer", handleRoot);
  httpServer.on("/FSexplorer/upload", HTTP_POST, []() {
    httpServer.send(200, "text/plain", "");
  }, handleFileUpload);
#endif
  httpServer.onNotFound([]() {
    if (httpServer.uri() == "/update") {
      httpServer.send(200, "text/html", "/update" );
    } else {
      DebugTf("onNotFound(%s)\r\n", httpServer.uri().c_str());
    }
    if (!handleFileRead(httpServer.uri())) {
      httpServer.send(404, "text/plain", "FileNotFound");
    }
  });

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
    getSensorID(DS18B20, cMsg);
    DebugTf("Device %2d sensorID: [%s] ..\n", sensorNr, cMsg);
    if (!readIniFile(sensorNr, cMsg)) {
      appendIniFile(sensorNr, cMsg);
    }
    // set the resolution to 9 bit per device
    sensors.setResolution(DS18B20, TEMPERATURE_PRECISION);
    DebugTf("Device %2d Resolution: %d\n", sensorNr, sensors.getResolution(DS18B20));
  } // for sensorNr ..

  Debugln("========================================================================================");
  sortSensors();
  printSensorArray();
  Debugln("========================================================================================");

  readDataPoints();

  lastPlotTime = now() / _PLOT_INTERVAL;

  String DT = buildDateTimeString();
  DebugTf("Startup complete! @[%s]\r\n\n", DT.c_str());

} // setup()


void loop()
{
  httpServer.handleClient();
  webSocket.loop();
  MDNS.update();
  handleWebSockRefresh();

  if ((millis() - nextPollTimer) > _POLL_INTERVAL) {
    nextPollTimer = millis();
    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));

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
    synchronizeNTP();                                                             //USE_NTP
  }                                                                               //USE_NTP
#endif                                                                            //USE_NTP

} // loop()
