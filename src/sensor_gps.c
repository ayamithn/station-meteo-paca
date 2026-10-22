#include "sensor_gps.h"
#include "grove_gps_air530z.h"
#include "board_pins.h"
#include "modes.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <string.h>




extern UART_HandleTypeDef GPS_UART_HANDLE;




static int valeur_hexa(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}



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






static void angle_en_texte(double valeur, char *out, size_t taille)
{
  double absolu = (valeur < 0.0) ? -valeur : valeur;
  int32_t micro = (int32_t)(absolu * 1000000.0 + 0.5);
  const char *signe = (valeur < 0.0 && micro != 0) ? "-" : "";

  snprintf(out, taille, "%s%ld.%06ld", signe, (long)(micro / 1000000), (long)(micro % 1000000));
}



static void vider_uart(void)
{
  uint8_t c;

  __HAL_UART_CLEAR_OREFLAG(&GPS_UART_HANDLE);
  while (HAL_UART_Receive(&GPS_UART_HANDLE, &c, 1, 0) == HAL_OK)
  {

  }
  __HAL_UART_CLEAR_OREFLAG(&GPS_UART_HANDLE);
}




bool SensorGPS_Init(void)
{
  GroveGPS_Init();
  return true;
}


bool SensorGPS_Acquire(char *out, size_t size)
{
  if (out == NULL || size == 0) return false;

  uint32_t timeout_ms = (uint32_t)Modes_Config()->timeout_s * 1000UL;
  uint32_t debut = HAL_GetTick();
  GPS_Data gps;

  vider_uart();



  while (1)
  {
    uint32_t ecoule = HAL_GetTick() - debut;
    if (ecoule >= timeout_ms) break;

    Modes_Update();

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
      break;
    }
  }

  snprintf(out, size, "NA");
  return false;
}