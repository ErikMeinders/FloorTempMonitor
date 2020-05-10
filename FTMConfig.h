#ifndef _CONF
#define _CONF

// ideally these values at some point should come from a config file ...

#define MAX_WATER_TEMP      40.0    // CV setting of max water temperature, currently 40 for UGFH
#define HEAT_ON_PCT         90.0    // consider heating is on when input temp > 90% of MAX_WATER_TEMP
#define RETURN_PCT          80.0    // strive for a return temp of less than 80% of MAX_WATER_TEMP
                                    // valve will close when return temp above threshold

#define HEATER_ON_TEMP      (HEAT_ON_PCT/100.0 * MAX_WATER_TEMP)
#define HOTTEST_RETURN      (RETURN_PCT/100.0  * MAX_WATER_TEMP)

#endif