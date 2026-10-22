#ifndef SENSOR_GPS_H
#define SENSOR_GPS_H

#include <stdbool.h>
#include <stddef.h>

bool SensorGPS_Init(void);
bool SensorGPS_Acquire(char *out, size_t size);

#endif
