#include "sensor_light.h"
#include "grove_light.h"   // pilote du prof
#include <stdio.h>

bool SensorLight_Init(void)
{
  GroveLight_Init();       // configure PA4 et l'ADC
  return true;
}

// Écrit la luminosité de 0 à 1023, ou "NA" si la valeur est impossible
bool SensorLight_Acquire(char *out, size_t size)
{
  unsigned long brut = (unsigned long)GroveLight_ReadRaw();   // 12 bits : 0 à 4095 (0xFFFF = erreur)
  if (brut > 4095UL) {
    snprintf(out, size, "NA");
    return false;
  }
  snprintf(out, size, "%lu", brut / 4UL);   // converti en 10 bits comme dans le document
  return true;
}