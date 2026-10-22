#include "board.h"
#include "setup.h"
#include "modes.h"
#include "sensor_rtc.h"
#include "sensor_gps.h"
#include "sensor_bme680.h"
#include "sensor_light.h"
#include "sensor_sd.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * MAIN : boucle principale de la station (document "Modes de fonctionnement")
 *
 *  - STANDARD       : mesure tous les LOG_INTERVAL, ligne horodatée -> carte SD + port série
 *  - CONFIGURATION  : pas de mesure, la console est gérée par modes.c
 *  - MAINTENANCE    : mesure + affichage série, RIEN n'est écrit sur la SD
 *  - ECONOMIQUE     : intervalle x2 et GPS une mesure sur deux (géré par modes.c)
 *
 * Format d'une ligne (séparateur ';', une ligne par mesure) :
 *   AAAA-MM-JJ HH:MM:SS;latitude,longitude;température;humidité;pression;luminosité
 * Une valeur absente ou en erreur est écrite "NA".
 */

#define BME_NA   "NA"

// Limites physiques du BME680 : au-delà, la valeur est incohérente (câblage / capteur à vérifier)
#define BME_TEMP_MIN   (-40.0f)
#define BME_TEMP_MAX   (85.0f)
#define BME_HUM_MIN    (0.0f)
#define BME_HUM_MAX    (100.0f)
#define BME_PRES_MIN   (300.0f)
#define BME_PRES_MAX   (1100.0f)

static const char *niveau_lumiere(int valeur, const Config_t *c)
{
  if (valeur < c->lumin_low)  return "FAIBLE";
  if (valeur > c->lumin_high) return "FORTE";
  return "MOYENNE";
}

// Une mesure complète : lit les capteurs, fabrique la ligne, l'affiche et l'enregistre
static void cycle_mesure(void)
{
  const Config_t *c = Modes_Config();

  char date[24]  = "NA";
  char gps[40]   = "NA";
  char temp[16]  = BME_NA, hum[16] = BME_NA, pres[16] = BME_NA;
  char lum[16]   = "NA";
  char ligne[160];
  char msg[200];

  bool rtc_ok = true, gps_ok = true, bme_ok = true, lum_ok = true;
  bool incoherent = false;
  bool sd_ok = true;
  bool sd_ecrite = false;

  // 1. Horodatage (RTC) : "NA" si la RTC ne répond pas
  rtc_ok = SensorRTC_Acquire(date, sizeof(date));

  // 2. GPS : en mode économique, une mesure sur deux seulement.
  //    Quand le GPS est sauté, ce n'est pas une erreur : la valeur est "NA".
  if (Modes_GpsAutorise()) gps_ok = SensorGPS_Acquire(gps, sizeof(gps));

  // 3. BME680 (température, humidité, pression), selon les paramètres activés
  if (c->temp_air || c->hygr || c->pressure) {
    Env_Data e;
    if (SensorBME680_Read(&e)) {
      if (c->temp_air) {
        SensorBME680_ValeurEnTexte(e.temperature_c, temp, sizeof(temp));
        if (e.temperature_c < BME_TEMP_MIN || e.temperature_c > BME_TEMP_MAX ||
            e.temperature_c < (float)c->min_temp_air || e.temperature_c > (float)c->max_temp_air)
          incoherent = true;
      }
      // Hygrométrie ignorée si la température est hors de [HYGR_MINT ; HYGR_MAXT]
      if (c->hygr && e.temperature_c >= (float)c->hygr_mint && e.temperature_c <= (float)c->hygr_maxt) {
        SensorBME680_ValeurEnTexte(e.humidity_percent, hum, sizeof(hum));
        if (e.humidity_percent < BME_HUM_MIN || e.humidity_percent > BME_HUM_MAX) incoherent = true;
      }
      if (c->pressure) {
        SensorBME680_ValeurEnTexte(e.pressure_hpa, pres, sizeof(pres));
        if (e.pressure_hpa < BME_PRES_MIN || e.pressure_hpa > BME_PRES_MAX ||
            e.pressure_hpa < (float)c->pressure_min || e.pressure_hpa > (float)c->pressure_max)
          incoherent = true;
      }
    } else {
      bme_ok = false;   // les 3 valeurs restent "NA"
    }
  }

  // 4. Luminosité
  if (c->lumin) {
    lum_ok = SensorLight_Acquire(lum, sizeof(lum));
    if (lum_ok) {
      int v = atoi(lum);
      snprintf(lum, sizeof(lum), "%d", v);
    }
  }

  // 5. Une seule ligne horodatée
  snprintf(ligne, sizeof(ligne), "%s;%s;%s;%s;%s;%s\r\n", date, gps, temp, hum, pres, lum);

  // 6. Affichage série (tous les modes sauf configuration, qui ne passe jamais ici)
  Serial_Print(ligne);
  if (c->lumin && lum_ok) {
    snprintf(msg, sizeof(msg), "  Luminosite : %s\r\n", niveau_lumiere(atoi(lum), c));
    Serial_Print(msg);
  }

  // 7. Enregistrement SD : interdit en maintenance (la carte peut être retirée)
  if (Modes_SdAutorisee()) {
    // sur la carte, la ligne se termine par '\n' (le "\r" n'est utile que sur le terminal)
    char ligne_sd[160];
    snprintf(ligne_sd, sizeof(ligne_sd), "%s;%s;%s;%s;%s;%s\n", date, gps, temp, hum, pres, lum);
    sd_ok = SensorSD_WriteLine(ligne_sd);
    sd_ecrite = sd_ok;
    if (!sd_ok) Serial_Print("  ERREUR : ecriture SD impossible\r\n");
  } else {
    Serial_Print("  (maintenance : rien n'est ecrit sur la SD)\r\n");
  }
  (void)sd_ecrite;

  // 8. Signalisation LED des erreurs (le document : erreur après 2 échecs de suite, géré dans modes.c)
  Modes_SuivreErreurs(rtc_ok, gps_ok, bme_ok && lum_ok, incoherent, sd_ok);
}

