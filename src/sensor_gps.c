#include "sensor_gps.h"
#include "grove_gps_air530z.h"
#include <stdio.h>

bool SensorGPS_Init(void)
{
    return GroveGPS_Init();
}

bool SensorGPS_Acquire(char *out, size_t size)
{
    GPS_Data gps_data;

    if (GroveGPS_GetPosition(&gps_data, 2000))
    {
        GroveGPS_FormatPosition(&gps_data, out, size);
        return true;
    }
    else
    {
        snprintf(out, size, "NA");
        return false;
    }
}