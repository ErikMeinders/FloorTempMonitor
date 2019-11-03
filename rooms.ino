#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#define MAXROOMS                8
#define MAX_UFHLOOPS_PER_ROOM   2
#define MAX_NAMELEN             20

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

DECLARE_TIMERs(roomUpdate,60);

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
        
        timeCritical();
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
            bool  nameFound=false;

            timeCritical();

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
                timeCritical();

                if(!strcmp(Rooms[roomIndex].Name,jsonName))
                {
                    Rooms[roomIndex].actualTemp = jsonTemp;
                    //DebugTf("Match for room %s (%f) in room %s\n", jsonName, jsonTemp, Rooms[roomIndex].Name);

                    nameFound=true;
                    dumpRoom(roomIndex);
                    break;
                }
            }
            
            if(!nameFound)
                DebugTf("No match for room %s (%f)\n", jsonName, jsonTemp);   
        }  
    } else 
        DebugTf("API call to HomeWizard failed with rc %d\n", apiRC);
    
    apiClient.end();  
}