int main(void)
{
  Board_Init();      // HAL, UART2 (console PC)
  Global_Init();     // I2C1 (RTC, BME680) et SPI1 (SD)
  Modes_Init();      // paramètres, LED, boutons ; bouton rouge pressé = configuration

  bool rtc  = SensorRTC_Init();
  if (!rtc) SensorRTC_SetFromBuild(__DATE__, __TIME__);   // pas de RTC : heure de compilation
  bool bme  = SensorBME680_Init();
  bool lum  = SensorLight_Init();
  bool gps  = SensorGPS_Init();
  bool sd   = SensorSD_Init();

  Serial_Print("\r\n=== Station meteo PACA ===\r\n");
  Serial_Print(rtc ? "RTC : OK\r\n" : "RTC : absente -> heure de compilation\r\n");
  Serial_Print(bme ? "BME680 : OK\r\n" : "BME680 : ERREUR\r\n");
  Serial_Print(lum ? "Lumiere : OK\r\n" : "Lumiere : ERREUR\r\n");
  Serial_Print(gps ? "GPS : initialise\r\n" : "GPS : ERREUR\r\n");
  Serial_Print(sd  ? "SD : OK\r\n" : "SD : absente ou illisible (nouvel essai a chaque mesure)\r\n");
  Serial_Print("Format : date;lat,lon;temp;hum;pression;luminosite\r\n");

  uint32_t derniere_mesure = 0;
  bool     mesure_faite    = false;
  Mode_t   mode_vu         = Modes_Get();

  while (1)
  {
    Modes_Update();                      // boutons, changements de mode, console, LED

    Mode_t mode = Modes_Get();
    if (mode != mode_vu) {               // changement de mode : on repart d'une mesure immédiate
      mode_vu = mode;
      mesure_faite = false;
    }

    if (!Modes_AcquisitionActive()) {    // mode configuration : capteurs désactivés
      mesure_faite = false;
      continue;
    }

    if (!mesure_faite || (HAL_GetTick() - derniere_mesure) >= Modes_IntervalleMs()) {
      cycle_mesure();                    // peut durer jusqu'à TIMEOUT s (GPS)
      derniere_mesure = HAL_GetTick();
      mesure_faite = true;
    }
  }
}