#include "board.h"          // Board_Init, Serial_Print, Delay_ms
#include "setup.h"          // Global_Init : I2C1 et SPI1 (voir src/setup.c)
#include "grove_rgb.h"      // LED d'état (D6)
#include "grove_button.h"   // boutons rouge / vert (D2)
#include "sensor_rtc.h"
#include "sensor_light.h"
#include "sensor_bme680.h"
#include "sensor_gps.h"
#include "sensor_sd.h"
#include "stm32l4xx_hal.h"  // HAL_GetTick, GPIO
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

// ====================================================================
// 1 = test avec la carte seule (le bouton bleu B1 joue le bouton rouge,
//     intervalles et appuis raccourcis)
// 0 = version finale avec le shield, les capteurs et le Grove Dual Button
// ====================================================================
#define TEST_CARTE_SEULE  1

#if TEST_CARTE_SEULE
  #define LOG_INTERVAL_S       2        // une mesure toutes les 2 s
  #define DUREE_APPUI_LONG_MS  2000UL   // appui "long" de 2 s
  // B1 (PC13) : appuyé = niveau bas. Pas de bouton vert sans le Dual Button.
  #define BTN_ROUGE_PRESSE()   (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
  #define BTN_VERT_PRESSE()    false
#else
  #define LOG_INTERVAL_S       600      // 10 minutes (valeur par défaut du document)
  #define DUREE_APPUI_LONG_MS  5000UL   // appui de 5 secondes
  // Inverse les deux lignes si le rouge et le vert sont intervertis
  #define BTN_ROUGE_PRESSE()   GroveButton1_IsPressed()
  #define BTN_VERT_PRESSE()    GroveButton2_IsPressed()
#endif

#define TIMEOUT_CONFIG_MS    (30UL * 60UL * 1000UL)   // 30 min sans activité
#define AFFICHER_SUR_SERIE   1                        // 1 = affiche chaque ligne sur le PC

typedef enum {
  MODE_STANDARD,
  MODE_CONFIGURATION,
  MODE_MAINTENANCE,
  MODE_ECONOMIQUE
} Mode_t;

static Mode_t   mode_courant;       // mode actuel
static Mode_t   mode_precedent;     // mode à retrouver en quittant la maintenance
static uint32_t derniere_activite;  // dernier appui de bouton (pour le mode configuration)
static uint32_t derniere_mesure;    // instant de la dernière mesure

// Suivi d'un appui long sur un bouton
typedef struct {
  uint32_t debut;      // instant du début de l'appui
  bool     actif;      // bouton maintenu
  bool     declenche;  // appui long déjà signalé
} Appui_t;

static Appui_t appui_rouge, appui_vert;

// Initialise les boutons selon le mode de test
static void init_boutons(void)
{
#if TEST_CARTE_SEULE
  __HAL_RCC_GPIOC_CLK_ENABLE();               // horloge du port C
  GPIO_InitTypeDef b1 = {0};
  b1.Pin  = GPIO_PIN_13;                      // bouton bleu B1 de la carte
  b1.Mode = GPIO_MODE_INPUT;
  b1.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &b1);
#else
  GroveButton_Init();                         // Grove Dual Button
#endif
}

// Renvoie true UNE SEULE FOIS quand le bouton est maintenu assez longtemps
static bool appui_long(Appui_t *a, bool presse)
{
  uint32_t now = HAL_GetTick();
  if (!presse) {                              // relâché : remise à zéro
    a->actif = false;
    a->declenche = false;
    return false;
  }
  if (!a->actif) {                            // début de l'appui
    a->actif = true;
    a->debut = now;
    a->declenche = false;
    return false;
  }
  if (!a->declenche && (now - a->debut) >= DUREE_APPUI_LONG_MS) {
    a->declenche = true;                      // durée atteinte
    return true;
  }
  return false;
}

// Change de mode : couleur de la LED + message sur le PC
static void changer_mode(Mode_t nouveau)
{
  mode_courant = nouveau;
  derniere_activite = HAL_GetTick();

  switch (nouveau) {
    case MODE_STANDARD:      GroveRGB_SetColor(0, 255, 0);   Serial_Print("Mode STANDARD\r\n");      break;  // vert
    case MODE_CONFIGURATION: GroveRGB_SetColor(255, 180, 0); Serial_Print("Mode CONFIGURATION\r\n"); break;  // jaune
    case MODE_ECONOMIQUE:    GroveRGB_SetColor(0, 0, 255);   Serial_Print("Mode ECONOMIQUE\r\n");    break;  // bleu
    case MODE_MAINTENANCE:   GroveRGB_SetColor(255, 60, 0);  Serial_Print("Mode MAINTENANCE\r\n");   break;  // orange
  }
}

