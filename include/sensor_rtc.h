#ifndef SENSOR_RTC_H
#define SENSOR_RTC_H

#include <stdbool.h>
#include <stddef.h>

bool SensorRTC_Init(void);                        // initialise le capteur
bool SensorRTC_Acquire(char *out, size_t size);   // ecrit la valeur en texte dans out ("NA" si echec)
                                           // renvoie true si la mesure est valide
#endif
