#include "sd_archive.h"
#include <stdio.h>
#include <string.h>

/*
 * ARCHIVAGE DU FICHIER DE LOG PLEIN (README, section 6)
 *
 * Règle du sujet : le système écrit toujours dans AAMMJJ_0.LOG. Quand ce fichier est
 * plein (>= FILE_MAX_SIZE), on en garde une copie sous le premier numéro de révision
 * libre (AAMMJJ_1.LOG, _2, ...) puis on repart avec un AAMMJJ_0.LOG vide.
 *
 * Ici on RENOMME _0 en _N au lieu de le recopier octet par octet : le résultat est
 * identique (le contenu est conservé dans _N, _0 n'existe plus et sera recréé vide
 * à la prochaine écriture) mais c'est instantané et sans risque de copie incomplète.
 *
 * PRÉ-REQUIS : un système de fichiers FAT. Le projet n'en contient pas encore
 * (SDLogger_WriteLine dit "File write support needs FatFs + diskio SPI").
 * Tant que FatFs n'est pas intégré, laisser SD_FATFS_DISPONIBLE à 0 : la fonction
 * renvoie false (comme avant) et le projet compile. Passer à 1 une fois FatFs en place.
 *
 * ATTENTION : le fichier doit être FERMÉ (f_close) avant d'appeler SDArchive_Rotate.
 */
#define SD_FATFS_DISPONIBLE  0

#if SD_FATFS_DISPONIBLE

#include "ff.h"

// Noms en 8.3 (sans noms longs FatFs) : "AAMMJJ_N" = 8 caractères max, donc N de 1 à 9
#define SD_REVISION_MAX  9

// nom_fichier : "AAMMJJ_0.LOG". Renvoie true si l'archive a été créée.
bool SDArchive_Rotate(const char *nom_fichier)
{
  if (nom_fichier == NULL) return false;

  // Sépare "AAMMJJ_" (préfixe) et ".LOG" (extension)
  const char *souligne = strrchr(nom_fichier, '_');
  const char *point    = strrchr(nom_fichier, '.');
  if (souligne == NULL || point == NULL || point < souligne) return false;

  char prefixe[16];
  size_t taille = (size_t)(souligne - nom_fichier) + 1;      // inclut le '_'
  if (taille >= sizeof(prefixe)) return false;
  memcpy(prefixe, nom_fichier, taille);
  prefixe[taille] = '\0';

  // Cherche la première révision libre et y déplace le fichier plein
  for (int rev = 1; rev <= SD_REVISION_MAX; rev++)
  {
    char nouveau[24];
    FILINFO info;

    snprintf(nouveau, sizeof(nouveau), "%s%d%s", prefixe, rev, point);

    FRESULT r = f_stat(nouveau, &info);
    if (r == FR_NO_FILE)                                      // libre : on archive ici
      return f_rename(nom_fichier, nouveau) == FR_OK;
    if (r != FR_OK)                                           // erreur d'accès à la carte
      return false;
    // sinon cette révision existe déjà : on essaie la suivante
  }

  return false;   // les 9 révisions du jour sont prises : le système doit le signaler (LED)
}

#else

// FatFs pas encore intégré : rien ne peut être archivé (même comportement qu'avant)
bool SDArchive_Rotate(const char *nom_fichier)
{
  (void)nom_fichier;
  return false;
}

#endif