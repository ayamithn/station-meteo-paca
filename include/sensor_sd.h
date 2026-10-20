#ifndef SENSOR_SD_H
#define SENSOR_SD_H
#include <stdbool.h>
bool sensorSD_Init(void);                      // initialise la carte SD
bool sensorSD_WriteLine(const char ligne);    // ajoute la ligne dans AAMMJJ_0.LOG
 
// Utilises par sd_archive.c pour archiver un fichier plein
bool sensorSD_FileExists(const charnom);                              // le fichier existe-t-il sur la carte ?
bool sensorSD_Rename(const char ancien_nom, const charnouveau_nom);  // renomme un fichier de la carte
#endif