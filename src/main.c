
#include "board.h"
#include "setup.h" /**/
#include "modes.h"

#include "sensor_rtc.h"
#include "sensor_light.h"
#include "sensor_bme680.h"/**/
#include "sensor_gps.h"
#include "sensor_sd.h"

#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdint.h>/**/
#include <stdbool.h>

// 0 : signalement des erreurs désactivé
// 1 : signalement des erreurs activé
#define SIGNALER_ERREURS 0

// Mémorise les échecs successifs de chaque capteur
#if SIGNALER_ERREURS
static uint8_t echec_rtc = 0;
static uint8_t echec_gps = 0;
static uint8_t echec_capteur = 0;
static uint8_t echec_sd = 0;
#endif


// Calcule le temps entre deux acquisitions
static uint32_t intervalle_ms(void)
{
    uint32_t intervalle =
        (uint32_t)Modes_Config()->log_interval_s * 1000UL;

    // En mode économique, on espace les mesures
    if (Modes_Get() == MODE_ECONOMIQUE)
        intervalle *= 2UL;

    return intervalle;
}


#if SIGNALER_ERREURS
// Déclare une erreur après deux échecs consécutifs
static bool en_erreur(uint8_t *compteur, bool acquisition_ok)
{
    if (acquisition_ok)
        *compteur = 0;
    else if (*compteur < 255)
        (*compteur)++;

    return (*compteur >= 2);
}
#endif


// Effectue une acquisition complète et prépare une ligne de données
static void faire_mesure(void)
{
    static uint8_t compteur_gps = 0;

    const Config_t *cfg = Modes_Config();

    char date[24];
    char lumin[16];
    char bme[48];
    char gps[48];
    char ligne[200];

    // 1. Lire la date et l'heure
    bool rtc_ok = SensorRTC_Acquire(date, sizeof(date));

    // 2. Lire la luminosité si le capteur est activé
    bool lum_ok = true;

    if (cfg->lumin)
        lum_ok = SensorLight_Acquire(lumin, sizeof(lumin));
    else
        snprintf(lumin, sizeof(lumin), "NA");

    // 3. Lire la température, l'humidité et la pression
    bool bme_ok = SensorBME680_Acquire(bme, sizeof(bme));

    // 4. Lire les coordonnées GPS
    bool gps_ok = true;

    // En mode économique, une lecture GPS sur deux est ignorée
    if (Modes_Get() == MODE_ECONOMIQUE &&
        (compteur_gps++ % 2U) == 1U)
    {
        snprintf(gps, sizeof(gps), "NA");
    }
    else
    {
        gps_ok = SensorGPS_Acquire(gps, sizeof(gps));
    }

    // 5. Regrouper toutes les données dans une seule ligne
    snprintf(ligne, sizeof(ligne),
             "%s;%s;%s;%s\r\n",
             date, lumin, bme, gps);

    // 6. Enregistrer sur la carte SD si le mode l'autorise
    bool sd_ok = true;

    if (Modes_SdAutorisee())
        sd_ok = SensorSD_WriteLine(ligne);

    // 7. Afficher les données sur le port série
    Serial_Print(ligne);


#if SIGNALER_ERREURS

    // Vérifier les erreurs après deux échecs consécutifs
    bool err_rtc = en_erreur(&echec_rtc, rtc_ok);
    bool err_gps = en_erreur(&echec_gps, gps_ok);
    bool err_cap = en_erreur(&echec_capteur, lum_ok && bme_ok);
    bool err_sd  = en_erreur(&echec_sd, sd_ok);

    // Afficher en priorité la première erreur détectée
    if (err_rtc)
        Modes_SetErreur(LED_ERR_RTC);
    else if (err_gps)
        Modes_SetErreur(LED_ERR_GPS);
    else if (err_cap)
        Modes_SetErreur(LED_ERR_CAPTEUR);
    else if (err_sd)
        Modes_SetErreur(LED_ERR_SD_ECRITURE);
    else
        Modes_ClearErreur();

#endif
}


// Fonction principale
int main(void)
{
    // 1. Initialiser la carte et les communications
    Board_Init();
    Global_Init();

    // 2. Initialiser les capteurs
    SensorRTC_Init();
    SensorLight_Init();
    SensorBME680_Init();
    SensorGPS_Init();
    SensorSD_Init();

    // 3. Initialiser les modes, les boutons et les LED
    Modes_Init();

    // Permet d'effectuer la première mesure immédiatement
    uint32_t derniere_mesure =
        HAL_GetTick() - intervalle_ms();

    // 4. Boucle principale du programme
    while (1)
    {
        // Mettre à jour les boutons et le mode actif
        Modes_Update();

        // Vérifier si les acquisitions sont autorisées
        // et si le temps prévu entre deux mesures est écoulé
        if (Modes_AcquisitionActive() &&
            (HAL_GetTick() - derniere_mesure) >= intervalle_ms())
        {
            derniere_mesure = HAL_GetTick();

            faire_mesure();
        }

        // Attendre brièvement avant de recommencer la boucle
        if (Modes_Get() == MODE_CONFIGURATION)
            Delay_ms(1);
        else
            Delay_ms(20);
    }
}
