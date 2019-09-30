#include <ArduinoJson.h>

#define MAX_CACHE_AGE (uint32_t) 60

uint32_t _last_cache_refresh=0;

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

// global vars

DynamicJsonDocument _doc(4096);

void _cacheJSON()
{
  
  uint32_t _now=now();
  
  if((_now - _last_cache_refresh) < MAX_CACHE_AGE)
  {
    Debugf("No need to refresh cache, age is %lu (max %lu)\n", _now - _last_cache_refresh, MAX_CACHE_AGE);
    return;
  }

  Debugf("Populate the cache\n");

  _doc.clear();
  
  _last_cache_refresh=_now;
  
  JsonArray sensors = _doc.createNestedArray("sensors");

  // some extra fields and sensors for testing purposes
  
  _doc["time"] = "Now";
  
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
    
    so["counter"]     = s;
    
    sensors.add(so);
  }
}


int _find_sensorIndex_by_query() // api/describe_sensor?[name|sensorID]=<string>
{  
   _cacheJSON();

  JsonArray arr = _doc["sensors"];

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
        Debugf("Comparing %s with %s\n", obj[Key], Value);
        
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

  JsonObject obj = _doc.as<JsonObject>() ;
  
  _returnJSON(obj);
   
} 

void handleAPI_describe_sensor() // api/describe_sensor?[name|sensorID]=<string>
{  
  int si = _find_sensorIndex_by_query();
  
  Debugf("sensorIndex=%d\n", si);
  
  if (si < 0 )
    return;
  
  JsonObject toRet = _doc["sensors"][si];
  _returnJSON(toRet);
}

DynamicJsonDocument toRetDoc(50);

void handleAPI_get_temperature() // api/get_temperature?[name|sensorID]=<string>
{
  int si = _find_sensorIndex_by_query();
  
  Debugf("sensorIndex=%d\n", si);
  
  if (si < 0 )
    return;
  
  toRetDoc["temperature"] = _doc["sensors"][si]["temperature"];
  JsonObject toRet = toRetDoc.as<JsonObject>();
  _returnJSON(toRet);

  
}

// bogus handler for testing

void nothingAtAll()
{
  httpServer.send(200, "text/html",  "done nothing at all");

}

void apiInit()
{
  httpServer.on("/api",  nothingAtAll);
  httpServer.on("/api/list_sensors", handleAPI_list_sensors);
  httpServer.on("/api/describe_sensor", handleAPI_describe_sensor);
  httpServer.on("/api/get_temperature", handleAPI_get_temperature);
}
