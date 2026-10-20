// TEST DE LA CARTE SD UNIQUEMENT (aucun autre capteur, aucun mode, aucune ecriture sur la carte)

#include "board.h"          // Board_Init, Serial_Print
#include "setup.h"          // Global_Init : configure SPI1 (et I2C1)
#include "sd_logger.h"      // SDLogger_Init, SDLogger_BitBangCMD0Test...
#include "stm32l4xx_hal.h"  // HAL_Delay
#include <stdio.h>

static const char *nom_erreur(SDLoggerInitStatus st)
{
  switch (st)
  {
    case SDLOGGER_INIT_OK:                 return "OK";
    case SDLOGGER_INIT_ERR_CMD0:           return "CMD0 : la carte ne repond pas (carte absente ? cablage ?)";
    case SDLOGGER_INIT_ERR_CMD8_PATTERN:   return "CMD8 : reponse incorrecte (carte non compatible / signal bruite)";
    case SDLOGGER_INIT_ERR_ACMD41_TIMEOUT: return "ACMD41 : la carte n'a pas fini son initialisation (timeout)";
    case SDLOGGER_INIT_ERR_CMD58:          return "CMD58 : lecture du registre OCR impossible";
    case SDLOGGER_INIT_ERR_CMD1_TIMEOUT:   return "CMD1 : initialisation carte ancienne (MMC) en timeout";
    default:                               return "erreur inconnue";
  }
}

int main(void)
{
  char msg[160];
  unsigned int essai = 0;

  Board_Init();                       // HAL, horloge, liaison serie (115200 bauds)
  Global_Init();                      // SPI1 : PA5 = SCK, PA6 = MISO, PA7 = MOSI, CS = PB5 (D4)
  setvbuf(stdout, NULL, _IONBF, 0);   // le printf() du pilote SD s'affiche immediatement

  Serial_Print("\r\n=== TEST CARTE SD (SPI1, CS = D4/PB5) ===\r\n");
  Serial_Print("Carte inseree AVANT la mise sous tension. Rien n'est ecrit sur la carte.\r\n");

  while (1)
  {
    essai++;
    snprintf(msg, sizeof(msg), "\r\n--- Essai %u ---\r\n", essai);
    Serial_Print(msg);

    if (SDLogger_Init())              // affiche lui-meme les lignes "[SD diag] ..."
    {
      SDLoggerCardType type = SDLogger_GetCardType();
      snprintf(msg, sizeof(msg), ">>> CARTE OK : %s detectee (CS %s)\r\n",
               (type == SDLOGGER_CARD_SDHC) ? "SDHC/SDXC" :
               (type == SDLOGGER_CARD_SDSC) ? "SDSC (<= 2 Go)" : "carte de type inconnu",
               SDLogger_GetActiveCsLabel());
      Serial_Print(msg);
      Serial_Print("Test termine. RESET pour recommencer.\r\n");
      while (1) { HAL_Delay(1000); }
    }

    snprintf(msg, sizeof(msg), ">>> ECHEC : %s\r\n", nom_erreur(SDLogger_GetLastInitStatus()));
    Serial_Print(msg);

    Serial_Print("\r\n--- Diagnostic bit-bang (SPI logiciel) ---\r\n");
    if (SDLogger_BitBangCMD0Test())
      Serial_Print(">>> Le bit-bang marche mais pas le SPI materiel : verifier la config SPI dans setup.c\r\n");
    else
      Serial_Print(">>> Le bit-bang echoue aussi : probleme PHYSIQUE (carte absente/mal formatee, fils ICSP SCK-MISO-MOSI, CS, alim du shield)\r\n");

    Serial_Print("Nouvel essai dans 5 s...\r\n");
    HAL_Delay(5000);
  }
}