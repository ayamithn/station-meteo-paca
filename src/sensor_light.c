#include "sensor_light.h"
#include <stdio.h>

// TODO : a remplir par le responsable de ce capteur
bool SensorLight_Init(void)
{
  return true;
}

bool SensorLight_Acquire(char *out, size_t size)
{
  snprintf(out, size, "NA");   // en attendant le vrai code
  return false;
}
