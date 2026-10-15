#ifndef SENSOR_SD_H
#define SENSOR_SD_H
#include <stdbool.h>
bool SensorSD_Init(void);                      // initialise la carte SD
bool SensorSD_WriteLine(const char *ligne);    // ajoute la ligne dans AAMMJJ_0.LOG
#endif
