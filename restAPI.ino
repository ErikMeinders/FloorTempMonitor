#include <ArduinoJson.h>
#include "FloorTempMonitor.h"

DECLARE_TIMER(cacheRefresh, 60)

#define CALIBRATE_HIGH 25.0

// global vars

static DynamicJsonDocument _cache(4096);   // the cache for sensors
static DynamicJsonDocument toRetDoc(1024);  // generic doc to return, clear() before use!

boolean cacheEmpty = true;

// some helper functions

void _returnJSON(JsonObject obj);
void _returnJSON(JsonObject obj)
{
  String toReturn;

  serializeJson(obj, toReturn);
  
  httpServer.send(200, "application/json", toReturn);
}

void _returnJSON400(const char * message)
{
  
  httpServer.send(400, "application/json", message);
}

void _cacheJSON()
{
  if(!DUE(cacheRefresh) && !cacheEmpty) 
  {
    DebugTf("No need to refresh JSON cache\n");
    return;
  }
  cacheEmpty = false;

  DebugTf("Time to populate the JSON cache\n");

  _cache.clear();
    
  JsonArray sensors = _cache.createNestedArray("sensors");
  
  for(int8_t s=0; s<noSensors; s++)
  {
    DynamicJsonDocument so(512);
        
    so["sensorIndex"] = sensorArray[s].sensorIndex;
    so["sensorID"]    = sensorArray[s].sensorID;
    so["name"]        = sensorArray[s].name;
    so["offset"]      = sensorArray[s].tempOffset;
    so["factor"]      = sensorArray[s].tempFactor;
    so["temperature"] = sensorArray[s].tempC;
    so["servonr"]     = sensorArray[s].servoNr;
    so["deltatemp"]   = sensorArray[s].deltaTemp;
    so["counter"]     = s;
    
    sensors.add(so);
  }
}

int _find_sensorIndex_in_query() // api/describe_sensor?[name|sensorID]=<string>
{  
 
  // describe_sensor takes name= or sensorID= as query parameters
  // e.g. api/describe_sensor?sensorID=0x232323232323
  // returns index in _SA 
  
  const char * Keys[]= {"name", "sensorID" };

  // loop over all possible query keys
  
  for ( const char * Key : Keys )
  {
    
    const char* Value=httpServer.arg(Key).c_str();
      
    // when query is ?name=kitchen
    // key = name and Value = kitchen
    // if query doesn't contain name=, Value is ""
    
    if ( strlen(Value) >0 )
    {
      // the Key was in the querystring with a non-zero Value
      
      // loop over the array entries to find object that contains Key/Value pair
      
      for(int8_t i=0 ; i < noSensorRecs ; i++)
      {
        Debugf("Comparing %s/%s with %s\n", _SA[i].name, _SA[i].sensorID, Value);
        
        if ( !strcmp(_SA[i].name,Value) || !strcmp(_SA[i].sensorID,Value))
        {
            // first record with matching K/V pair is returned
            //_returnJSON(obj);
            return i;
        }
      }

      // no object in array contains Key/Value pair, hence return 400

      _returnJSON400("valid key, but no record with requested value");
      return -1;
    }
  }

  // the query doesn't ask for a field we know (or support)

  _returnJSON400("invalid key");
  return -1;
}
  
// --- the handlers ---

void handleAPI_list_sensors() // api/list_sensors
{  
  _cacheJSON();

  JsonObject obj = _cache.as<JsonObject>() ;
  
  _returnJSON(obj);
   
} 

JsonArray calibrations;

void calibrate_low(int sensorIndex, float lowCalibratedTemp)
{

  DynamicJsonDocument c(64);
  

  float tempRaw = sensors.getTempCByIndex(_SA[sensorIndex].sensorIndex);

  // this should be lowTemp, change tempOffset
  // so that reading + offset equals lowTemp
  // that is, offset = lowTemp - raw reading

  _SA[sensorIndex].tempC = tempRaw;

  _SA[sensorIndex].tempOffset = lowCalibratedTemp - tempRaw;
  _SA[sensorIndex].tempFactor = 1.0;
  
  c["idx"] = sensorIndex;
  c["raw"] = tempRaw;
  c["off"] = _SA[sensorIndex].tempOffset;
  c["fct"] = _SA[sensorIndex].tempFactor;

  calibrations.add(c);
  
}

