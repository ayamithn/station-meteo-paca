#include "sd_archive.h"
#include "sensor_sd.h"
#include <stdio.h>
#include <string.h>




#define SD_REVISION_MAX  9


bool SDArchive_Rotate(const char *nom_fichier)
{
  if (nom_fichier == NULL) return false;


  const char *souligne = strrchr(nom_fichier, '_');
  const char *point    = strrchr(nom_fichier, '.');
  if (souligne == NULL || point == NULL || point < souligne || strcmp(point, ".CSV") != 0)
    return false;

  char prefixe[16];
  size_t taille = (size_t)(souligne - nom_fichier) + 1;
  if (taille >= sizeof(prefixe)) return false;
  memcpy(prefixe, nom_fichier, taille);
  prefixe[taille] = '\0';


  for (int rev = 1; rev <= SD_REVISION_MAX; rev++)
  {
    char nouveau[24];
    snprintf(nouveau, sizeof(nouveau), "%s%d%s", prefixe, rev, point);

    if (!SensorSD_FileExists(nouveau))
      return SensorSD_Rename(nom_fichier, nouveau);
  }

  return false;
}