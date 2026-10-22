#ifndef MODES_H
#define MODES_H

#include <stdbool.h>
#include <stdint.h>
#include "status_led.h"

typedef enum {
  MODE_STANDARD,
  MODE_CONFIGURATION,
  MODE_MAINTENANCE,
  MODE_ECONOMIQUE
} Mode_t;


typedef struct {
  int32_t log_interval_s;
  int32_t file_max_size;
  int32_t timeout_s;
  int32_t lumin, lumin_low, lumin_high;
  int32_t temp_air, min_temp_air, max_temp_air;
  int32_t hygr, hygr_mint, hygr_maxt;
  int32_t pressure, pressure_min, pressure_max;
} Config_t;

void     Modes_Init(void);
void     Modes_Update(void);
Mode_t   Modes_Get(void);
bool     Modes_AcquisitionActive(void);
bool     Modes_SdAutorisee(void);
const Config_t *Modes_Config(void);
uint32_t Modes_IntervalleMs(void);
bool     Modes_GpsAutorise(void);


void Modes_SuivreErreurs(bool rtc_ok, bool gps_ok, bool capteurs_ok, bool incoherent, bool sd_ok);
void Modes_SetErreur(LedEtat_t err);
void Modes_ClearErreur(void);

#endif