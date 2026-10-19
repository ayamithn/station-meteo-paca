#include "sensor_gps.h"
#include "grove_gps_air530z.h"   // pilote du prof : GroveGPS_Init() et GroveGPS_ReadGGA()
#include "board_pins.h"
#include "modes.h"               // Modes_Config()->timeout_s (paramètre TIMEOUT) et Modes_Update()
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <string.h>

/*
 * PRINCIPE (à expliquer à l'oral) :
 * Le GPS envoie en continu des lignes de texte (trames NMEA). Celle qui contient
 * la position s'appelle GGA. Le pilote du prof sait la lire (GroveGPS_ReadGGA),
 * mais il ne vérifie pas qu'elle est correcte. Ce fichier ajoute :
 *   1. l'attente d'un "fix" (position valide) pendant TIMEOUT secondes au maximum
 *   2. la vérification de la trame (checksum + coordonnées plausibles)
 *   3. la mise en forme "latitude,longitude", ou "NA" en cas d'échec
 * (code de Pierre ; Modes_Update() ajouté dans la boucle d'attente)
 */

// Le pilote du prof définit ce handle (même principe que GROVE_I2C_HANDLE pour l'I2C)
extern UART_HandleTypeDef GPS_UART_HANDLE;


/* ---------- Vérification d'une trame ---------- */

static int valeur_hexa(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Une trame NMEA se termine par "*XY" : XY = XOR (en hexadécimal) de tout ce qui
// est entre '$' et '*'. Si le résultat est différent, la trame est abîmée.
static bool checksum_correct(const char *trame)
{
  if (trame[0] != '$') return false;

  uint8_t somme = 0;
  const char *p = trame + 1;

  while (*p != '\0' && *p != '*')
  {
    somme ^= (uint8_t)*p;
    p++;
  }
  if (*p != '*') return false;

  int haut = valeur_hexa(p[1]);
  int bas  = (haut < 0) ? -1 : valeur_hexa(p[2]);
  if (haut < 0 || bas < 0) return false;

  return somme == (uint8_t)((haut << 4) | bas);
}

static bool position_plausible(const GPS_Data *gps)
{
  return gps->latitude  >= -90.0  && gps->latitude  <= 90.0 &&
         gps->longitude >= -180.0 && gps->longitude <= 180.0;
}


/* ---------- Mise en forme ---------- */

// Écrit un angle avec 6 décimales (ex : -6.505620) sans utiliser "%f" de snprintf
// (absent de la bibliothèque allégée sur STM32, comme pour le BME680).
static void angle_en_texte(double valeur, char *out, size_t taille)
{
  double absolu = (valeur < 0.0) ? -valeur : valeur;
  int32_t micro = (int32_t)(absolu * 1000000.0 + 0.5);   // millionièmes de degré
  const char *signe = (valeur < 0.0 && micro != 0) ? "-" : "";

  snprintf(out, taille, "%s%ld.%06ld", signe, (long)(micro / 1000000), (long)(micro % 1000000));
}

// Vide l'UART avant d'écouter : efface un éventuel dépassement (octets perdus
// pendant les minutes sans lecture) et jette l'octet qui attendait.
static void vider_uart(void)
{
  uint8_t c;

  __HAL_UART_CLEAR_OREFLAG(&GPS_UART_HANDLE);
  while (HAL_UART_Receive(&GPS_UART_HANDLE, &c, 1, 0) == HAL_OK)
  {
    // octet ignoré
  }
  __HAL_UART_CLEAR_OREFLAG(&GPS_UART_HANDLE);
}


/* ---------- Fonctions publiques ---------- */

bool SensorGPS_Init(void)
{
  GroveGPS_Init();   // configure UART4 ; les broches sont réglées dans setup.c (HAL_UART_MspInit)
  return true;       // le pilote du prof ne renvoie pas de résultat : on ne peut pas vérifier ici
}

// Écrit "latitude,longitude" dans out, ou "NA" si pas de position avant le TIMEOUT
bool SensorGPS_Acquire(char *out, size_t size)
{
  if (out == NULL || size == 0) return false;

  uint32_t timeout_ms = (uint32_t)Modes_Config()->timeout_s * 1000UL;
  uint32_t debut = HAL_GetTick();
  GPS_Data gps;

  vider_uart();

  // On lit des trames GGA jusqu'à en obtenir une valide, ou jusqu'au TIMEOUT.
  // (Sans fix, GroveGPS_ReadGGA renvoie false : on recommence avec la trame suivante.)
  while (1)
  {
    uint32_t ecoule = HAL_GetTick() - debut;
    if (ecoule >= timeout_ms) break;

    Modes_Update();   // garde les boutons, la LED et la console actifs pendant l'attente

    memset(&gps, 0, sizeof(gps));

    if (GroveGPS_ReadGGA(&gps, timeout_ms - ecoule) &&
        gps.valid &&
        checksum_correct(gps.raw_sentence) &&
        position_plausible(&gps))
    {
      char lat[16], lon[16];
      angle_en_texte(gps.latitude, lat, sizeof(lat));
      angle_en_texte(gps.longitude, lon, sizeof(lon));

      int n = snprintf(out, size, "%s,%s", lat, lon);
      if (n > 0 && (size_t)n < size) return true;
      break;   // buffer trop petit : on le signale par "NA"
    }
  }

  snprintf(out, size, "NA");
  return false;
}