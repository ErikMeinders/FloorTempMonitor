/*
**  Program   : ESP8266_basic
*/
#define _FW_VERSION "v0.6.3 (30-09-2019)"
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
// #define TESTDATA
/******************** don't change anything below this comment **********************/

#include <Timezone.h>           // https://github.com/JChristensen/Timezone
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include "Debug.h"
#include "networkStuff.h"
#include <FS.h>                 // part of ESP8266 Core https://github.com/esp8266/Arduino
#include <OneWire.h>            // https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>  // https://github.com/milesburton/Arduino-Temperature-Control-Library

#define _S                    sensorArray
#define _PULSE_TIME           (uint32_t)sensorArray[0].deltaTemp
#define LED_ON                LOW
#define LED_OFF               HIGH

#define ONE_WIRE_BUS          0     // Data Wire is plugged into GPIO-00
#define TEMPERATURE_PRECISION 12
#define _MAX_SENSORS          18    // 16 Servo's/Relais + heater in & out
#define _MAX_NAME_LEN         12
#define _FIX_SETTINGSREC_LEN  85
#define _MAX_DATAPOINTS       100   // 24 hours every 15 minites - more will crash the gui
#define _LED_OFF_TIME         500   // milliseconds
#define _REFLOW_TIME         (5*60000) // 5 minutes
#define _POLL_INTERVAL        10000 // in milli-seconds - every 10 seconds
#define _PLOT_INTERVAL        60    // in seconds - 60 = 1min, 600 = 10min, 900 = 15min
#define _DELTATEMP_CHECK      1     // when to check the deltaTemp's in minutes
#define _HOUR_CLOSE_ALL       3     // @03:00 start closing servos one-by-one. Don't use 1:00!
#define _MIN                  60000 // milliSecs in a minute

/*********************************************************************************
* Uitgangspunten:
* 
* S0 - Sensor op position '0' is verwarmingsketel water UIT (Flux In)
*      deze sensor moet een servoNr '-1' hebben -> want geen klep.
*      De 'deltaTemp' van deze sensor wordt gebruikt om een eenmaal
*      gesloten Servo/Klep dicht te houden (als tijd dus!).
* S1 - Sensor op position '1' is (Flux) retour naar verwarmingsketel
*      servoNr '-1' -> want geen klep
* 
* Cyclus voor bijv S3:
*                                                start _REFLOW_TIME
*   (S0.tempC - S3.tempC) < S3.deltaTemp         ^        einde _REFLOW_TIME
*                                      v         |        v
*  - servo/Klep open            -------+         +--------+------> wacht tot (S0.tempC - S3.tempC) < S3.deltaTemp 
*                                      |         |
*  - Servo/Klep closed                 +---------+
*                                      ^         ^
*                                      |         |
*   start S0.deltaTemp (_PULSE_TIME) --/         |                       
*       S0.deltaTemp (_PULSE_TIME) verstreken ---/
* 
*********************************************************************************/

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

enum    { SERVO_IS_OPEN, SERVO_IS_CLOSED, SERVO_IN_LOOP, SERVO_COUNT0_CLOSE, ERROR };

typedef struct {
  int8_t    index;
  char      sensorID[20];
  uint8_t   position;
  char      name[_MAX_NAME_LEN];
  float     tempOffset;
  float     tempFactor;
  int8_t    servoNr;
  float     deltaTemp;      //-- in S0 -> closeTime
  uint8_t   closeCount;
  float     tempC;          //-- not in sensors.ini
  uint8_t   servoState;     //-- not in sensors.ini
  uint32_t  servoTimer;     //-- not in sensors.ini
} sensorStruct;

typedef struct {
  uint32_t  timestamp;
  float     tempC[_MAX_SENSORS];
} dataStruct;

sensorStruct  sensorArray[_MAX_SENSORS];
dataStruct    dataStore[_MAX_DATAPOINTS+1];

