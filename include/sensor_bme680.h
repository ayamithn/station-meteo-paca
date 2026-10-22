#ifndef SENSOR_BME680_H
#define SENSOR_BME680_H

#include <stdbool.h>
#include <stddef.h>
#include "cesi_types.h"



bool SensorBME680_Init(void);




bool SensorBME680_Read(Env_Data *data);





bool SensorBME680_Acquire(char *out, size_t size);


void SensorBME680_ValeurEnTexte(float valeur, char *out, size_t size);

#endif