#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#define MAXROOMS                8
#define MAX_UFHLOOPS_PER_ROOM   2
#define MAX_NAMELEN             20

// const char *R="{\"status\": \"ok\", \"version\": \"3.403\", \"request\": {\"route\": \"/telist\" }, \"response\": [{\"id\":0,\"name\":\"Keuken\",\"code\":\"11391347\",\"model\":1,\"lowBattery\":\"no\",\"version\":2.32,\"te\":21.4,\"hu\":39,\"te+\":22.1,\"te+t\":\"00:01\",\"te-\":20.8,\"te-t\":\"05:43\",\"hu+\":39,\"hu+t\":\"12:42\",\"hu-\":35,\"hu-t\":\"00:41\",\"outside\":\"yes\",\"favorite\":\"no\"},{\"id\":1,\"name\":\"Muziek kamer\",\"code\":\"2815108\",\"model\":1,\"lowBattery\":\"no\",\"version\":2.32,\"te\":19.4,\"hu\":45,\"te+\":19.6,\"te+t\":\"00:19\",\"te-\":18.8,\"te-t\":\"07:09\",\"hu+\":45,\"hu+t\":\"13:04\",\"hu-\":41,\"hu-t\":\"03:26\",\"outside\":\"no\",\"favorite\":\"no\"},{\"id\":2,\"name\":\"Julius\",\"code\":\"14974854\",\"model\":1,\"lowBattery\":\"no\",\"version\":2.32,\"te\":19.3,\"hu\":48,\"te+\":19.7,\"te+t\":\"08:11\",\"te-\":19.2,\"te-t\":\"06:17\",\"hu+\":58,\"hu+t\":\"08:21\",\"hu-\":45,\"hu-t\":\"00:01\",\"outside\":\"no\",\"favorite\":\"no\"},{\"id\":4,\"name\":\"Master Bedroom\",\"channel\":2,\"model\":0,\"te\":17.1,\"hu\":45,\"te+\":19.5,\"te+t\":\"00:00\",\"te-\":16.6,\"te-t\":\"08:24\",\"hu+\":45,\"hu+t\":\"08:57\",\"hu-\":38,\"hu-t\":\"03:11\",\"outside\":\"no\",\"favorite\":\"no\"}]}";

typedef struct _troom {
    char    Name[MAX_NAMELEN];                 // display name matches SensorNames
    int     Servos[MAX_UFHLOOPS_PER_ROOM];     // array of indices in Servos array (max 2)
    float   targetTemp;                        // target temp for room
    float   actualTemp;                        // actual room temp
} room_t;

room_t Rooms[MAXROOMS];

int noRooms = 0;

HTTPClient apiClient;
WiFiClient apiWiFiclient;
String apiEndpoint = "http://192.168.2.24/geheim1967/telist";

DECLARE_TIMERs(roomUpdate,15);

void roomsInit(){

    const char *rInit[MAXROOMS];

    // hardcoded for now, should come from rooms.ini file
    rInit[0] = "Living;1,7;22.5";
    rInit[1] = "Basement;4,-1;17.5";
    rInit[2] = "Kitchen;3,5;21.5";
    rInit[3] = "Dining;6,-1;22.5";
    rInit[4] = "Office;8,-1;20.5";
    rInit[5] = "Utility;9,-1;18.5";

    noRooms = 6;

    for(byte i=0 ; i < noRooms ; i++)
    {
        sscanf(rInit[i],"%[^;];%d,%d;%f", 
            Rooms[i].Name, 
            &Rooms[i].Servos[0],
            &Rooms[i].Servos[1],
            &Rooms[i].targetTemp);
        
        yield();
        dumpRoom(i);
    }
}

void dumpRoom(byte i)
{
    DebugTf("RoomDump: Name: %s Servos [%d,%d] Target/ActualTemp %f/%f \n",  
            Rooms[i].Name, 
            Rooms[i].Servos[0],
            Rooms[i].Servos[1],
            Rooms[i].targetTemp,
            Rooms[i].actualTemp);
}

#define QuotedString(x) "\"##x##\""

void handleRoomTemps()
{
    int apiRC;

    if (!DUE(roomUpdate))
        return;

    // Make API call

    timeThis(apiClient.begin(apiWiFiclient, apiEndpoint));
    timeThis(apiRC=apiClient.GET());
    if ( apiRC > 0)    // OK
    {
        const char *ptr;    
        String Response;
        // get response
        timeThis(Response = apiClient.getString()); 
        //DebugTf("Receive API response >%s<", Response);

        // process all records in resonse (name, te) pairs

        ptr=&Response.c_str()[0];

        while( (ptr=strstr(ptr,"\"name\"")))
        {
            char jsonName[20];
            float jsonTemp;

            yield();

            sscanf(ptr,"\"name\":\"%[^\"]", jsonName);
            ptr = (char*) &ptr[7];

            //DebugTf("name found: >%s<\n", jsonName);
            
            ptr = strstr(ptr,"\"te\"");
            sscanf(ptr,"\"te\":%f", &jsonTemp);
            ptr = (char*) &ptr[5];

            //DebugTf("name found: >%s< with te >%f<\n", jsonName, jsonTemp);

            // assign API result to correct Room 

            for( int8_t roomIndex=0 ; roomIndex < noRooms ; roomIndex++)
            {
                yield();

                if(!strcmp(Rooms[roomIndex].Name,jsonName))
                {
                    Rooms[roomIndex].actualTemp = jsonTemp;
                    //DebugTf("Match for room %s (%f) in room %s\n", jsonName, jsonTemp, Rooms[roomIndex].Name);

                    strcpy(jsonName,"found");
                    dumpRoom(roomIndex);
                    break;
                }
            }
            
            if(strcmp(jsonName, "found"))
                DebugTf("No match for room %s (%f)\n", jsonName, jsonTemp);   
        }  
    } else 
        DebugTf("API call to HomeWizard failed with rc %d\n", apiRC);
    
    apiClient.end();  
}

