#ifndef SD_ARCHIVE_H
#define SD_ARCHIVE_H

#include <stdbool.h>

/*
 * ARCHIVAGE DU FICHIER DE LOG PLEIN
 *
 * Règle du sujet : le système écrit toujours dans le fichier de révision 0
 * (ex : 261020_0.LOG). Quand il atteint FILE_MAX_SIZE, son contenu est conservé
 * sous le premier numéro de révision libre (261020_1.LOG, 261020_2.LOG, ...),
 * puis l'enregistrement repart dans un fichier de révision 0 vide.
 *
 * nom_fichier : nom du fichier plein, au format "AAMMJJ_0.LOG"
 *               (le fichier doit être fermé avant l'appel)
 *
 * Renvoie true  : archive créée, le fichier de révision 0 peut être recréé vide
 *         false : échec (carte inaccessible, plus de révision libre, nom invalide)
 *                 -> à signaler avec la LED d'erreur SD
 */
bool SDArchive_Rotate(const char *nom_fichier);

#endif