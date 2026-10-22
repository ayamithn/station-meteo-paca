#include "sd_archive.h"
#include "sensor_sd.h"
#include <stdio.h>
#include <string.h>

/*
 * ARCHIVAGE DU FICHIER CSV PLEIN (README, section 6)
 *
 * Règle du sujet : le système écrit toujours dans AAMMJJ_0.CSV. Quand ce fichier est
 * plein (>= FILE_MAX_SIZE), on en garde une copie sous le premier numéro de révision
 * libre (AAMMJJ_1.CSV, _2, ...) puis on repart avec un AAMMJJ_0.CSV vide.
 *
 * On RENOMME _0 en _N au lieu de le recopier : le contenu est conservé dans _N,
 * _0 n'existe plus et sera recréé vide à la prochaine écriture (instantané, sans
 * risque de copie incomplète).
 *
 * Le projet utilise sa propre couche FAT (sensor_sd.c), pas FatFs : on passe donc
 * par SensorSD_FileExists() et SensorSD_Rename().
 */

// Noms en 8.3 : "AAMMJJ_N" = 8 caractères max, donc N de 1 à 9
#define SD_REVISION_MAX  9

// nom_fichier : "AAMMJJ_0.CSV". Renvoie true si l'archive a été créée.
bool SDArchive_Rotate(const char *nom_fichier)
{
  if (nom_fichier == NULL) return false;

  // Sépare "AAMMJJ_" (préfixe) et ".CSV" (extension)
  const char *souligne = strrchr(nom_fichier, '_');
  const char *point    = strrchr(nom_fichier, '.');
  if (souligne == NULL || point == NULL || point < souligne || strcmp(point, ".CSV") != 0)
    return false;

  char prefixe[16];
  size_t taille = (size_t)(souligne - nom_fichier) + 1;      // inclut le '_'
  if (taille >= sizeof(prefixe)) return false;
  memcpy(prefixe, nom_fichier, taille);
  prefixe[taille] = '\0';

  // Cherche la première révision libre et y déplace le fichier plein
  for (int rev = 1; rev <= SD_REVISION_MAX; rev++)
  {
    char nouveau[24];
    snprintf(nouveau, sizeof(nouveau), "%s%d%s", prefixe, rev, point);

    if (!SensorSD_FileExists(nouveau))                        // libre : on archive ici
      return SensorSD_Rename(nom_fichier, nouveau);
  }

  return false;   // les 9 révisions du jour sont prises : le système doit le signaler (LED)
}