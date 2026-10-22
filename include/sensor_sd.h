#ifndef SENSOR_SD_H
#define SENSOR_SD_H
#include <stdbool.h>

bool SensorSD_Init(void);                      // initialise la carte SD
bool SensorSD_WriteLine(const char *ligne);    // ajoute la ligne dans AAMMJJ_0.CSV (la ligne doit finir par '\n')

// Utilises par sd_archive.c pour archiver un fichier plein
bool SensorSD_FileExists(const char *nom);                              // le fichier existe-t-il sur la carte ?
bool SensorSD_Rename(const char *ancien_nom, const char *nouveau_nom);  // renomme un fichier de la carte
#endif