char      cMsg[150], fChar[10];
String    pTimestamp;
int8_t    prevNtpHour     = 0;
uint64_t  upTimeSeconds;
uint32_t  nextPollTimer;
uint32_t  ledOffTimer;
int8_t    noSensors;
int8_t    cycleNr         = 0;
uint8_t   wsClientID;
uint32_t  lastPlotTime    = 0;
int8_t    lastSaveHour    = 0;
bool      connectToSX1509 = false;
bool      SPIFFSmounted   = false;
bool      readRaw         = false;
bool      cycleAllSensors = false;



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

  apiInit();
  
  httpServer.serveStatic("/",                 SPIFFS, "/index.html");
  httpServer.serveStatic("/index.html",       SPIFFS, "/index.html");
  httpServer.serveStatic("/floortempmon.js",  SPIFFS, "/floortempmon.js");
  httpServer.serveStatic("/sensorEdit.html",  SPIFFS, "/sensorEdit.html");
  httpServer.serveStatic("/sensorEdit.js",    SPIFFS, "/sensorEdit.js");
  httpServer.serveStatic("/FSexplorer.png",   SPIFFS, "/FSexplorer.png");
  httpServer.serveStatic("/favicon.ico",      SPIFFS, "/favicon.ico");
  httpServer.serveStatic("/favicon-32x32.png",SPIFFS, "/favicon-32x32.png");
  httpServer.serveStatic("/favicon-16x16.png",SPIFFS, "/favicon-16x16.png");
  
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
#ifdef TESTDATA                                         // TESTDATA
  noSensors = 9;                                        // TESTDATA
#endif                                                  // TESTDATA
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

#ifdef TESTDATA                                               // TESTDATA
  for (int s=0; s < noSensors; s++) {                         // TESTDATA
    sprintf(_S[s].name, "Sensor %d", (s+1));                  // TESTDATA
    sprintf(_S[s].sensorID, "0x2800000000%02x", s*3);         // TESTDATA
    _S[s].servoNr     =  s;                                   // TESTDATA
    _S[s].position    = s+2;                                  // TESTDATA
    _S[s].closeCount  =  0;                                   // TESTDATA
    _S[s].deltaTemp   = 15 + s;                               // TESTDATA
    _S[s].servoTimer  = millis();                             // TESTDATA
  }                                                           // TESTDATA
  sprintf(_S[5].name, "*Flux IN");                            // TESTDATA
  _S[5].position      =  0;                                   // TESTDATA
  _S[5].deltaTemp     =  2;                                   // TESTDATA
  _S[5].servoNr       = -1;                                   // TESTDATA
  sprintf(_S[6].name, "*Flux RETOUR");                        // TESTDATA
  _S[6].position      =  1;                                   // TESTDATA
  _S[6].deltaTemp     =  0;                                   // TESTDATA
  _S[6].servoNr       = -1;                                   // TESTDATA
  sprintf(_S[7].name, "*Room Temp.");                         // TESTDATA
  _S[7].servoNr       = -1;                                   // TESTDATA
#endif                                                        // TESTDATA

  Debugln("========================================================================================");
  sortSensors();
  printSensorArray();
  Debugln("========================================================================================");

  readDataPoints();

  setupSX1509();

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
  checkDeltaTemps();
  handleCycleServos();

  if ((millis() - nextPollTimer) > _POLL_INTERVAL) {
    nextPollTimer = millis();
    digitalWrite(BUILTIN_LED, LED_ON);
    ledOffTimer   = millis();

    // call sensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
    DebugT("Requesting temperatures...");
    sensors.requestTemperatures();
    Debugln("DONE");
    handleSensors();
    
  } // pollTimer ..
  
  if ((millis() - ledOffTimer) > _LED_OFF_TIME) {
    digitalWrite(BUILTIN_LED, LED_OFF);
  }
  
#if defined(USE_NTP_TIME)                                                         //USE_NTP
  if (timeStatus() == timeNeedsSync || prevNtpHour != hour()) {                   //USE_NTP
    prevNtpHour = hour();                                                         //USE_NTP
    synchronizeNTP();                                                             //USE_NTP
  }                                                                               //USE_NTP
#endif                                                                            //USE_NTP

} // loop()
