#ifndef STATUS_LED_H
#define STATUS_LED_H


typedef enum {
  LED_STANDARD,
  LED_CONFIGURATION,
  LED_ECONOMIQUE,
  LED_MAINTENANCE,
  LED_ERR_RTC,
  LED_ERR_GPS,
  LED_ERR_CAPTEUR,
  LED_ERR_INCOHERENT,
  LED_ERR_SD_PLEINE,
  LED_ERR_SD_ECRITURE
} LedEtat_t;

void StatusLed_Init(void);
void StatusLed_Set(LedEtat_t etat);
void StatusLed_Update(void);

#endif