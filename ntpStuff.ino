/*
***************************************************************************
**  Program  : ntpStuff, part of FloorTempMonitor
**  Version  : v0.5.0
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/
#if defined(USE_NTP_TIME)

#include <WiFiUdp.h>            //--- part of ESP8266 Core https://github.com/esp8266/Arduino
WiFiUDP           Udp;

int               timeZone  = 0;      // UTC
unsigned int      localPort = 8888;   // local port to listen for UDP packets

// NTP Servers:
static const char ntpPool[][30] = { "time.google.com",
                                    "nl.pool.ntp.org",
                                    "0.nl.pool.ntp.org",
                                    "1.nl.pool.ntp.org",
                                    "3.nl.pool.ntp.org"
                                  };
static unsigned int        ntpPoolIndx = 0;

char              ntpServerName[50];

const int         NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte              packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

//static bool       externalNtpTime = false;
static IPAddress  ntpServerIP; // NTP server's ip address


//=======================================================================
bool startNTP()
{
  DebugTln("Starting UDP");
  Udp.begin(localPort);
  DebugT("Local port: ");
  Debugln(String(Udp.localPort()));
  DebugTln("waiting for NTP sync");
  synchronizeNTP();
  if (timeStatus() == timeSet) {    // time is set,
    return true;                    // exit with time set
  }
  return false;

} // startNTP()

//=======================================================================
void synchronizeNTP()
{
  timeZone = 0;
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);
  time_t utc = now();
  DebugTf("[%02d:%02d:%02d] (UTC) ->timeZone[%d]\n", hour(utc), minute(utc), second(utc), timeZone);
  timeZone = hour();
  time_t local = CE.toLocal(utc, &tcr);
  setTime(local);
  timeZone = hour() - timeZone;
  DebugTf("[%02d:%02d:%02d] (Central European) ->timeZone[%d]\n", hour(), minute(), second(), timeZone);

} // synchronizeNTP()


//=======================================================================
time_t getNtpTime()
{
  while(true) {
    yield();
    ntpPoolIndx++;
    if ( ntpPoolIndx > (sizeof(ntpPool) / sizeof(ntpPool[0]) -1) ) {
      ntpPoolIndx = 0;
    }
    sprintf(ntpServerName, "%s", String(ntpPool[ntpPoolIndx]).c_str());

    while (Udp.parsePacket() > 0) ; // discard any previously received packets
    Serial.printf("%41.41s Transmit NTP Request\n", " ");
    TelnetStream.printf("%41.41s Transmit NTP Request\n", " ");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.printf("%41.41s ", " ");
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    Serial.flush();
    TelnetStream.printf("%41.41s ", " ");
    TelnetStream.print(ntpServerName);
    TelnetStream.print(": ");
    TelnetStream.println(ntpServerIP);
    TelnetStream.flush();
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
      int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        //time_t t = (secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);

        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    }
    Serial.printf("%41.41s No NTP Response :-(\n", " ");
    TelnetStream.printf("%41.41s No NTP Response :-(\n", " ");

  } // while true ..

  return 0; // return 0 if unable to get the time

} // getNtpTime()


// send an NTP request to the time server at the given address
//=======================================================================
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();

} // sendNTPpacket()


//===========================================================================================
String buildDateTimeString()
{
  sprintf(cMsg, "%02d-%02d-%04d  %02d:%02d:%02d", day(), month(), year()
          , hour(), minute(), second());
  return cMsg;

} // buildDateTimeString()

void handleNTP()
{
  if (timeStatus() == timeNeedsSync || prevNtpHour != hour()) {
    prevNtpHour = hour();
    synchronizeNTP();
  }                     
}

void ntpInit()
{
   if (!startNTP()) {                                        //USE_NTP
    DebugTln("ERROR!!! No NTP server reached!\r\n\r");      //USE_NTP
    DebugFlush();                                           //USE_NTP
    delay(2000);                                            //USE_NTP
    ESP.restart();                                          //USE_NTP
    delay(3000);                                            //USE_NTP
  }                                                         //USE_NTP
  prevNtpHour = hour();
}
# else

// NO NTP!

void handleNTP()
{
  return;
}

void ntpInit()
{
  return;
  
}
#endif
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
