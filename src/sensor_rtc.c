#include "sensor_rtc.h"
#include "grove_rtc_ds1307.h"   // pilote du prof
#include <stdio.h>

bool SensorRTC_Init(void)
{
  return GroveRTC_Init();       // true si la RTC répond sur l'I2C
}

// Écrit "AAAA-MM-JJ HH:MM:SS" dans out, ou "NA" si la RTC ne répond pas
bool SensorRTC_Acquire(char *out, size_t size)
{
  RTC_DateTime now;
  if (!GroveRTC_GetDateTime(&now)) {
    snprintf(out, size, "NA");
    return false;
  }
  snprintf(out, size, "%04u-%02u-%02u %02u:%02u:%02u",
           (unsigned)now.year, (unsigned)now.month, (unsigned)now.day,
           (unsigned)now.hours, (unsigned)now.minutes, (unsigned)now.seconds);
  return true;
}