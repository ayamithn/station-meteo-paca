#ifndef SENSOR_LIGHT_H
#define SENSOR_LIGHT_H

#include <stdbool.h>
#include <stddef.h>

bool SensorLight_Init(void);
bool SensorLight_Acquire(char *out, size_t size);

#endif