void calibrate_high(int sensorIndex, float hiCalibratedTemp)
{
  float tempRaw = sensors.getTempCByIndex(_SA[sensorIndex].sensorIndex);
  DynamicJsonDocument c(64);

  // calculate correction factor
  // so that 
  // raw_reading * correction_factor = actualTemp
  // --> correction_factor = actualTemp/raw_reading

  // r(read)=40, t(actual)=50 --> f=50/40=1.25
  // r=20  --> t=20.0*1.25=25.0

  // make sure to use the calibrated reading, 
  // so add the offset!
  
  _SA[sensorIndex].tempC = tempRaw;
  _SA[sensorIndex].tempFactor = hiCalibratedTemp  / ( tempRaw + _SA[sensorIndex].tempOffset );

  c["idx"] = sensorIndex;
  c["raw"] = tempRaw;
  c["off"] = _SA[sensorIndex].tempOffset;
  c["fct"] = _SA[sensorIndex].tempFactor;

  calibrations.add(c);
}

void calibrate_sensor(int sensorIndex, float realTemp)
{

  if ( realTemp < CALIBRATE_HIGH )
  {
    timeThis(calibrate_low(sensorIndex, realTemp));
  } else {
    timeThis(calibrate_high(sensorIndex, realTemp));
  }
}



void handleAPI_calibrate_sensor()
{
  //DynamicJsonDocument toRetCalDoc(512);

  DebugTf("starting handleAPI_calibrate_sensor()");
  toRetDoc.clear();
  timeThis(sensors.requestTemperatures());  // update sensors! 

  // a temperature should be given and optionally a sensor(name or ID)
  // when sensor is missing, all sensors are calibrated
  // /api/calibrate_sensor?temp=<float>[&[name|sensorID]=<string>]
  // *** actualTemp should come from a calibrated source
  
  const char* actualTempString=httpServer.arg("temp").c_str();
  
  // check correct use
  
  if (strlen(actualTempString) <= 0)
  {
     _returnJSON400("temp is mandatory query parameter in calibration function");
     return;
  }
  cacheEmpty = true;  // invalidate cache forcefully

  float actualTemp = atof(actualTempString);

  // prepare some JSON stuff to return
  
  calibrations = toRetDoc.createNestedArray("calibrations");
  
  // was a name or sensorID specified --> calibrate only that one sensor
  
  if (strlen(httpServer.arg("name").c_str()) +
      strlen(httpServer.arg("sensorID").c_str()) > 0)
  {
    int sensorToCalibrate = _find_sensorIndex_in_query();
    DebugTf("About to calibrate %s sensor with %f\n", _SA[sensorToCalibrate].name, actualTemp);

    calibrate_sensor(sensorToCalibrate, actualTemp);
      
  } else {

    // otherwise, calibrate all sensors
    DebugTf("About to calibrate all sensors with %f\n", actualTemp);
    for (int sensorToCalibrate=0 ; sensorToCalibrate < noSensors ; sensorToCalibrate++)
      calibrate_sensor(sensorToCalibrate, actualTemp);

  }
  sensorsWrite();

  _returnJSON(toRetDoc.as<JsonObject>());
}

void handleAPI_describe_sensor() // api/describe_sensor?[name|sensorID]=<string>
{  
  int si = _find_sensorIndex_in_query();
  
  Debugf("sensorIndex=%d\n", si);
  
  if (si < 0 )
    return;
  
  JsonObject toRet = _cache["sensors"][si];
  _returnJSON(toRet);
}


void handleAPI_get_temperature() // api/get_temperature?[name|sensorID]=<string>
{
  int si = _find_sensorIndex_in_query();
  //DynamicJsonDocument toRetDoc(64);

  toRetDoc.clear();

  Debugf("sensorIndex=%d\n", si);
  
  if (si < 0 )
    return;
  
  toRetDoc["temperature"] = _cache["sensors"][si]["temperature"];
  // JsonObject toRet = toRetDoc.as<JsonObject>();
  //_returnJSON(toRet);

  _returnJSON( toRetDoc.as<JsonObject>() );
}

void handleAPI_get_status()
{
  // DynamicJsonDocument toRetDoc(256);

  toRetDoc.clear();
  
  toRetDoc["uptime"] = upTime();
  toRetDoc["rssi"] = WiFi.RSSI();
  toRetDoc["disconnects"] = connectionMuxLostCount;
  toRetDoc["ESPrr"] = ESP.getResetReason();
  toRetDoc["ESPheap"] = ESP.getFreeHeap();
  toRetDoc["ESPhfrag"] = ESP.getHeapFragmentation();
  toRetDoc["ESPhmax"] = ESP.getMaxFreeBlockSize();

  toRetDoc["FWversion"] =  _FW_VERSION;
  toRetDoc["CompiledData"] = __DATE__;
  toRetDoc["CompiledTime"] = __TIME__;

  _returnJSON( toRetDoc.as<JsonObject>() );

}
// bogus handler for testing

