#include "sensor_gps.h"
#include <stdio.h>

// TODO : a remplir par le responsable de ce capteur
bool SensorGPS_Init(void)
{
  return true;
}

bool SensorGPS_Acquire(char *out, size_t size)
{
  snprintf(out, size, "NA");   // en attendant le vrai code
  return false;
}
