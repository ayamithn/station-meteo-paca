#include "sensor_bme680.h"
#include <stdio.h>

// TODO : a remplir par le responsable de ce capteur
bool SensorBME680_Init(void)
{
  return true;
}

bool SensorBME680_Acquire(char *out, size_t size)
{
  snprintf(out, size, "NA");   // en attendant le vrai code
  return false;
}