void nothingAtAll()
{
  httpServer.send(200, "text/html",  "options are: /status, /sensor/{list,describe,temperature,calibrate}, /room/{list,temperature}");

}

void handleAPI_room_temperature()
{
  _returnJSON400("Not implemented yet");
}

void handleAPI_room_list()
{
  toRetDoc.clear();

  JsonArray rooms = toRetDoc.createNestedArray("rooms");

  int roomId = 0;

  for (room_t room : Rooms)
  {
    DynamicJsonDocument r(1024);
    byte i;
    
    r.clear();

    r["name"]   = room.Name;
    r["target"] = room.targetTemp;
    r["actual"] = room.actualTemp;

    JsonArray servos = r.createNestedArray("servos");

    for ( i=0 ; i < 2 ; i++)
    {
      if(room.Servos[i]<0)
        break;
      servos.add(room.Servos[i]);
    }
    r["servocount"] = i;
    r["id"] = roomId++;
    rooms.add(r);

  }

  _returnJSON(toRetDoc.as<JsonObject>());

}

void handleAPI_servo_status()
{
  const char* statusText;
  const char* Value=httpServer.arg("id").c_str();
  
  if(!strlen(Value))
    _returnJSON400("mandatory id missing");

  byte i = atoi(Value);

  toRetDoc.clear();

  toRetDoc["servo"] = i;
  toRetDoc["status"]  = servoArray[i].servoState;
  toRetDoc["reason"]  = servoArray[i].closeReason;

  switch(servoArray[i].servoState) {
    case SERVO_IS_OPEN:       
      statusText = "OPEN";
      break;
    case SERVO_IS_OPENING:       
      statusText = "OPENING";
      break;
    case SERVO_IS_CLOSING:  
      statusText = "CLOSING";
      break;
    case SERVO_IS_CLOSED:     
      statusText = "CLOSED";
      break;
  } 

  toRetDoc["message"] = statusText;

  _returnJSON(toRetDoc.as<JsonObject>());

}

void handleAPI_servo_statusarray()
{
  toRetDoc.clear();

  JsonArray servos = toRetDoc.createNestedArray("servos");

  for (byte i=0 ; i < 10; i++)
  {  
      servos.add(servoArray[i].servoState);
  }
  _returnJSON(toRetDoc.as<JsonObject>());
}

void handleAPI_servo_reasonarray()
{
  toRetDoc.clear();

  JsonArray servos = toRetDoc.createNestedArray("reason");

  for (byte i=0 ; i < 10; i++)
  {  
      servos.add(servoArray[i].closeReason);
  }
  _returnJSON(toRetDoc.as<JsonObject>());
}

void handleAPI_room_put()
{
  const char * room=httpServer.arg("room").c_str();
  const char * temp=httpServer.arg("temp").c_str();

  toRetDoc.clear();
  toRetDoc["room"] = room;

  DebugTf("PUT %s to %s\n", room, temp);

  for( int8_t  v=0 ; v < noRooms ; v++)
  {
    DebugTf("%s == %s ?\n", room, Rooms[v].Name);
    if(!strcmp(room, Rooms[v].Name))
    {
      DebugTf("Room found, set temp\n");
      Rooms[v].targetTemp = (10.0 * atof(temp))/10.0;
      toRetDoc["targetTemp"]= temp;
      DebugTf("now: %f\n", Rooms[v].targetTemp);
      break;
    }
  }
  _returnJSON(toRetDoc.as<JsonObject>());
  roomsWrite(); // write new target temp to SPIFF for persistence after reboot.
}

void apiInit()
{
  httpServer.on("/api",  nothingAtAll);

  httpServer.on("/api/status", handleAPI_get_status);

  httpServer.on("/api/sensor/list", handleAPI_list_sensors);
  httpServer.on("/api/sensor/status", handleAPI_list_sensors);

  httpServer.on("/api/sensor/describe", handleAPI_describe_sensor);
  httpServer.on("/api/sensor/temperature", handleAPI_get_temperature);
  httpServer.on("/api/sensor/calibrate", handleAPI_calibrate_sensor);

  httpServer.on("/api/room", HTTP_PUT, handleAPI_room_put);

  httpServer.on("/api/room/list", handleAPI_room_list);
  httpServer.on("/api/room/temperature", handleAPI_room_temperature);

  httpServer.on("/api/servo/status", handleAPI_servo_status);
  httpServer.on("/api/servo/statusarray", handleAPI_servo_statusarray);
  httpServer.on("/api/servo/reasonarray", handleAPI_servo_reasonarray);

}
