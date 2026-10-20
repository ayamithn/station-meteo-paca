#include "sensor_bme680.h"
#include "board.h"        // Delay_ms()
#include "board_pins.h"   // GROVE_I2C_HANDLE (bus I2C1)
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdint.h>

/*
 * PRINCIPE (à expliquer à l'oral) :
 * Le BME680 ne donne pas directement "21 °C" : il renvoie des nombres bruts.
 * Chaque capteur contient aussi des "coefficients de calibration" écrits par
 * le fabricant (Bosch). On les lit une fois au démarrage, puis on les utilise
 * pour transformer chaque nombre brut en vraie valeur.
 *   1. Init : trouver le capteur, lire ses coefficients, le régler
 *   2. Read : lancer une mesure, lire les nombres bruts, les convertir
 */

extern I2C_HandleTypeDef GROVE_I2C_HANDLE;

// Adresses I2C possibles (décalées d'un bit comme l'exige la HAL)
#define ADRESSE_1        (0x76 << 1)
#define ADRESSE_2        (0x77 << 1)

// Registres du capteur utilisés
#define REG_ID           0xD0   // contient 0x61 sur un BME680
#define REG_RESET        0xE0   // écrire 0xB6 = redémarrer le capteur
#define REG_CTRL_HUM     0x72   // réglage de la mesure d'humidité
#define REG_CTRL_MEAS    0x74   // réglage température/pression + lancement d'une mesure
#define REG_CONFIG       0x75   // filtre
#define REG_CTRL_GAS_1   0x71   // activation de la mesure de gaz
#define REG_GAS_WAIT_0   0x64   // durée de chauffe du capteur de gaz
#define REG_RES_HEAT_0   0x5A   // température de chauffe du capteur de gaz
#define REG_STATUT       0x1D   // état de la mesure
#define REG_DONNEES      0x1F   // début des données brutes (pression, température, humidité)
#define REG_GAZ          0x2A   // données brutes du gaz

#define ID_BME680        0x61
#define COMMANDE_RESET   0xB6

// Réglages choisis
#define CTRL_HUM_X2      0x02   // humidité : 2 mesures moyennées
#define CTRL_MEAS_FORCE  0x4D   // température x2, pression x4, mode forcé (= 1 seule mesure)
#define CTRL_GAS_ACTIF   0x10   // mesure de gaz activée
#define GAS_WAIT_100MS   0x59   // chauffe pendant 100 ms
#define TEMP_CHAUFFE_C   300    // le capteur de gaz chauffe à 300 °C
#define TEMP_AMBIANTE_C  25     // température ambiante supposée (réglage de la chauffe)

#define TIMEOUT_I2C_MS   100    // attente max pour une communication I2C

// Variables du fichier
static uint16_t adresse = ADRESSE_1;   // adresse où le capteur a été trouvé
static bool capteur_pret = false;      // true quand l'init a réussi

// Coefficients de calibration de CE capteur (lus à l'init)
static uint16_t par_t1;  static int16_t par_t2;  static int8_t par_t3;
static uint16_t par_p1;  static int16_t par_p2;  static int8_t par_p3;
static int16_t  par_p4;  static int16_t par_p5;  static int8_t par_p6;
static int8_t   par_p7;  static int16_t par_p8;  static int16_t par_p9;
static uint8_t  par_p10;
static uint16_t par_h1;  static uint16_t par_h2;  static int8_t par_h3;
static int8_t   par_h4;  static int8_t par_h5;    static uint8_t par_h6;
static int8_t   par_h7;
static int8_t   par_g1;  static int16_t par_g2;   static int8_t par_g3;
static uint8_t  res_heat_range;
static int8_t   res_heat_val;
static int8_t   range_sw_err;

static float t_fine = 0.0f;   // valeur intermédiaire (calculée avec la température)


/* ---------- Communication I2C ---------- */

static bool lire_registres(uint8_t reg, uint8_t *donnees, uint16_t nb)
{
  return HAL_I2C_Mem_Read(&GROVE_I2C_HANDLE, adresse, reg, I2C_MEMADD_SIZE_8BIT,
                          donnees, nb, TIMEOUT_I2C_MS) == HAL_OK;
}

