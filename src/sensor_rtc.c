#include "sensor_rtc.h"
#include <stdio.h>

// TODO : a remplir par le responsable de ce capteur
bool SensorRTC_Init(void)
{
  return true;
}

bool SensorRTC_Acquire(char *out, size_t size)
{
  snprintf(out, size, "NA");   // en attendant le vrai code
  return false;
}
