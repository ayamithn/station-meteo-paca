#include "status_led.h"
#include "grove_rgb.h"
#include "stm32l4xx_hal.h"
#include <stdint.h>
// Un motif contient deux couleurs et la duree de chaque phase.
typedef struct {
  uint8_t r1, g1, b1; // Couleur de la premiere phase.
  uint8_t r2, g2, b2; // Couleur de la seconde phase.
  uint16_t d1, d2;    // Duree des phases en millisecondes.
} Motif_t;

// Chaque motif definit deux couleurs et leurs durees d'affichage.
// L'ordre correspond aux valeurs de LedEtat_t dans status_led.h.
static const Motif_t motifs[] = {
   {  0, 255,   0,   0,   0,   0,   0,   0 }, // Mode standard : vert fixe.
   {255, 180,   0,   0,   0,   0,   0,   0 }, // Configuration : orange fixe.
   {  0,   0, 255,   0,   0,   0,   0,   0 }, // Economique : bleu fixe.
   {255,  60,   0,   0,   0,   0,   0,   0 }, // Maintenance : orange-rouge fixe.
   {255,   0,   0,   0,   0, 255, 500, 500 }, // Erreur RTC : rouge et bleu.
   {255,   0,   0, 255, 180,   0, 500, 500 }, // Erreur GPS : rouge et orange.
   {255,   0,   0,   0, 255,   0, 500, 500 }, // Erreur capteur : rouge et vert.
   {255,   0,   0,   0, 255,   0, 333, 667 }, // Incoherence : clignotement rouge/vert.
   {255,   0,   0, 255, 255, 255, 500, 500 }, // SD pleine : rouge et blanc.
   {255,   0,   0, 255, 255, 255, 333, 667 }  // Ecriture SD : clignotement rouge/blanc.
};

static LedEtat_t etat_courant = LED_STANDARD;
static int       phase = 0;
static uint32_t  debut_phase = 0;

// Applique la couleur correspondant a la phase courante.
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
  if (etat == etat_courant) return;
  etat_courant = etat;
  phase = 0;
  debut_phase = HAL_GetTick();
  afficher();
}

void StatusLed_Update(void)
{
  const Motif_t *m = &motifs[etat_courant];
  if (m->d2 == 0) return;

  // Les erreurs utilisent les phases pour produire un clignotement.
  uint16_t duree = (phase == 0) ? m->d1 : m->d2;
  if ((HAL_GetTick() - debut_phase) >= duree) {
    phase = 1 - phase;
    debut_phase = HAL_GetTick();
    afficher();
  }
}