static bool ecrire_registre(uint8_t reg, uint8_t valeur)
{
  return HAL_I2C_Mem_Write(&GROVE_I2C_HANDLE, adresse, reg, I2C_MEMADD_SIZE_8BIT,
                           &valeur, 1, TIMEOUT_I2C_MS) == HAL_OK;
}

// Vérifie qu'un BME680 répond à l'adresse donnée
static bool capteur_present(uint16_t adr)
{
  uint8_t id = 0;
  adresse = adr;
  return lire_registres(REG_ID, &id, 1) && (id == ID_BME680);
}


/* ---------- Calibration (faite une seule fois) ---------- */

static bool lire_calibration(void)
{
  uint8_t b1[23];   // registres 0x8A à 0xA0
  uint8_t b2[14];   // registres 0xE1 à 0xEE
  uint8_t r_range, r_val, r_err;

  if (!lire_registres(0x8A, b1, sizeof(b1))) return false;
  if (!lire_registres(0xE1, b2, sizeof(b2))) return false;
  if (!lire_registres(0x02, &r_range, 1))    return false;
  if (!lire_registres(0x00, &r_val, 1))      return false;
  if (!lire_registres(0x04, &r_err, 1))      return false;

  // Température
  par_t1 = (uint16_t)((b2[9] << 8) | b2[8]);
  par_t2 = (int16_t)((b1[1] << 8) | b1[0]);
  par_t3 = (int8_t)b1[2];

  // Pression
  par_p1  = (uint16_t)((b1[5] << 8) | b1[4]);
  par_p2  = (int16_t)((b1[7] << 8) | b1[6]);
  par_p3  = (int8_t)b1[8];
  par_p4  = (int16_t)((b1[11] << 8) | b1[10]);
  par_p5  = (int16_t)((b1[13] << 8) | b1[12]);
  par_p6  = (int8_t)b1[15];
  par_p7  = (int8_t)b1[14];
  par_p8  = (int16_t)((b1[19] << 8) | b1[18]);
  par_p9  = (int16_t)((b1[21] << 8) | b1[20]);
  par_p10 = b1[22];

  // Humidité (h1 et h2 se partagent l'octet 0xE2)
  par_h1 = (uint16_t)((b2[2] << 4) | (b2[1] & 0x0F));
  par_h2 = (uint16_t)((b2[0] << 4) | (b2[1] >> 4));
  par_h3 = (int8_t)b2[3];
  par_h4 = (int8_t)b2[4];
  par_h5 = (int8_t)b2[5];
  par_h6 = b2[6];
  par_h7 = (int8_t)b2[7];

  // Gaz
  par_g2 = (int16_t)((b2[11] << 8) | b2[10]);
  par_g1 = (int8_t)b2[12];
  par_g3 = (int8_t)b2[13];
  res_heat_range = (r_range & 0x30) >> 4;
  res_heat_val   = (int8_t)r_val;
  range_sw_err   = (int8_t)(r_err & 0xF0) / 16;

  return true;
}

// Calcule la valeur à écrire pour que le capteur de gaz chauffe à "temp_cible" °C
static uint8_t calculer_chauffe(int temp_cible, int temp_ambiante)
{
  float v1 = ((float)par_g1 / 16.0f) + 49.0f;
  float v2 = (((float)par_g2 / 32768.0f) * 0.0005f) + 0.00235f;
  float v3 = (float)par_g3 / 1024.0f;
  float v4 = v1 * (1.0f + (v2 * (float)temp_cible));
  float v5 = v4 + (v3 * (float)temp_ambiante);

  return (uint8_t)(3.4f * ((v5 * (4.0f / (4.0f + (float)res_heat_range))
                            * (1.0f / (1.0f + ((float)res_heat_val * 0.002f)))) - 25.0f));
}


/* ---------- Conversion des nombres bruts en vraies valeurs ---------- */
/* (formules du fabricant Bosch, version "nombres à virgule") */

