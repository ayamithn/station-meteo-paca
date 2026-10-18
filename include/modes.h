#ifndef MODES_H
#define MODES_H

#include <stdbool.h>
#include <stdint.h>
#include "status_led.h"

typedef enum {
  MODE_STANDARD,
  MODE_CONFIGURATION,
  MODE_MAINTENANCE,
  MODE_ECONOMIQUE
} Mode_t;

// Paramètres du système (modifiables en mode configuration)
typedef struct {
  int32_t log_interval_s;                          // intervalle entre 2 mesures (secondes)
  int32_t file_max_size;                           // taille max d'un fichier de log (octets)
  int32_t timeout_s;                               // timeout d'un capteur (secondes)
  int32_t lumin, lumin_low, lumin_high;            // luminosité
  int32_t temp_air, min_temp_air, max_temp_air;    // température de l'air
  int32_t hygr, hygr_mint, hygr_maxt;              // hygrométrie
  int32_t pressure, pressure_min, pressure_max;    // pression
} Config_t;

void   Modes_Init(void);                 // LED, boutons, paramètres par défaut, mode de départ
void   Modes_Update(void);               // à appeler en boucle : boutons, transitions, console, LED
Mode_t Modes_Get(void);                  // mode actuel
bool   Modes_AcquisitionActive(void);    // false en mode configuration
bool   Modes_SdAutorisee(void);          // false en mode maintenance
void   Modes_SetErreur(LedEtat_t err);   // la LED affiche cette erreur à la place du mode
void   Modes_ClearErreur(void);          // la LED revient à la couleur du mode
const Config_t *Modes_Config(void);      // paramètres en lecture seule

#endif