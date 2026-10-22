#include "status_led.h"
#include "grove_rgb.h"
#include "stm32l4xx_hal.h"
#include <stdint.h>


typedef struct {
  uint8_t r1, g1, b1;
  uint8_t r2, g2, b2;
  uint16_t d1, d2;
} Motif_t;


static const Motif_t motifs[] = {
   {  0, 255,   0,   0,   0,   0,   0,   0 },
   {255, 180,   0,   0,   0,   0,   0,   0 },
   {  0,   0, 255,   0,   0,   0,   0,   0 },
   {255,  60,   0,   0,   0,   0,   0,   0 },
   {255,   0,   0,   0,   0, 255, 500, 500 },
   {255,   0,   0, 255, 180,   0, 500, 500 },
   {255,   0,   0,   0, 255,   0, 500, 500 },
   {255,   0,   0,   0, 255,   0, 333, 667 },
   {255,   0,   0, 255, 255, 255, 500, 500 },
   {255,   0,   0, 255, 255, 255, 333, 667 }
};

static LedEtat_t etat_courant = LED_STANDARD;
static int       phase = 0;
static uint32_t  debut_phase = 0;


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

  uint16_t duree = (phase == 0) ? m->d1 : m->d2;
  if ((HAL_GetTick() - debut_phase) >= duree) {
    phase = 1 - phase;
    debut_phase = HAL_GetTick();
    afficher();
  }
}