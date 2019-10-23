/*
**  Program   : FloorTempMonitor
*/
#define _FW_VERSION "v0.6.9 (24-10-2019)"
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

#define PROFILING             // comment this line out if you want not profiling 
#define PROFILING_THRESHOLD 25 // defaults to 3ms - don't show any output when duration below TH

//  #define TESTDATA
/******************** don't change anything below this comment **********************/

#include <Timezone.h>           // https://github.com/JChristensen/Timezone
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include "Debug.h"
#include "timing.h"
#include "networkStuff.h"
#include <FS.h>                 // part of ESP8266 Core https://github.com/esp8266/Arduino
#include <OneWire.h>            // https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>  // https://github.com/milesburton/Arduino-Temperature-Control-Library

#define _SA                   sensorArray
#define _PULSE_TIME           (uint32_t)sensorArray[0].deltaTemp
#define HEATER_ON_TEMP        40.0
#define LED_BUILTIN_ON        LOW
#define LED_BUILTIN_OFF       HIGH
#define LED_WHITE             14      // D5
#define LED_RED               12      // D6
#define LED_GREEN             13      // D7 - ONLY USE FOR HEARTBEAT!!
#define LED_ON                HIGH
#define LED_OFF               LOW

#define ONE_WIRE_BUS          0     // Data Wire is plugged into GPIO-00
#define _MAX_SENSORS          18    // 16 Servo's/Relais + heater in & out
#define _MAX_NAME_LEN         12
#define _FIX_SETTINGSREC_LEN  85
#define _MAX_DATAPOINTS       100   // 24 hours every 15 minutes - more will crash the gui
#define _LAST_DATAPOINT      (_MAX_DATAPOINTS -1)
#define _REFLOW_TIME         (5*60000) // 5 minutes
#define _DELTATEMP_CHECK      2     // when to check the deltaTemp's in minutes
#define _HOUR_CLOSE_ALL       3     // @03:00 start closing servos one-by-one. Don't use 1:00!
#define _MIN                  60000 // milliSecs in a minute

#define _PLOT_INTERVAL        300    // in seconds

DECLARE_TIMER(graphUpdate, _PLOT_INTERVAL)

DECLARE_TIMER(pingCheck,    30) // check connection every 30 seconds 
DECLARE_TIMER(heartBeat,     3) // flash LED_GREEN 

DECLARE_TIMER(sensorPoll,   20) // update sensors every 20s 

DECLARE_TIMER(screenClockRefresh, 10)

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
  int8_t    servoStateV[_MAX_SENSORS];
} dataStruct;

sensorStruct  sensorArray[_MAX_SENSORS];
dataStruct    dataStore[_MAX_DATAPOINTS+1];

char      cMsg[150];
String    pTimestamp;
int8_t    prevNtpHour         = 0;
uint32_t  startTimeNow;

int8_t    noSensors;
int8_t    cycleNr             = 0;
uint8_t   wsClientID;
int8_t    lastSaveHour        = 0;
bool      connectedToMux      = false;
bool      SPIFFSmounted       = false;
bool      readRaw             = false;
bool      cycleAllSensors     = false;
bool      onlyUpdateLastPoint = false;
bool      onlyUpdateSensors   = false;
bool      onlyUpdateServos    = false;



//===========================================================================================
String upTime()
{
  char    calcUptime[20];
  uint32_t  upTimeNow = now() - startTimeNow; // upTimeNow = seconds

  sprintf(calcUptime, "%d[d] %02d:%02d" 
          , (int)((upTimeNow / (60 * 60 * 24)) % 365)
          , (int)((upTimeNow / (60 * 60)) % 24) // hours
          , (int)((upTimeNow / 60) % 60));       // minuten
  return calcUptime;

} // upTime()


//===========================================================================================
void handleHeartBeat()
{
  if ( DUE (heartBeat) ) {
    digitalWrite(LED_GREEN, LED_ON); 
  } 
  else
  if ((millis() - heartBeat_last) > 1000) {
    digitalWrite(LED_GREEN, LED_OFF);
  }
} //  handleHeartBeat()

