#ifndef SENSOR_BME680_H
#define SENSOR_BME680_H

#include <stdbool.h>
#include <stddef.h>

bool SensorBME680_Init(void);                        // initialise le capteur
bool SensorBME680_Acquire(char *out, size_t size);   // ecrit la valeur en texte dans out ("NA" si echec)
                                           // renvoie true si la mesure est valide
#endif
