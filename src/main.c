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



#define BME_NA   "NA"


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


static void cycle_mesure(void)
{
  const Config_t *c = Modes_Config();

  char date[24]  = "NA";
  char gps[40]   = "NA";
  char temp[16]  = BME_NA, hum[16] = BME_NA, pres[16] = BME_NA;
  char gaz[16]   = BME_NA;
  char lum[16]   = "NA";
  char ligne[192];

  bool rtc_ok = true, gps_ok = true, bme_ok = true, lum_ok = true;
  bool incoherent = false;
  bool sd_ok = true;
  bool sd_ecrite = false;


  rtc_ok = SensorRTC_Acquire(date, sizeof(date));



  if (Modes_GpsAutorise()) gps_ok = SensorGPS_Acquire(gps, sizeof(gps));


  if (c->temp_air || c->hygr || c->pressure) {
    Env_Data e;
    if (SensorBME680_Read(&e)) {
      if (c->temp_air) {
        SensorBME680_ValeurEnTexte(e.temperature_c, temp, sizeof(temp));
        if (e.temperature_c < BME_TEMP_MIN || e.temperature_c > BME_TEMP_MAX ||
            e.temperature_c < (float)c->min_temp_air || e.temperature_c > (float)c->max_temp_air)
          incoherent = true;
      }

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
      if (e.gas_resistance_ohm > 0.0f)
        SensorBME680_ValeurEnTexte(e.gas_resistance_ohm, gaz, sizeof(gaz));
    } else {
      bme_ok = false;
    }
  }


  if (c->lumin) {
    lum_ok = SensorLight_Acquire(lum, sizeof(lum));
    if (lum_ok) {
      int v = atoi(lum);
      snprintf(lum, sizeof(lum), "%d", v);
    }
  }


  snprintf(ligne, sizeof(ligne), "%s;%s;%s;%s;%s;%s;%s\r\n",
           date, gps, temp, hum, pres, gaz, lum);


  Serial_Print(ligne);



  if (Modes_SdAutorisee()) {

    char ligne_sd[192];
    snprintf(ligne_sd, sizeof(ligne_sd), "%s;%s;%s;%s;%s;%s;%s\n",
             date, gps, temp, hum, pres, gaz, lum);
    sd_ok = SensorSD_WriteLine(ligne_sd);
    sd_ecrite = sd_ok;
    if (!sd_ok) Serial_Print("  ERREUR : ecriture SD impossible\r\n");
  } else {
    Serial_Print("  (maintenance : rien n'est ecrit sur la SD)\r\n");
  }
  (void)sd_ecrite;


  Modes_SuivreErreurs(rtc_ok, gps_ok, bme_ok && lum_ok, incoherent, sd_ok);
}

int main(void)
{
  Board_Init();
  Global_Init();
  Modes_Init();

  bool rtc  = SensorRTC_Init();
  if (!rtc) SensorRTC_SetFromBuild(__DATE__, __TIME__);
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
  Serial_Print("Format : date;lat,lon;temp;hum;pression;gaz;luminosite\r\n");

  uint32_t derniere_mesure = 0;
  bool     mesure_faite    = false;
  Mode_t   mode_vu         = Modes_Get();

  while (1)
  {
    Modes_Update();

    Mode_t mode = Modes_Get();
    if (mode != mode_vu) {
      mode_vu = mode;
      mesure_faite = false;
    }

    if (!Modes_AcquisitionActive()) {
      mesure_faite = false;
      continue;
    }

    if (!mesure_faite || (HAL_GetTick() - derniere_mesure) >= Modes_IntervalleMs()) {
      cycle_mesure();
      derniere_mesure = HAL_GetTick();
      mesure_faite = true;
    }
  }
}