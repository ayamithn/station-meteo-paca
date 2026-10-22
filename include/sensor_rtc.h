#ifndef SENSOR_RTC_H
#define SENSOR_RTC_H

#include <stdbool.h>
#include <stddef.h>
#include "cesi_types.h"

bool SensorRTC_Init(void);                        // initialise l'horloge RTC
bool SensorRTC_Acquire(char *out, size_t size);   // écrit "AAAA-MM-JJ HH:MM:SS" dans out ("NA" si échec)
                                                  // renvoie true si la date lue est valide
bool SensorRTC_FileDate(char *out, size_t size);  // écrit "AAMMJJ" (ex : "261019") pour nommer le fichier .CSV
                                                  // renvoie true si la date lue est valide

// Heure du système : RTC matérielle si elle répond, sinon horloge logicielle
bool SensorRTC_GetDateTime(RTC_DateTime *t);      // false si aucune des deux n'est disponible
bool SensorRTC_SetDateTime(const RTC_DateTime *t);// règle la RTC (si présente) ET l'horloge logicielle
void SensorRTC_SetFromBuild(const char *date, const char *heure);  // règle l'horloge logicielle avec __DATE__ et __TIME__
#endif