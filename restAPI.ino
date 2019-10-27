#include <ArduinoJson.h>

DECLARE_TIMER(cacheRefresh, 15)

#define CALIBRATE_HIGH 25

// global vars

static DynamicJsonDocument _cache(4096); // the cache

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
  if(!DUE(cacheRefresh)) 
  {
    DebugTf("No need to refresh JSON cache\n");
    return;
  }

  DebugTf("Time to populate the JSON cache\n");

  _cache.clear();
    
  JsonArray sensors = _cache.createNestedArray("sensors");
  
  for(int8_t s=0; s<noSensors; s++)
  {
    DynamicJsonDocument so(512);
        
    so["index"]       = sensorArray[s].index;
    so["position"]    = sensorArray[s].position;
    so["sensorID"]    = sensorArray[s].sensorID;
    so["name"]        = sensorArray[s].name;
    so["offset"]      = sensorArray[s].tempOffset;
    so["factor"]      = sensorArray[s].tempFactor;
    so["temperature"] = sensorArray[s].tempC;
    so["servonr"]     = sensorArray[s].servoNr;
    so["servostate"]  = sensorArray[s].servoState;
    so["deltatemp"]   = sensorArray[s].deltaTemp;
    so["counter"]     = s;
    
    sensors.add(so);
  }
}

int _find_sensorIndex_in_query() // api/describe_sensor?[name|sensorID]=<string>
{  
   _cacheJSON(); // first update cache - if necessary

  JsonArray arr = _cache["sensors"];

  // describe_sensor takes name= or sensorID= as query parameters
  // e.g. api/describe_sensor?sensorID=0x232323232323
  // returns the JSON struct of the requested sensor
  
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
      
      for(JsonObject obj : arr)
      {
        Debugf("Comparing %s with %s\n", obj[Key].as<char*>(), Value);
        
        if ( !strcmp(obj[Key],Value))
        {
            // first record with matching K/V pair is returned
            //_returnJSON(obj);
            return(obj["counter"].as<int>());
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
  
  float tempRaw = sensors.getTempCByIndex(_SA[sensorIndex].index);

  // this should be lowTemp, change tempOffset
  // so that reading + offset equals lowTemp
  // that is, offset = lowTemp - raw reading 

  _SA[sensorIndex].tempOffset = lowCalibratedTemp - tempRaw;
  _SA[sensorIndex].tempFactor = 1.0;
  
  c["index"]  = sensorIndex;
  c["offset"] = _SA[sensorIndex].tempOffset;

  calibrations.add(c);
  
  updateIniRec(_SA[sensorIndex]);

}

void calibrate_high(int sensorIndex, float hiCalibratedTemp)
{
  float tempRaw = sensors.getTempCByIndex(_SA[sensorIndex].index);
  DynamicJsonDocument c(64);

  // calculate correction factor
  // so that 
  // raw_reading * correction_factor = actualTemp
  // --> correction_factor = actualTemp/raw_reading

  // r(read)=40, t(actual)=50 --> f=50/40=1.25
  // r=20  --> t=20.0*1.25=25.0

  // make sure to use the calibrated reading, 
  // so add the offset!
  
  _SA[sensorIndex].tempFactor = hiCalibratedTemp  / ( tempRaw + _SA[sensorIndex].tempOffset );

  c["index"]  = sensorIndex;
  c["factor"] = _SA[sensorIndex].tempFactor;

  calibrations.add(c);
  updateIniRec(_SA[sensorIndex]);
}

void calibrate_sensor(int sensorIndex, float realTemp)
{
  if ( realTemp < CALIBRATE_HIGH )
    calibrate_low(sensorIndex, realTemp);
  else
    calibrate_high(sensorIndex, realTemp);
}



void handleAPI_calibrate_sensor()
{
  DynamicJsonDocument toRetCalDoc(512);

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
  float actualTemp = atof(actualTempString);
  
  // prepare some JSON stuff to return
  
  toRetCalDoc.clear();
  calibrations = toRetCalDoc.createNestedArray("calibrations");
  
  // was a name or sensorID specified --> calibrate only that one sensor
  
  if (strlen(httpServer.arg("name").c_str()) +
      strlen(httpServer.arg("sensorID").c_str()) > 0)
  {
    int sensorToCalibrate = _find_sensorIndex_in_query();

    calibrate_sensor(sensorToCalibrate, actualTemp);
      
  } else {

    // otherwise, calibrate all sensors
    
    for (int sensorToCalibrate=0 ; sensorToCalibrate < noSensors ; sensorToCalibrate++)
      calibrate_sensor(sensorToCalibrate, actualTemp);

  }
  
  _returnJSON(toRetCalDoc.as<JsonObject>());
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
  DynamicJsonDocument toRetDoc(50);

  Debugf("sensorIndex=%d\n", si);
  
  if (si < 0 )
    return;
  
  toRetDoc["temperature"] = _cache["sensors"][si]["temperature"];
  // JsonObject toRet = toRetDoc.as<JsonObject>();
  //_returnJSON(toRet);

  _returnJSON( toRetDoc.as<JsonObject>() );
}

// bogus handler for testing

void nothingAtAll()
{
  httpServer.send(200, "text/html",  "options are: list_sensors, describe_sensor?name=str, get_temperature?name=str, calibrate_sensors?temp=ff.fff[&name=str]");

}

void apiInit()
{
  httpServer.on("/api",  nothingAtAll);
  httpServer.on("/api/list_sensors", handleAPI_list_sensors);
  httpServer.on("/api/describe_sensor", handleAPI_describe_sensor);
  httpServer.on("/api/get_temperature", handleAPI_get_temperature);
  httpServer.on("/api/calibrate_sensors", handleAPI_calibrate_sensor);
}
