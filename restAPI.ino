#include <ArduinoJson.h>

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

bool cached=false;
DynamicJsonDocument _doc(4096);

void _cacheJSON()
{
  // might want to timeout chached data, so far just cache once
  
  if(cached)
    return;

  cached=true;
  
  JsonArray sensors = _doc.createNestedArray("sensors");

  // some extra fields and sensors for testing purposes
  
  _doc["time"] = "Now";
  
  for(int8_t si=0; si<3; si++)
  {
    DynamicJsonDocument so(512);
    
    int s = 0;
    
    so["index"]       = _S[s].index;
    so["position"]    = _S[s].position;
    so["sensorID"]    = _S[s].sensorID;
    so["name"]        = _S[s].name;
    so["offset"]      = _S[s].tempOffset;
    so["factor"]      = _S[s].tempFactor;
    
    so["erix"]        = "OK";
    so["counter"]     = si;
    
    sensors.add(so);
  }
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
            _returnJSON(obj);
            return;
        }
      }

      // no object in array contains Key/Value pair, hence return 400

      _returnJSON400("valid key, but no record with requested value");
      return ;
    }
  }

  // the query doesn't ask for a field we know (or support)

  _returnJSON400("invalid key");
  return ;
}

void handleAPI_get_temperature() // api/get_temperature?[name|sensorID]=<string>
{

  
}

// bogus handler for testing

void nothingAtAll()
{
  httpServer.send(200, "text/html",  "done nothing at all");

}

// earlier version without ArduinoJSON lib

void handleAPI_list_devices_old() 
{
  String DOC;
  char   json[300];
  
  DOC="{\r\n\t\"sensors\" : [ ";
  
  for(int8_t s=0; s<noSensors; s++) {
    sprintf(json,"{\t\"index\"    : \"%d\",\r\n\
                  \t\"position\" : \"%2d\",\r\n\
                  \t\"sensorID\"  : \"%s\",\r\n\
                  \t\"name\"     : \"%s\",\r\n\
                  \t\"offset\"   : \"%7.6f\",\r\n\
                  \t\"factor\"   : \"%7.6f\"\r\n\
                  }\r\n"
                      , _S[s].index
                      , _S[s].position
                      , _S[s].sensorID
                      , _S[s].name
                      , _S[s].tempOffset
                      , _S[s].tempFactor);
    DOC += json;
    
    if (s<noSensors-1)
      DOC += ",\r\n";
    
  }

  DOC += "\t]\r\n}\r\n";
  
  httpServer.send(200, "application/json", DOC);
  
} // handleAPI_list_devices

  
