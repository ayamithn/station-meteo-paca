#include "sensor_light.h"
#include "grove_light.h"
#include <stdio.h>

bool SensorLight_Init(void)
{
  GroveLight_Init();
  return true;
}


bool SensorLight_Acquire(char *out, size_t size)
{
  unsigned long brut = (unsigned long)GroveLight_ReadRaw();
  if (brut > 4095UL) {
    snprintf(out, size, "NA");
    return false;
  }
  snprintf(out, size, "%lu", brut / 4UL);
  return true;
}