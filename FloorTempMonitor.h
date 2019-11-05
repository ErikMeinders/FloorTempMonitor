#ifndef _FTM_H
#define _FTM_H


#define MAXROOMS                7
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

#endif