// Intervalle entre 2 mesures : doublé en mode économique (document du projet)
static uint32_t intervalle_ms(void)
{
  uint32_t ms = LOG_INTERVAL_S * 1000UL;
  return (mode_courant == MODE_ECONOMIQUE) ? ms * 2UL : ms;
}

// Fait UNE acquisition complète : une seule ligne horodatée
static void faire_mesure(void)
{
  static uint8_t compteur_gps = 0;
  char date[24], lumin[16], bme[48], gps[48];
  char ligne[200];

  SensorRTC_Acquire(date, sizeof(date));         // "AAAA-MM-JJ HH:MM:SS" ou "NA"
  SensorLight_Acquire(lumin, sizeof(lumin));     // 0-1023 ou "NA"
  SensorBME680_Acquire(bme, sizeof(bme));        // "temp;hygr;pression" ou "NA"

  // Mode économique : le GPS n'est lu qu'une fois sur deux
  if (mode_courant == MODE_ECONOMIQUE && (compteur_gps++ % 2U) == 1U) {
    snprintf(gps, sizeof(gps), "NA");
  } else {
    SensorGPS_Acquire(gps, sizeof(gps));         // "latitude;longitude" ou "NA"
  }

  snprintf(ligne, sizeof(ligne), "%s;%s;%s;%s\r\n", date, lumin, bme, gps);

  // Maintenance : rien n'est écrit sur la SD (on peut la retirer sans risque)
  if (mode_courant != MODE_MAINTENANCE) {
    SensorSD_WriteLine(ligne);
  }
#if AFFICHER_SUR_SERIE
  Serial_Print(ligne);                           // lecture en direct sur le port série
#endif
}

int main(void)
{
  Board_Init();         // HAL, horloge, liaison série
  Global_Init();        // I2C1 et SPI1 (src/setup.c)
  GroveRGB_Init();      // LED d'état
  init_boutons();       // B1 (test) ou Grove Dual Button (final)

  SensorRTC_Init();     // chaque capteur s'initialise lui-même
  SensorLight_Init();
  SensorBME680_Init();
  SensorGPS_Init();
  SensorSD_Init();
  // TODO : si un Init renvoie false, afficher le clignotement d'erreur de la LED

  Delay_ms(50);         // laisse les entrées se stabiliser

  // Démarrage : bouton rouge pressé = configuration, sinon standard
  if (BTN_ROUGE_PRESSE()) changer_mode(MODE_CONFIGURATION);
  else                    changer_mode(MODE_STANDARD);

  derniere_mesure = HAL_GetTick() - intervalle_ms();   // 1re mesure tout de suite

  while (1)
  {
    bool rouge = BTN_ROUGE_PRESSE();
    bool vert  = BTN_VERT_PRESSE();
    if (rouge || vert) derniere_activite = HAL_GetTick();

    bool rouge_long = appui_long(&appui_rouge, rouge);
    bool vert_long  = appui_long(&appui_vert, vert);

    switch (mode_courant)
    {
      case MODE_STANDARD:
        if (rouge_long)     { mode_precedent = MODE_STANDARD; changer_mode(MODE_MAINTENANCE); }
        else if (vert_long) { changer_mode(MODE_ECONOMIQUE); }
        break;

      case MODE_ECONOMIQUE:
        if (rouge_long) changer_mode(MODE_STANDARD);          // rouge long : retour au standard
        break;

      case MODE_MAINTENANCE:
        if (rouge_long) changer_mode(mode_precedent);         // rouge long : mode précédent
        break;

      case MODE_CONFIGURATION:
        // TODO : lire les commandes UART (LOG_INTERVAL=..., CLOCK=..., etc.)
        if ((HAL_GetTick() - derniere_activite) >= TIMEOUT_CONFIG_MS) {
          changer_mode(MODE_STANDARD);                        // 30 min sans activité
        }
        break;
    }

    // Mesures : pas en mode configuration (acquisition désactivée)
    if (mode_courant != MODE_CONFIGURATION &&
        (HAL_GetTick() - derniere_mesure) >= intervalle_ms())
    {
      derniere_mesure = HAL_GetTick();
      faire_mesure();
    }

    Delay_ms(20);       // lecture des boutons toutes les 20 ms
  }
}