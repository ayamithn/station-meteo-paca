#ifndef SENSOR_RTC_H
#define SENSOR_RTC_H

#include <stdbool.h>
#include <stddef.h>
#include "cesi_types.h"

bool SensorRTC_Init(void);
bool SensorRTC_Acquire(char *out, size_t size);

bool SensorRTC_FileDate(char *out, size_t size);



bool SensorRTC_GetDateTime(RTC_DateTime *t);
bool SensorRTC_SetDateTime(const RTC_DateTime *t);
void SensorRTC_SetFromBuild(const char *date, const char *heure);
#endif