#ifndef SENSOR_LIGHT_H
#define SENSOR_LIGHT_H

#include <stdbool.h>
#include <stddef.h>

bool SensorLight_Init(void);                        // initialise le capteur
bool SensorLight_Acquire(char *out, size_t size);   // ecrit la valeur en texte dans out ("NA" si echec)
                                           // renvoie true si la mesure est valide
#endif
