#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#include "FloorTempMonitor.h"

HTTPClient apiClient;
WiFiClient apiWiFiclient;

String apiEndpoint = "http://192.168.2.24/geheim1967/telist";

DECLARE_TIMERs(roomUpdate,60);

#define tempMargin 0.2

void roomsRead()
{
    File file = SPIFFS.open("/rooms.ini", "r");
    int i = 0;

    char buffer[64];
    while (file.available()) {
        int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
        buffer[l] = 0;
    
        sscanf(buffer,"%[^;];%d,%d;%f", 
            Rooms[i].Name, 
            &Rooms[i].Servos[0],
            &Rooms[i].Servos[1],
            &Rooms[i].targetTemp);
        
        yield();
        roomDump(i);
        i++;
    }
    noRooms = i;
    file.close();
}

void roomsWrite()
{
    File file = SPIFFS.open("/rooms.ini", "w");
    char buffer[64];

    for (int8_t i=0 ; i < noRooms ; i++)
    {    
        sprintf(buffer,"%s;%d,%d;%f\n", 
            Rooms[i].Name, 
            Rooms[i].Servos[0],
            Rooms[i].Servos[1],
            Rooms[i].targetTemp);
        
        yield();
        file.write(buffer, strlen(buffer));
        roomDump(i);
    }
    file.close(); //
}

void roomsInit()
{

   roomsRead();
}

void roomDump(byte i)
{
    DebugTf("roomDump: Name: %s Servos [%d,%d] Target/ActualTemp %f/%f \n",  
            Rooms[i].Name, 
            Rooms[i].Servos[0],
            Rooms[i].Servos[1],
            Rooms[i].targetTemp,
            Rooms[i].actualTemp);
}

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

        // process all records in resonse (name, te) pairs

        Response.trim();
        ptr=&Response.c_str()[0];

        if (!strlen(ptr) || ptr[strlen(ptr)-1] != '}')
        {
            DebugTf("[HomeWizard] No response or last char not '}'\n");
            // just a warning, do proceed to find usable values in result
        }

        DebugTf("ptr = >%s<\n", ptr);
        while( (ptr=strstr(ptr,"\"name\"")))
        {
            char jsonName[20];
            float jsonTemp;
            bool  nameFound=false;

            //DebugTf("ptr = >%s<\n", ptr);

            yield();
            
            sscanf(ptr,"\"name\":\"%[^\"]", jsonName);
            ptr = (char*) &ptr[7];

            //DebugTf("name found: >%s<\n", jsonName);
            
            if(!(ptr = strstr(ptr,"\"te\"")))
            {
                DebugTf("[HomeWizard] Incomplete JSON response, no 'te' found for %s\n", jsonName);
                break;
            }
            sscanf(ptr,"\"te\":%f", &jsonTemp);
            ptr = (char*) &ptr[5];

            // assign API result to correct Room 

            for( int8_t roomIndex=0 ; roomIndex < noRooms ; roomIndex++)
            {
                yield();

                if(!strcmp(Rooms[roomIndex].Name,jsonName))
                {
                    Rooms[roomIndex].actualTemp = jsonTemp;
                    //DebugTf("Match for room %s (%f) in room %s\n", jsonName, jsonTemp, Rooms[roomIndex].Name);

                    nameFound=true;
                    roomDump(roomIndex);
                    byte s;
                    for(byte i=0 ; (s=Rooms[roomIndex].Servos[i]) > 0 ; i++)
                    {
                        // close servos based on room temperature?
                        if (Rooms[roomIndex].actualTemp > Rooms[roomIndex].targetTemp+tempMargin )
                        {
                            servoClose(s,ROOM_HOT);
                        }
                        // open servos based on room temperature
                        if (Rooms[roomIndex].actualTemp < Rooms[roomIndex].targetTemp-tempMargin)
                        {
                            servoOpen(s,ROOM_HOT);
                        }
                    }

                    break;
                }
            }
            
            if(!nameFound)
                DebugTf("No match for room %s (%f)\n", jsonName, jsonTemp);   
        }  
    } else 
        DebugTf("[HomeWizard] API call failed with rc %d\n", apiRC);
    
    apiClient.end();  
}