// Température en °C (à calculer EN PREMIER : pression et humidité en dépendent)
static float calculer_temperature(uint32_t brut)
{
  float v1 = (((float)brut / 16384.0f) - ((float)par_t1 / 1024.0f)) * (float)par_t2;
  float ecart = ((float)brut / 131072.0f) - ((float)par_t1 / 8192.0f);
  float v2 = ecart * ecart * ((float)par_t3 * 16.0f);

  t_fine = v1 + v2;
  return t_fine / 5120.0f;
}

// Pression en hPa
static float calculer_pression(uint32_t brut)
{
  float v1 = (t_fine / 2.0f) - 64000.0f;
  float v2 = v1 * v1 * ((float)par_p6 / 131072.0f);
  v2 = v2 + (v1 * (float)par_p5 * 2.0f);
  v2 = (v2 / 4.0f) + ((float)par_p4 * 65536.0f);
  v1 = ((((float)par_p3 * v1 * v1) / 16384.0f) + ((float)par_p2 * v1)) / 524288.0f;
  v1 = (1.0f + (v1 / 32768.0f)) * (float)par_p1;

  if ((int)v1 == 0) return 0.0f;   // évite une division par zéro

  float p = 1048576.0f - (float)brut;
  p = ((p - (v2 / 4096.0f)) * 6250.0f) / v1;

  v1 = ((float)par_p9 * p * p) / 2147483648.0f;
  v2 = p * ((float)par_p8 / 32768.0f);
  float v3 = (p / 256.0f) * (p / 256.0f) * (p / 256.0f) * ((float)par_p10 / 131072.0f);
  p = p + (v1 + v2 + v3 + ((float)par_p7 * 128.0f)) / 16.0f;

  return p / 100.0f;   // pascals -> hectopascals
}

// Humidité relative en %
static float calculer_humidite(uint16_t brut)
{
  float temp = t_fine / 5120.0f;

  float v1 = (float)brut - (((float)par_h1 * 16.0f) + (((float)par_h3 / 2.0f) * temp));
  float v2 = v1 * (((float)par_h2 / 262144.0f)
             * (1.0f + (((float)par_h4 / 16384.0f) * temp)
                     + (((float)par_h5 / 1048576.0f) * temp * temp)));
  float v3 = (float)par_h6 / 16384.0f;
  float v4 = (float)par_h7 / 2097152.0f;
  float h = v2 + ((v3 + (v4 * temp)) * v2 * v2);

  if (h > 100.0f) h = 100.0f;
  if (h < 0.0f)   h = 0.0f;
  return h;
}