//===========================================================================================
void setup()
{
  Serial.begin(115200);
  Debugln("\nBooting ... \n");
  Debugf("[%s] %s  compiled [%s %s]\n", _HOSTNAME, _FW_VERSION, __DATE__, __TIME__);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LED_ON);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LED_ON);
  pinMode(LED_WHITE, OUTPUT);
  digitalWrite(LED_WHITE, LED_ON);

  startWiFi();
  digitalWrite(LED_WHITE, LED_OFF);
  startTelnet();

  Debug("Gebruik 'telnet ");
  Debug(WiFi.localIP());
  Debugln("' voor verdere debugging");
  digitalWrite(LED_RED, LED_OFF);

  startMDNS(_HOSTNAME);
  MDNS.addService("arduino", "tcp", 81);
  MDNS.port(81);  // webSockets

  ntpInit();
  startTimeNow = now();

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
  digitalWrite(LED_GREEN, LED_OFF);
  
  sprintf(cMsg, "Last reset reason: [%s]\r", ESP.getResetReason().c_str());
  DebugTln(cMsg);

  //--- Start up the library for the DS18B20 sensors
  sensors.begin();

  delay(5000); // time to start telnet
  setupDallasSensors();

#ifdef TESTDATA                                       // TESTDATA
  noSensors = 11;                                              // TESTDATA
  for (int s=0; s < noSensors; s++) {                          // TESTDATA
    sprintf(_SA[s].name, "Sensor %d", (s+2));                  // TESTDATA
    sprintf(_SA[s].sensorID, "0x2800ab0000%02x", (s*3) + 3);   // TESTDATA
    _SA[s].servoNr     =  (s+1);                               // TESTDATA
    _SA[s].position    = s+2;                                  // TESTDATA
    _SA[s].closeCount  =  0;                                   // TESTDATA
    _SA[s].deltaTemp   = 15 + s;                               // TESTDATA
    _SA[s].tempFactor  = 1.0;                                  // TESTDATA
    _SA[s].servoTimer  = millis();                             // TESTDATA
  }                                                            // TESTDATA
  sprintf(_SA[5].name, "*Flux IN");                            // TESTDATA
  _SA[5].position      =  0;                                   // TESTDATA
  _SA[5].deltaTemp     =  2; // is _PULSE_TIME                 // TESTDATA
  _SA[5].servoNr       = -1;                                   // TESTDATA
  sprintf(_SA[6].name, "*Flux RETOUR");                        // TESTDATA
  _SA[6].position      =  1;                                   // TESTDATA
  _SA[6].deltaTemp     =  0;                                   // TESTDATA
  _SA[6].servoNr       = -1;                                   // TESTDATA
  sprintf(_SA[7].name, "*Room Temp.");                         // TESTDATA
  _SA[7].servoNr       = -1;                                   // TESTDATA
#endif                                                // TESTDATA

  Debugln("========================================================================================");
  sortSensors();
  printSensorArray();
  Debugln("========================================================================================");

  readDataPoints();

  connectedToMux = setupI2C_Mux();

  String DT = buildDateTimeString();
  DebugTf("Startup complete! @[%s]\r\n\n", DT.c_str());

} // setup()


//===========================================================================================
void loop()
{
  handleHeartBeat();                  // blink GREEN led
  
  timeThis( webSocket.loop() );
  timeThis( MDNS.update() );
  timeThis( httpServer.handleClient() );
  timeThis( handleNTP() );
  timeThis( checkI2C_Mux() );         // maybe call setupI2C_Mux() in the process (if needed) ..
  timeThis( handleSensors() );        // update upper part of screen
  timeThis( checkI2C_Mux() );         // handle Sensors may take a long time ..
  timeThis( handleDatapoints() );     // update graph in lower screen half
  
  timeThis( handleWebSockRefresh() ); // essentially update of clock on screen
  
  timeThis( checkDeltaTemps() );
  timeThis( handleCycleServos() );

  timeThis( handlePing() );

} 
