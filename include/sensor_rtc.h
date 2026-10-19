#ifndef SENSOR_RTC_H
#define SENSOR_RTC_H

#include <stdbool.h>
#include <stddef.h>

bool SensorRTC_Init(void);                        // initialise l'horloge RTC
bool SensorRTC_Acquire(char *out, size_t size);   // écrit "AAAA-MM-JJ HH:MM:SS" dans out ("NA" si échec)
                                                  // renvoie true si la date lue est valide
bool SensorRTC_FileDate(char *out, size_t size);  // écrit "AAMMJJ" (ex : "261019") pour nommer le fichier .LOG
                                                  // renvoie true si la date lue est valide
#endif