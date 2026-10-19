#ifndef STATUS_LED_H
#define STATUS_LED_H

// Tous les états que la LED peut afficher (codes du document du projet)
typedef enum {
  LED_STANDARD,          // vert continu
  LED_CONFIGURATION,     // jaune continu
  LED_ECONOMIQUE,        // bleu continu
  LED_MAINTENANCE,       // orange continu
  LED_ERR_RTC,           // rouge / bleu, 1 Hz, durées égales
  LED_ERR_GPS,           // rouge / jaune, 1 Hz, durées égales
  LED_ERR_CAPTEUR,       // rouge / vert, 1 Hz, durées égales
  LED_ERR_INCOHERENT,    // rouge / vert, 1 Hz, vert 2 fois plus long
  LED_ERR_SD_PLEINE,     // rouge / blanc, 1 Hz, durées égales
  LED_ERR_SD_ECRITURE    // rouge / blanc, 1 Hz, blanc 2 fois plus long
} LedEtat_t;

void StatusLed_Init(void);
void StatusLed_Set(LedEtat_t etat);   // choisit l'état à afficher
void StatusLed_Update(void);          // à appeler souvent : gère le clignotement sans bloquer

#endif