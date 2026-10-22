#ifndef SENSOR_SD_H
#define SENSOR_SD_H
#include <stdbool.h>

bool SensorSD_Init(void);
bool SensorSD_WriteLine(const char *ligne);


bool SensorSD_FileExists(const char *nom);
bool SensorSD_Rename(const char *ancien_nom, const char *nouveau_nom);
#endif