#ifndef SENSOR_GPS_H
#define SENSOR_GPS_H

#include <stdbool.h>
#include <stddef.h>

bool SensorGPS_Init(void);                        // initialise le capteur
bool SensorGPS_Acquire(char *out, size_t size);   // ecrit la valeur en texte dans out ("NA" si echec)
                                           // renvoie true si la mesure est valide
#endif
