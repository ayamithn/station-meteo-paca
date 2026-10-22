#include "sensor_rtc.h"
#include "grove_rtc_ds1307.h"   // pilote du prof : GroveRTC_Init / GetDateTime / SetDateTime
#include "stm32l4xx_hal.h"      // HAL_GetTick (horloge logicielle)
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------
// 1. Lecture sécurisée : on lit la RTC ET on vérifie que la date a du sens
//    (une RTC débranchée ou jamais réglée peut renvoyer n'importe quoi)
// ---------------------------------------------------------------
static bool champs_valides(const RTC_DateTime *t)
{
  // Chaque valeur doit rester dans sa plage normale
  return (t->seconds <= 59) && (t->minutes <= 59) && (t->hours <= 23) &&
         (t->day   >= 1) && (t->day   <= 31) &&
         (t->month >= 1) && (t->month <= 12) &&
         (t->year  >= 2000) && (t->year <= 2099);
}

static bool lire_materiel_valide(RTC_DateTime *t)
{
  if (!GroveRTC_GetDateTime(t))      // la RTC ne répond pas sur l'I2C
    return false;
  return champs_valides(t);
}

// ---------------------------------------------------------------
// 1b. Horloge logicielle : sert quand la RTC matérielle est absente.
//     Réglée au démarrage avec l'heure de compilation, puis avancée avec HAL_GetTick().
//     Elle repart de l'heure de compilation à chaque redémarrage.
// ---------------------------------------------------------------
static bool     logiciel_regle = false;
static uint32_t secondes_base  = 0;     // secondes depuis 1970 à l'instant tick_base
static uint32_t tick_base      = 0;

// Nombre de jours entre le 1970-01-01 et la date donnée (calcul du calendrier)
static int32_t jours_depuis_1970(int y, int m, int d)
{
  y -= (m <= 2);
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  int32_t yoe = y - era * 400;
  int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static void date_depuis_jours(int32_t z, uint16_t *y, uint8_t *m, uint8_t *d)
{
  z += 719468;
  int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  int32_t doe = z - era * 146097;
  int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int32_t mp  = (5 * doy + 2) / 153;
  int32_t jour = doy - (153 * mp + 2) / 5 + 1;
  int32_t mois = mp < 10 ? mp + 3 : mp - 9;
  *y = (uint16_t)(yoe + era * 400 + (mois <= 2));
  *m = (uint8_t)mois;
  *d = (uint8_t)jour;
}

static void logiciel_regler(const RTC_DateTime *t)
{
  int32_t jours = jours_depuis_1970(t->year, t->month, t->day);
  secondes_base = (uint32_t)jours * 86400UL + t->hours * 3600UL + t->minutes * 60UL + t->seconds;
  tick_base     = HAL_GetTick();
  logiciel_regle = true;
}

static bool logiciel_lire(RTC_DateTime *t)
{
  if (!logiciel_regle) return false;

  // On fait avancer la base par secondes entières : pas de dérive, même après 49 jours
  uint32_t ecoule_s = (HAL_GetTick() - tick_base) / 1000UL;
  secondes_base += ecoule_s;
  tick_base     += ecoule_s * 1000UL;

  uint32_t jours = secondes_base / 86400UL;
  uint32_t reste = secondes_base % 86400UL;

  date_depuis_jours((int32_t)jours, &t->year, &t->month, &t->day);
  t->hours     = (uint8_t)(reste / 3600UL);
  t->minutes   = (uint8_t)((reste % 3600UL) / 60UL);
  t->seconds   = (uint8_t)(reste % 60UL);
  t->dayOfWeek = (uint8_t)(((jours + 3U) % 7U) + 1U);   // 1 = lundi ... 7 = dimanche (1970-01-01 = jeudi)
  return true;
}

// Heure du système : la RTC matérielle si elle répond, sinon l'horloge logicielle
bool SensorRTC_GetDateTime(RTC_DateTime *t)
{
  if (lire_materiel_valide(t)) return true;
  return logiciel_lire(t);
}

bool SensorRTC_SetDateTime(const RTC_DateTime *t)
{
  if (t == NULL || !champs_valides(t)) return false;
  logiciel_regler(t);                       // toujours : sert de secours si la RTC disparaît
  (void)GroveRTC_SetDateTime(t);            // sans RTC, l'échec est normal
  return true;
}

// Règle l'horloge logicielle avec l'heure de compilation : SetFromBuild(__DATE__, __TIME__)
// __DATE__ = "Oct 22 2026", __TIME__ = "15:44:00"
void SensorRTC_SetFromBuild(const char *date, const char *heure)
{
  static const char mois[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char nom[4] = {0};
  unsigned jour, annee, h, mi, s;

  if (sscanf(date, "%3s %u %u", nom, &jour, &annee) != 3) return;
  if (sscanf(heure, "%u:%u:%u", &h, &mi, &s) != 3) return;

  const char *p = strstr(mois, nom);
  if (p == NULL) return;

  RTC_DateTime t;
  t.year = (uint16_t)annee; t.month = (uint8_t)((p - mois) / 3 + 1); t.day = (uint8_t)jour;
  t.hours = (uint8_t)h; t.minutes = (uint8_t)mi; t.seconds = (uint8_t)s;
  t.dayOfWeek = (uint8_t)(((jours_depuis_1970(t.year, t.month, t.day) + 3) % 7 + 7) % 7 + 1);
  SensorRTC_SetDateTime(&t);
}

// ---------------------------------------------------------------
// 2. Initialisation (appelée une fois au démarrage dans main)
// ---------------------------------------------------------------
bool SensorRTC_Init(void)
{
  RTC_DateTime t;

  if (!GroveRTC_Init())              // 1) la RTC répond-elle à son adresse I2C ?
    return false;

  if (!SensorRTC_GetDateTime(&t))         // 2) la date lue est-elle cohérente ?
    return false;

  // 3) Le DS1307 peut être arrêté (bit CH) après une coupure d'alimentation.
  //    Réécrire l'heure lue remet CH à 0 : l'horloge repart.
  return GroveRTC_SetDateTime(&t);
}

// ---------------------------------------------------------------
// 3. Acquisition pour le mode standard : texte horodaté dans out
// ---------------------------------------------------------------
bool SensorRTC_Acquire(char *out, size_t size)
{
  RTC_DateTime t;

  if (!SensorRTC_GetDateTime(&t)) {
    snprintf(out, size, "NA");       // le sujet impose "NA" si le capteur ne répond pas
    return false;                    // main.c compte les échecs pour la LED rouge/bleu
  }

  snprintf(out, size, "%04u-%02u-%02u %02u:%02u:%02u",
           (unsigned)t.year, (unsigned)t.month, (unsigned)t.day,
           (unsigned)t.hours, (unsigned)t.minutes, (unsigned)t.seconds);
  return true;
}

// ---------------------------------------------------------------
// 4. Date pour le nom du fichier : AAMMJJ (ex : 261019 -> 261019_0.CSV)
//    Sert à celui qui code la carte SD
// ---------------------------------------------------------------
bool SensorRTC_FileDate(char *out, size_t size)
{
  RTC_DateTime t;

  if (!SensorRTC_GetDateTime(&t)) {
    snprintf(out, size, "NA");
    return false;
  }

  snprintf(out, size, "%02u%02u%02u",
           (unsigned)(t.year % 100), (unsigned)t.month, (unsigned)t.day);
  return true;
}