// Résistance du capteur de gaz en ohms
static float calculer_gaz(uint16_t brut, uint8_t plage)
{
  static const float k1[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, -0.8f,
                               0.0f, 0.0f, -0.2f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f};
  static const float k2[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.7f, 0.0f, -0.8f,
                               -0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  float v1 = 1340.0f + (5.0f * (float)range_sw_err);
  float v2 = v1 * (1.0f + k1[plage] / 100.0f);
  float v3 = 1.0f + (k2[plage] / 100.0f);

  return 1.0f / (v3 * 0.000000125f * (float)(1UL << plage)
                 * ((((float)brut - 512.0f) / v2) + 1.0f));
}


/* ---------- Fonctions publiques ---------- */

bool SensorBME680_Init(void)
{
  capteur_pret = false;

  // 1. Chercher le capteur aux deux adresses possibles
  if (!capteur_present(ADRESSE_1) && !capteur_present(ADRESSE_2)) return false;

  // 2. Redémarrer le capteur puis lire sa calibration
  if (!ecrire_registre(REG_RESET, COMMANDE_RESET)) return false;
  Delay_ms(10);
  if (!lire_calibration()) return false;

  // 3. Régler le capteur (humidité, filtre, chauffe du capteur de gaz)
  if (!ecrire_registre(REG_CTRL_HUM, CTRL_HUM_X2))      return false;
  if (!ecrire_registre(REG_CONFIG, 0x00))               return false;   // pas de filtre
  if (!ecrire_registre(REG_GAS_WAIT_0, GAS_WAIT_100MS)) return false;
  if (!ecrire_registre(REG_RES_HEAT_0, calculer_chauffe(TEMP_CHAUFFE_C, TEMP_AMBIANTE_C))) return false;
  if (!ecrire_registre(REG_CTRL_GAS_1, CTRL_GAS_ACTIF)) return false;

  capteur_pret = true;
  return true;
}

bool SensorBME680_Read(Env_Data *data)
{
  uint8_t statut = 0;
  uint8_t brut[8];       // pression, température, humidité
  uint8_t brut_gaz[2];
  int i;

  if (data == NULL) return false;

  // Si l'init a échoué au démarrage (capteur débranché ?), on réessaie
  if (!capteur_pret && !SensorBME680_Init()) return false;

  // 1. Lancer UNE mesure
  if (!ecrire_registre(REG_CTRL_MEAS, CTRL_MEAS_FORCE)) return false;

  // 2. Attendre la fin de la mesure (environ 125 ms, on vérifie toutes les 10 ms)
  for (i = 0; i < 50; i++)
  {
    Delay_ms(10);
    if (!lire_registres(REG_STATUT, &statut, 1)) return false;
    if ((statut & 0x80) && !(statut & 0x60)) break;   // données prêtes et mesure terminée
  }
  if (i == 50) return false;   // le capteur n'a pas fini : échec

  // 3. Lire les nombres bruts
  if (!lire_registres(REG_DONNEES, brut, 8))    return false;
  if (!lire_registres(REG_GAZ, brut_gaz, 2))    return false;

  uint32_t pression_brute = ((uint32_t)brut[0] << 12) | ((uint32_t)brut[1] << 4) | (brut[2] >> 4);
  uint32_t temp_brute     = ((uint32_t)brut[3] << 12) | ((uint32_t)brut[4] << 4) | (brut[5] >> 4);
  uint16_t hum_brute      = (uint16_t)((brut[6] << 8) | brut[7]);
  uint16_t gaz_brut       = (uint16_t)((brut_gaz[0] << 2) | (brut_gaz[1] >> 6));
  uint8_t  plage_gaz      = brut_gaz[1] & 0x0F;
  bool     gaz_valide     = (brut_gaz[1] & 0x20) && (brut_gaz[1] & 0x10);

  // 4. Convertir (la température d'abord)
  data->temperature_c      = calculer_temperature(temp_brute);
  data->pressure_hpa       = calculer_pression(pression_brute);
  data->humidity_percent   = calculer_humidite(hum_brute);
  data->gas_resistance_ohm = gaz_valide ? calculer_gaz(gaz_brut, plage_gaz) : 0.0f;   // 0 = pas de valeur valide

  return true;
}

// Écrit un nombre avec 1 chiffre après la virgule (ex : -3.5).
// On évite "%f" de snprintf : par prudence, car sur STM32 il est souvent absent de la
// bibliothèque allégée et n'afficherait rien.
void SensorBME680_ValeurEnTexte(float valeur, char *out, size_t taille)
{
  int32_t dixiemes = (int32_t)(valeur * 10.0f + (valeur >= 0.0f ? 0.5f : -0.5f));
  const char *signe = (dixiemes < 0) ? "-" : "";

  if (dixiemes < 0) dixiemes = -dixiemes;
  snprintf(out, taille, "%s%ld.%ld", signe, (long)(dixiemes / 10), (long)(dixiemes % 10));
}

bool SensorBME680_Acquire(char *out, size_t size)
{
  Env_Data mesure;
  char temp[16], hum[16], pres[16];

  if (!SensorBME680_Read(&mesure))
  {
    snprintf(out, size, "NA;NA;NA");
    return false;
  }

  SensorBME680_ValeurEnTexte(mesure.temperature_c, temp, sizeof(temp));
  SensorBME680_ValeurEnTexte(mesure.humidity_percent, hum, sizeof(hum));
  SensorBME680_ValeurEnTexte(mesure.pressure_hpa, pres, sizeof(pres));
  
  snprintf(out, size, "%s;%s;%s", temp, hum, pres);
  return true;
}