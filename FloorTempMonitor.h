#ifndef _FTM_H
#define _FTM_H


#define MAXROOMS                7
#define MAX_UFHLOOPS_PER_ROOM   2
#define MAX_NAMELEN             20

#define ONE_WIRE_BUS          0     // Data Wire is plugged into GPIO-00
#define _MAX_SENSORS          12    // 16 Servo's/Relais + heater in & out
#define _MAX_SERVOS           10
#define _MAX_NAME_LEN         12
#define _FIX_SETTINGSREC_LEN  85
#define _MAX_DATAPOINTS       60   // 24 hours every 15 minutes - more will crash the gui
#define _LAST_DATAPOINT      (_MAX_DATAPOINTS -1)
#define _REFLOW_TIME         (5*60000) // 5 minutes
#define _DELTATEMP_CHECK      1     // when to check the deltaTemp's in minutes
#define _HOUR_CLOSE_ALL       3     // @03:00 start closing servos one-by-one. Don't use 1:00!
#define _MIN                  60000 // milliSecs in a minute

typedef struct _troom {
    char    Name[MAX_NAMELEN];                 // display name matches SensorNames
    int     Servos[MAX_UFHLOOPS_PER_ROOM];     // array of indices in Servos array (max 2)
    float   targetTemp;                        // target temp for room
    float   actualTemp;                        // actual room temp
} room_t;

room_t Rooms[MAXROOMS];

int noRooms = 0;


enum   e_servoState { SERVO_IS_OPEN, SERVO_IS_CLOSED, SERVO_IN_LOOP, SERVO_COUNT0_CLOSE, ERROR };

typedef struct _sensorStruct {
  int8_t    index;
  char      sensorID[20];
  uint8_t   position;
  char      name[_MAX_NAME_LEN];
  float     tempOffset;     // calibration
  float     tempFactor;     // calibration
  int8_t    servoNr;        // index in servoArray
  float     deltaTemp;      //-- in S0 -> closeTime
  float     tempC;          //-- not in sensors.ini
 
} sensorStruct;

typedef struct _servoStruct {
  uint8_t   servoState;     
  uint32_t  servoTimer;
  uint8_t   closeCount;

} servoStruct;

typedef struct _dataStruct{
  uint32_t  timestamp;
  float     tempC[_MAX_SENSORS];
  int8_t    servoStateV[_MAX_SENSORS];
} dataStruct;

#endif
