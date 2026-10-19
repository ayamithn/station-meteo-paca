#include "status_led.h"
#include "grove_rgb.h"          // pilote du prof (LED chainable sur D6)
#include "stm32l4xx_hal.h"      // HAL_GetTick
#include <stdint.h>

// Un motif = 2 couleurs et leur durée (d2 = 0 : couleur fixe, pas de clignotement)
typedef struct {
  uint8_t r1, g1, b1;   // couleur 1
  uint8_t r2, g2, b2;   // couleur 2
  uint16_t d1, d2;      // durées en ms (d1 + d2 = 1000 ms pour 1 Hz)
} Motif_t;

// Le tableau suit l'ordre de l'enum LedEtat_t
static const Motif_t motifs[] = {
  /* STANDARD       */ {  0, 255,   0,   0,   0,   0,   0,   0 },   // vert
  /* CONFIGURATION  */ {255, 180,   0,   0,   0,   0,   0,   0 },   // jaune
  /* ECONOMIQUE     */ {  0,   0, 255,   0,   0,   0,   0,   0 },   // bleu
  /* MAINTENANCE    */ {255,  60,   0,   0,   0,   0,   0,   0 },   // orange
  /* ERR_RTC        */ {255,   0,   0,   0,   0, 255, 500, 500 },   // rouge / bleu
  /* ERR_GPS        */ {255,   0,   0, 255, 180,   0, 500, 500 },   // rouge / jaune
  /* ERR_CAPTEUR    */ {255,   0,   0,   0, 255,   0, 500, 500 },   // rouge / vert
  /* ERR_INCOHERENT */ {255,   0,   0,   0, 255,   0, 333, 667 },   // rouge / vert (vert x2)
  /* SD_PLEINE      */ {255,   0,   0, 255, 255, 255, 500, 500 },   // rouge / blanc
  /* SD_ECRITURE    */ {255,   0,   0, 255, 255, 255, 333, 667 }    // rouge / blanc (blanc x2)
};

static LedEtat_t etat_courant = LED_STANDARD;
static int       phase = 0;          // 0 = couleur 1, 1 = couleur 2
static uint32_t  debut_phase = 0;    // instant du dernier changement de couleur

// Envoie la couleur de la phase en cours à la LED
static void afficher(void)
{
  const Motif_t *m = &motifs[etat_courant];
  if (phase == 0) GroveRGB_SetColor(m->r1, m->g1, m->b1);
  else            GroveRGB_SetColor(m->r2, m->g2, m->b2);
}

void StatusLed_Init(void)
{
  GroveRGB_Init();
  phase = 0;
  debut_phase = HAL_GetTick();
  afficher();
}

void StatusLed_Set(LedEtat_t etat)
{
  if (etat == etat_courant) return;      // rien à changer
  etat_courant = etat;
  phase = 0;
  debut_phase = HAL_GetTick();
  afficher();
}

void StatusLed_Update(void)
{
  const Motif_t *m = &motifs[etat_courant];
  if (m->d2 == 0) return;                // couleur fixe : rien à faire

  uint16_t duree = (phase == 0) ? m->d1 : m->d2;
  if ((HAL_GetTick() - debut_phase) >= duree) {
    phase = 1 - phase;                   // on passe à l'autre couleur
    debut_phase = HAL_GetTick();
    afficher();
  }
}