#ifndef SENSOR_BME680_H
#define SENSOR_BME680_H

#include <stdbool.h>
#include <stddef.h>
#include "cesi_types.h"   // contient la structure Env_Data (température, humidité, pression, gaz)

// Initialise le capteur BME680 (détection sur le bus I2C, lecture de sa calibration, réglages).
// Renvoie true si le capteur répond.
bool SensorBME680_Init(void);

// Fait UNE mesure et range les résultats dans "data" (°C, %, hPa, ohm).
// Renvoie true si la mesure est valide, false sinon (capteur absent, pas de réponse...).
// Note : gas_resistance_ohm vaut 0 tant que le capteur de gaz n'a pas de valeur valide.
bool SensorBME680_Read(Env_Data *data);

// Fait une mesure et écrit le résultat en texte dans out : "température;humidité;pression"
// (exemple : "21.4;48.2;1013.2"). Si la mesure échoue, écrit "NA;NA;NA".
// Renvoie true si la mesure est valide. (Appelée par main.c du projet)
bool SensorBME680_Acquire(char *out, size_t size);

// Transforme un nombre à virgule en texte avec 1 décimale (ex : 21.4) pour l'afficher en console.
void SensorBME680_ValeurEnTexte(float valeur, char *out, size_t size);

#endif