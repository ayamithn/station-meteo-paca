#include "board.h"          // Board_Init, Serial_Print, Delay_ms
#include "setup.h"          // Global_Init : I2C1 et SPI1
#include "modes.h"          // boutons, modes, LED, paramètres
#include "sensor_rtc.h"
#include "sensor_light.h"
#include "sensor_bme680.h"
#include "sensor_gps.h"
#include "sensor_sd.h"
#include "stm32l4xx_hal.h"  // HAL_GetTick
#include <stdio.h>

// 1 = la LED affiche les erreurs des capteurs (version finale)
#define SIGNALER_ERREURS  1
// Passe à 1 quand sensor_sd.c écrit vraiment sur la carte (sinon la LED signalerait une erreur SD en permanence)
#define SD_IMPLEMENTEE    0

// Une acquisition complète : lit les capteurs et enregistre UNE ligne horodatée
static void faire_mesure(void)
{
  const Config_t *cfg = Modes_Config();
  char date[24], lumin[16], temp[16], hum[16], pres[16], gps[48], ligne[200];
  bool capteurs_ok = true;      // false si un capteur activé n'a pas répondu
  bool incoherent  = false;     // true si une valeur sort des seuils des paramètres

  // Par défaut : "NA" (capteur désactivé, ou valeur non prise en compte)
  snprintf(lumin, sizeof(lumin), "NA");
  snprintf(temp,  sizeof(temp),  "NA");
  snprintf(hum,   sizeof(hum),   "NA");
  snprintf(pres,  sizeof(pres),  "NA");

  // 1. Date et heure
  bool rtc_ok = SensorRTC_Acquire(date, sizeof(date));

  // 2. Luminosité (paramètre LUMIN)
  if (cfg->lumin && !SensorLight_Acquire(lumin, sizeof(lumin))) capteurs_ok = false;

  // 3. BME680 : température, humidité, pression (paramètres TEMP_AIR, HYGR, PRESSURE)
  if (cfg->temp_air || cfg->hygr || cfg->pressure) {
    Env_Data env;
    if (SensorBME680_Read(&env)) {
      if (cfg->temp_air) {
        SensorBME680_ValeurEnTexte(env.temperature_c, temp, sizeof(temp));
        if (env.temperature_c < (float)cfg->min_temp_air ||
            env.temperature_c > (float)cfg->max_temp_air) incoherent = true;
      }
      // Hygrométrie : ignorée hors de la plage de température HYGR_MINT..HYGR_MAXT
      if (cfg->hygr &&
          env.temperature_c >= (float)cfg->hygr_mint &&
          env.temperature_c <= (float)cfg->hygr_maxt) {
        SensorBME680_ValeurEnTexte(env.humidity_percent, hum, sizeof(hum));
      }
      if (cfg->pressure) {
        SensorBME680_ValeurEnTexte(env.pressure_hpa, pres, sizeof(pres));
        if (env.pressure_hpa < (float)cfg->pressure_min ||
            env.pressure_hpa > (float)cfg->pressure_max) incoherent = true;
      }
    } else {
      capteurs_ok = false;
    }
  }

  // 4. GPS (une lecture sur deux en mode économique)
  bool gps_ok = true;
  if (Modes_GpsAutorise()) gps_ok = SensorGPS_Acquire(gps, sizeof(gps));
  else                     snprintf(gps, sizeof(gps), "NA");

  // 5. Une seule ligne avec toutes les mesures
  snprintf(ligne, sizeof(ligne), "%s;%s;%s;%s;%s;%s\r\n", date, lumin, temp, hum, pres, gps);

  // 6. Enregistrement sur la SD (interdit en mode maintenance) et affichage sur le PC
  bool sd_ok = true;
  if (Modes_SdAutorisee()) {
    bool ecrit = SensorSD_WriteLine(ligne);
    if (SD_IMPLEMENTEE) sd_ok = ecrit;
  }
  Serial_Print(ligne);

  // 7. LED d'erreur
  if (SIGNALER_ERREURS) Modes_SuivreErreurs(rtc_ok, gps_ok, capteurs_ok, incoherent, sd_ok);
}

int main(void)
{
  Board_Init();            // HAL, horloge, liaison série
  Global_Init();           // I2C1 et SPI1

  SensorRTC_Init();        // chaque capteur s'initialise lui-même
  SensorLight_Init();
  SensorBME680_Init();
  SensorGPS_Init();
  SensorSD_Init();

  Modes_Init();            // paramètres (Flash ou défauts), LED, boutons, mode de départ

  uint32_t derniere = HAL_GetTick() - Modes_IntervalleMs();   // 1re mesure tout de suite

  while (1)
  {
    Modes_Update();        // boutons, changements de mode, console, LED

    if (Modes_AcquisitionActive() &&
        (HAL_GetTick() - derniere) >= Modes_IntervalleMs())
    {
      derniere = HAL_GetTick();
      faire_mesure();
    }

    Delay_ms(Modes_Get() == MODE_CONFIGURATION ? 1 : 20);   // écoute plus vite la console en configuration
  }
}