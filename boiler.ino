
#define ERIXREPO

DECLARE_TIMER(Boiler, 60);

#define OT "http://192.168.2.107/api/v0/override/"

#define B_OT_TARGET_WATER 1
#define B_OT_TARGET_ROOM 16
#define B_OT_ACTUAL_ROOM 24

byte boiler_IP[] = { 192, 168, 2, 107};

void updateBoiler()
{
    if(!DUE(Boiler))
        return;

    // look for room with highest demand

    float  largest_delta=-20.0;
    int8_t largest_i = -1;

    for(int8_t i=0 ; i < noRooms ; i++)
    {
        if(Rooms[i].targetTemp - Rooms[i].actualTemp > largest_delta)
        {
            largest_delta = Rooms[i].targetTemp - Rooms[i].actualTemp;
            largest_i = i;
        }
    }

    // any room in need of heating?

    DebugTf("Room with largest delta t is %s (%.1f)\n", Rooms[largest_i].Name, largest_delta);

    if( largest_delta > 0.0)
    {   
        DebugTf("At least one room needs to be heated\n");

    } else {
        DebugTf("All rooms at or above target temperature\n");
    }

    // send highest demand to OpenTherm controller

    informBoiler(Rooms[largest_i].actualTemp,Rooms[largest_i].targetTemp );
}

void informBoiler(float actual, float target)
{
    char toPut[128];

    WiFiClient apiWiFiclient;

    apiWiFiclient.connect(boiler_IP,80);

    sprintf(toPut,"PUT /api/v0/override/%d/%.1f HTTP/1.1\r\n\r\n", B_OT_TARGET_ROOM, target);
    DebugTf("%s", toPut);

    apiWiFiclient.print(toPut);
    delay(500);
    apiWiFiclient.stop();

    apiWiFiclient.connect(boiler_IP,80);

    sprintf(toPut,"PUT /api/v0/override/%d/%.1f HTTP/1.1\r\n\r\n", B_OT_ACTUAL_ROOM, actual);
    DebugTf("%s", toPut);

    apiWiFiclient.print(toPut);
    delay(500);
    apiWiFiclient.stop();
}

void informBoiler(bool on)
{
    char toPut[128];
    float waterShouldBe;

    WiFiClient apiWiFiclient;

    apiWiFiclient.connect(boiler_IP,80);

    sprintf(toPut,"PUT /api/v0/boiler/%s HTTP/1.1\r\n\r\n",on ? "on" : "off");
    DebugTf("%s", toPut);

    apiWiFiclient.print(toPut);
    delay(500);
    apiWiFiclient.stop();

    if(on){
        waterShouldBe = 40.0;
    } else
    {
        waterShouldBe = 0.0;
    }
    
    apiWiFiclient.connect(boiler_IP,80);

    sprintf(toPut,"PUT /api/v0/override/%d/%.1f HTTP/1.1\r\n\r\n", B_OT_TARGET_WATER, waterShouldBe);
    DebugTf("%s", toPut);

    apiWiFiclient.print(toPut);
    delay(500);
    apiWiFiclient.stop();


}
