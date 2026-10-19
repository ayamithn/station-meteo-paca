#include "sensor_rtc.h"
#include "grove_rtc_ds1307.h"   // pilote du prof : GroveRTC_Init / GetDateTime / SetDateTime
#include <stdio.h>

// ---------------------------------------------------------------
// 1. Lecture sécurisée : on lit la RTC ET on vérifie que la date a du sens
//    (une RTC débranchée ou jamais réglée peut renvoyer n'importe quoi)
// ---------------------------------------------------------------
static bool lire_date_valide(RTC_DateTime *t)
{
  if (!GroveRTC_GetDateTime(t))      // la RTC ne répond pas sur l'I2C
    return false;

  // Chaque valeur doit rester dans sa plage normale
  return (t->seconds <= 59) && (t->minutes <= 59) && (t->hours <= 23) &&
         (t->day   >= 1) && (t->day   <= 31) &&
         (t->month >= 1) && (t->month <= 12) &&
         (t->year  >= 2000) && (t->year <= 2099);
}

// ---------------------------------------------------------------
// 2. Initialisation (appelée une fois au démarrage dans main)
// ---------------------------------------------------------------
bool SensorRTC_Init(void)
{
  RTC_DateTime t;

  if (!GroveRTC_Init())              // 1) la RTC répond-elle à son adresse I2C ?
    return false;

  if (!lire_date_valide(&t))         // 2) la date lue est-elle cohérente ?
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

  if (!lire_date_valide(&t)) {
    snprintf(out, size, "NA");       // le sujet impose "NA" si le capteur ne répond pas
    return false;                    // main.c compte les échecs pour la LED rouge/bleu
  }

  snprintf(out, size, "%04u-%02u-%02u %02u:%02u:%02u",
           (unsigned)t.year, (unsigned)t.month, (unsigned)t.day,
           (unsigned)t.hours, (unsigned)t.minutes, (unsigned)t.seconds);
  return true;
}

// ---------------------------------------------------------------
// 4. Date pour le nom du fichier : AAMMJJ (ex : 261019 -> 261019_0.LOG)
//    Sert à celui qui code la carte SD
// ---------------------------------------------------------------
bool SensorRTC_FileDate(char *out, size_t size)
{
  RTC_DateTime t;

  if (!lire_date_valide(&t)) {
    snprintf(out, size, "NA");
    return false;
  }

  snprintf(out, size, "%02u%02u%02u",
           (unsigned)(t.year % 100), (unsigned)t.month, (unsigned)t.day);
  return true;
}