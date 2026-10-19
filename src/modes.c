#include "modes.h"
#include "board.h"                // Serial_Print
#include "grove_button.h"         // pilote du prof (Grove Dual Button sur D2)
#include "grove_rtc_ds1307.h"     // commandes CLOCK, DATE, DAY
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===================== RÉGLAGES =====================
#define TEST_RAPIDE            0      // 1 = une mesure toutes les 5 s par défaut (tests) ; 0 = 10 min (version finale)
#define DUREE_APPUI_LONG_MS    5000UL                   // appui de 5 secondes (document)
#define TIMEOUT_CONFIG_MS      (30UL * 60UL * 1000UL)   // 30 min sans activité
#define TEST_CARTE_SEULE       0      // 1 = le bouton bleu B1 de la carte joue le bouton rouge
#define BOUTONS_INVERSES       0      // 1 si "appuyé" est lu au repos (logique inversée)

#if TEST_RAPIDE
  #define LOG_INTERVAL_DEFAUT_S  5
#else
  #define LOG_INTERVAL_DEFAUT_S  600
#endif

#if TEST_CARTE_SEULE
  #define LIRE_ROUGE()  (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
  #define LIRE_VERT()   false
#elif BOUTONS_INVERSES
  #define LIRE_ROUGE()  (!GroveButton1_IsPressed())
  #define LIRE_VERT()   (!GroveButton2_IsPressed())
#else
  #define LIRE_ROUGE()  GroveButton1_IsPressed()    // inverse les deux lignes si rouge et vert sont échangés
  #define LIRE_VERT()   GroveButton2_IsPressed()
#endif

extern UART_HandleTypeDef huart2;   // défini dans board.c

// ===================== ÉTAT =====================
typedef struct {
  uint32_t debut;      // instant du début de l'appui
  bool     actif;      // bouton maintenu
  bool     declenche;  // appui long déjà signalé
} Appui_t;

static Mode_t   mode_courant;
static Mode_t   mode_precedent;       // pour revenir de la maintenance
static uint32_t derniere_activite;    // dernier appui ou touche tapée (mode configuration)
static int      erreur = -1;          // -1 = aucune ; sinon une valeur LedEtat_t
static Appui_t  appui_rouge, appui_vert;
static Config_t cfg;

// ===================== PARAMÈTRES =====================
// Valeurs par défaut du document (la commande RESET y revient)
static void config_defauts(void)
{
  cfg.log_interval_s = LOG_INTERVAL_DEFAUT_S;
  cfg.file_max_size  = 2048;
  cfg.timeout_s      = 30;
  cfg.lumin = 1;     cfg.lumin_low = 255;    cfg.lumin_high = 768;
  cfg.temp_air = 1;  cfg.min_temp_air = -10; cfg.max_temp_air = 60;
  cfg.hygr = 1;      cfg.hygr_mint = 0;      cfg.hygr_maxt = 50;
  cfg.pressure = 1;  cfg.pressure_min = 850; cfg.pressure_max = 1080;
}

// --- Sauvegarde en Flash : la STM32L476 n'a pas d'EEPROM, on utilise la dernière page (2 Ko) ---
#define FLASH_CFG_ADRESSE  0x080FF800UL     // dernière page de la Flash (banque 2, page 255)
#define FLASH_CFG_PAGE     255U
#define FLASH_CFG_MAGIC    0x43464731UL     // "CFG1" : repère une sauvegarde valide

typedef struct {
  uint32_t magic;      // doit valoir FLASH_CFG_MAGIC
  uint32_t taille;     // sizeof(Config_t) : détecte un changement de version du programme
  Config_t cfg;        // les paramètres
  uint32_t somme;      // contrôle d'intégrité
} Stockage_t;

static uint32_t calculer_somme(const Config_t *c)
{
  const uint8_t *p = (const uint8_t *)c;
  uint32_t s = 5381U;
  for (size_t i = 0; i < sizeof(Config_t); i++) s = s * 33U + p[i];
  return s;
}

// Au démarrage : reprend les paramètres sauvegardés s'ils sont valides, sinon garde les défauts
static void config_charger(void)
{
  const Stockage_t *s = (const Stockage_t *)FLASH_CFG_ADRESSE;
  if (s->magic == FLASH_CFG_MAGIC &&
      s->taille == sizeof(Config_t) &&
      s->somme == calculer_somme(&s->cfg))
  {
    cfg = s->cfg;
  }
}

// Enregistre les paramètres en Flash ; renvoie false en cas d'erreur
static bool config_sauver(void)
{
  Stockage_t s;
  uint64_t blocs[(sizeof(Stockage_t) + 7U) / 8U];   // la Flash s'écrit par blocs de 64 bits

  memset(&s, 0xFF, sizeof(s));
  s.magic  = FLASH_CFG_MAGIC;
  s.taille = sizeof(Config_t);
  s.cfg    = cfg;
  s.somme  = calculer_somme(&cfg);

  memset(blocs, 0xFF, sizeof(blocs));
  memcpy(blocs, &s, sizeof(s));

  bool ok = true;
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  FLASH_EraseInitTypeDef efface = {0};
  uint32_t page_en_erreur = 0;
  efface.TypeErase = FLASH_TYPEERASE_PAGES;
  efface.Banks     = FLASH_BANK_2;
  efface.Page      = FLASH_CFG_PAGE;
  efface.NbPages   = 1;
  if (HAL_FLASHEx_Erase(&efface, &page_en_erreur) != HAL_OK) ok = false;

  for (size_t i = 0; ok && i < (sizeof(blocs) / sizeof(blocs[0])); i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                          FLASH_CFG_ADRESSE + (8U * i), blocs[i]) != HAL_OK) ok = false;
  }
  HAL_FLASH_Lock();
  return ok;
}

// Enregistre et prévient le PC (termine la ligne de réponse)
static void enregistrer(void)
{
  Serial_Print(config_sauver() ? " (enregistre)\r\n" : " (ERREUR d'enregistrement en Flash)\r\n");
}

// Table des paramètres simples : nom de la commande, variable, valeurs min et max autorisées
typedef struct { const char *nom; int32_t *valeur; int32_t min, max; } Param_t;

static const Param_t params[] = {
  { "FILE_MAX_SIZE", &cfg.file_max_size, 512,  65536 },
  { "TIMEOUT",       &cfg.timeout_s,     1,    3600  },
  { "LUMIN",         &cfg.lumin,         0,    1     },
  { "LUMIN_LOW",     &cfg.lumin_low,     0,    1023  },
  { "LUMIN_HIGH",    &cfg.lumin_high,    0,    1023  },
  { "TEMP_AIR",      &cfg.temp_air,      0,    1     },
  { "MIN_TEMP_AIR",  &cfg.min_temp_air,  -40,  85    },
  { "MAX_TEMP_AIR",  &cfg.max_temp_air,  -40,  85    },
  { "HYGR",          &cfg.hygr,          0,    1     },
  { "HYGR_MINT",     &cfg.hygr_mint,     -40,  85    },
  { "HYGR_MAXT",     &cfg.hygr_maxt,     -40,  85    },
  { "PRESSURE",      &cfg.pressure,      0,    1     },
  { "PRESSURE_MIN",  &cfg.pressure_min,  300,  1100  },
  { "PRESSURE_MAX",  &cfg.pressure_max,  300,  1100  }
};

// Lit "a<sep>b<sep>c" (ex : "14:30:00" ou "10,17,2026") ; renvoie false si le format est faux
static bool lire_3_nombres(const char *s, char sep, long *a, long *b, long *c)
{
  char *fin;
  *a = strtol(s, &fin, 10);       if (*fin != sep) return false;
  *b = strtol(fin + 1, &fin, 10); if (*fin != sep) return false;
  *c = strtol(fin + 1, &fin, 10); return *fin == '\0';
}

// Exécute une commande tapée sur le port série : "NOM=valeur", "RESET" ou "VERSION"
static void traiter_commande(char *cmd)
{
  char msg[100];

  for (char *p = cmd; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;   // tout en majuscules

  if (strcmp(cmd, "VERSION") == 0) { Serial_Print("Station meteo v1.0 - lot 001\r\n"); return; }
  if (strcmp(cmd, "RESET") == 0) {
    config_defauts();
    Serial_Print("Parametres reinitialises");
    enregistrer();
    return;
  }

  char *egal = strchr(cmd, '=');
  if (egal == NULL) { Serial_Print("Commande inconnue\r\n"); return; }
  *egal = '\0';                         // cmd devient le nom, "valeur" le texte après le '='
  const char *nom = cmd;
  const char *valeur = egal + 1;

  // --- Horloge : CLOCK=HH:MM:SS, DATE=MOIS,JOUR,ANNEE, DAY=MON..SUN (gardée par la RTC) ---
  if (strcmp(nom, "CLOCK") == 0 || strcmp(nom, "DATE") == 0 || strcmp(nom, "DAY") == 0) {
    RTC_DateTime t;
    if (!GroveRTC_GetDateTime(&t)) { Serial_Print("Erreur : RTC injoignable\r\n"); return; }
    long a, b, c;

    if (strcmp(nom, "CLOCK") == 0) {
      if (!lire_3_nombres(valeur, ':', &a, &b, &c) || a < 0 || a > 23 || b < 0 || b > 59 || c < 0 || c > 59) {
        Serial_Print("Format : CLOCK=HH:MM:SS\r\n"); return;
      }
      t.hours = (uint8_t)a; t.minutes = (uint8_t)b; t.seconds = (uint8_t)c;
    } else if (strcmp(nom, "DATE") == 0) {
      if (!lire_3_nombres(valeur, ',', &a, &b, &c) || a < 1 || a > 12 || b < 1 || b > 31 || c < 2000 || c > 2099) {
        Serial_Print("Format : DATE=MOIS,JOUR,ANNEE (ex : DATE=10,17,2026)\r\n"); return;
      }
      t.month = (uint8_t)a; t.day = (uint8_t)b; t.year = (uint16_t)c;
    } else {
      static const char *jours[] = { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };
      int j = -1;
      for (int i = 0; i < 7; i++) if (strcmp(valeur, jours[i]) == 0) j = i;
      if (j < 0) { Serial_Print("Format : DAY=MON, TUE, WED, THU, FRI, SAT ou SUN\r\n"); return; }
      t.dayOfWeek = (uint8_t)(j + 1);     // 1 = lundi ... 7 = dimanche
    }
    Serial_Print(GroveRTC_SetDateTime(&t) ? "OK\r\n" : "Erreur d'ecriture RTC\r\n");
    return;
  }

  // --- Paramètres numériques ---
  char *fin;
  long v = strtol(valeur, &fin, 10);
  if (*valeur == '\0' || *fin != '\0') { Serial_Print("Valeur invalide\r\n"); return; }

  // LOG_INTERVAL est donné en minutes (le document l'écrit aussi "LOG_INTERVALL")
  if (strcmp(nom, "LOG_INTERVAL") == 0 || strcmp(nom, "LOG_INTERVALL") == 0) {
    if (v < 1 || v > 1440) { Serial_Print("LOG_INTERVAL : 1 a 1440 minutes\r\n"); return; }
    cfg.log_interval_s = (int32_t)(v * 60);
    Serial_Print("OK");
    enregistrer();
    return;
  }

  for (unsigned i = 0; i < sizeof(params) / sizeof(params[0]); i++) {
    if (strcmp(nom, params[i].nom) == 0) {
      if (v < params[i].min || v > params[i].max) {
        snprintf(msg, sizeof(msg), "%s : valeur entre %ld et %ld\r\n",
                 params[i].nom, (long)params[i].min, (long)params[i].max);
        Serial_Print(msg);
        return;
      }
      *params[i].valeur = (int32_t)v;
      snprintf(msg, sizeof(msg), "OK : %s=%ld", params[i].nom, v);
      Serial_Print(msg);
      enregistrer();
      return;
    }
  }
  Serial_Print("Commande inconnue\r\n");
}

// Lit les caractères reçus du PC (sans bloquer) ; exécute la commande à chaque Entrée
static void lire_console(void)
{
  static char ligne[48];
  static uint8_t n = 0;
  uint8_t c;

  __HAL_UART_CLEAR_OREFLAG(&huart2);                       // évite un blocage en cas de dépassement
  while (HAL_UART_Receive(&huart2, &c, 1, 0) == HAL_OK) {
    derniere_activite = HAL_GetTick();                     // une touche tapée = de l'activité
    if (c == '\r' || c == '\n') {
      if (n > 0) { ligne[n] = '\0'; n = 0; traiter_commande(ligne); }
    } else if (n < sizeof(ligne) - 1) {
      ligne[n++] = (char)c;
    }
  }
}

// ===================== BOUTONS =====================
// Renvoie true UNE SEULE FOIS quand le bouton est maintenu assez longtemps
static bool appui_long(Appui_t *a, bool presse)
{
  uint32_t now = HAL_GetTick();
  if (!presse) { a->actif = false; a->declenche = false; return false; }
  if (!a->actif) { a->actif = true; a->debut = now; a->declenche = false; return false; }
  if (!a->declenche && (now - a->debut) >= DUREE_APPUI_LONG_MS) {
    a->declenche = true;
    return true;
  }
  return false;
}

// ===================== MODES ET LED =====================
// Choisit ce que la LED affiche : l'erreur si elle existe, sinon la couleur du mode
static void maj_led(void)
{
  if (erreur >= 0) { StatusLed_Set((LedEtat_t)erreur); return; }
  switch (mode_courant) {
    case MODE_STANDARD:      StatusLed_Set(LED_STANDARD);      break;
    case MODE_CONFIGURATION: StatusLed_Set(LED_CONFIGURATION); break;
    case MODE_ECONOMIQUE:    StatusLed_Set(LED_ECONOMIQUE);    break;
    case MODE_MAINTENANCE:   StatusLed_Set(LED_MAINTENANCE);   break;
  }
}

static void changer_mode(Mode_t nouveau)
{
  mode_courant = nouveau;
  derniere_activite = HAL_GetTick();
  if (nouveau == MODE_CONFIGURATION) erreur = -1;   // pas de mesures en configuration : on efface l'erreur affichée
  switch (nouveau) {
    case MODE_STANDARD:      Serial_Print("Mode STANDARD\r\n");      break;
    case MODE_CONFIGURATION: Serial_Print("Mode CONFIGURATION (tape VERSION, CLOCK=HH:MM:SS, LOG_INTERVAL=10...)\r\n"); break;
    case MODE_ECONOMIQUE:    Serial_Print("Mode ECONOMIQUE\r\n");    break;
    case MODE_MAINTENANCE:   Serial_Print("Mode MAINTENANCE\r\n");   break;
  }
  maj_led();
}

void Modes_Init(void)
{
  config_defauts();
  config_charger();                      // reprend les paramètres sauvegardés en Flash, s'il y en a
  StatusLed_Init();
#if TEST_CARTE_SEULE
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitTypeDef b1 = {0};
  b1.Pin  = GPIO_PIN_13;                 // bouton bleu B1 de la carte
  b1.Mode = GPIO_MODE_INPUT;
  b1.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &b1);
#else
  GroveButton_Init();
#endif
  HAL_Delay(50);                         // laisse les entrées se stabiliser

  // Bouton rouge pressé au démarrage = configuration, sinon standard
  changer_mode(LIRE_ROUGE() ? MODE_CONFIGURATION : MODE_STANDARD);
}

void Modes_Update(void)
{
  bool rouge = LIRE_ROUGE();
  bool vert  = LIRE_VERT();
  if (rouge || vert) derniere_activite = HAL_GetTick();

  bool rouge_long = appui_long(&appui_rouge, rouge);
  bool vert_long  = appui_long(&appui_vert, vert);

  switch (mode_courant)
  {
    case MODE_STANDARD:
      if (rouge_long)     { mode_precedent = MODE_STANDARD; changer_mode(MODE_MAINTENANCE); }
      else if (vert_long) { changer_mode(MODE_ECONOMIQUE); }
      break;

    case MODE_ECONOMIQUE:
      if (rouge_long) changer_mode(MODE_STANDARD);       // rouge 5 s : retour au standard
      break;

    case MODE_MAINTENANCE:
      if (rouge_long) changer_mode(mode_precedent);      // rouge 5 s : mode précédent
      break;

    case MODE_CONFIGURATION:
      lire_console();                                    // commandes tapées sur le PC
      if ((HAL_GetTick() - derniere_activite) >= TIMEOUT_CONFIG_MS) {
        changer_mode(MODE_STANDARD);                     // 30 min sans activité
      }
      break;
  }

  StatusLed_Update();                                    // fait clignoter la LED si besoin
}

Mode_t Modes_Get(void)               { return mode_courant; }
bool   Modes_AcquisitionActive(void) { return mode_courant != MODE_CONFIGURATION; }
bool   Modes_SdAutorisee(void)       { return mode_courant != MODE_MAINTENANCE; }
const Config_t *Modes_Config(void)   { return &cfg; }

// Temps entre 2 mesures : paramètre LOG_INTERVAL, doublé en mode économique
uint32_t Modes_IntervalleMs(void)
{
  uint32_t ms = (uint32_t)cfg.log_interval_s * 1000UL;
  return (mode_courant == MODE_ECONOMIQUE) ? ms * 2UL : ms;
}

// Mode économique : le GPS n'est lu qu'une fois sur deux
bool Modes_GpsAutorise(void)
{
  static uint8_t compteur = 0;
  if (mode_courant != MODE_ECONOMIQUE) return true;
  return (compteur++ % 2U) == 0U;
}

// ===================== ERREURS =====================
// Document : un capteur est en erreur après 2 mesures ratées d'affilée
static bool echec_repete(uint8_t *compteur, bool ok)
{
  if (ok) *compteur = 0;
  else if (*compteur < 255) (*compteur)++;
  return *compteur >= 2;
}

void Modes_SuivreErreurs(bool rtc_ok, bool gps_ok, bool capteurs_ok, bool incoherent, bool sd_ok)
{
  static uint8_t e_rtc, e_gps, e_cap, e_sd;     // échecs consécutifs de chaque élément

  bool err_rtc = echec_repete(&e_rtc, rtc_ok);
  bool err_gps = echec_repete(&e_gps, gps_ok);
  bool err_cap = echec_repete(&e_cap, capteurs_ok);
  bool err_sd  = echec_repete(&e_sd,  sd_ok);

  if (err_rtc)         Modes_SetErreur(LED_ERR_RTC);
  else if (err_gps)    Modes_SetErreur(LED_ERR_GPS);
  else if (err_cap)    Modes_SetErreur(LED_ERR_CAPTEUR);
  else if (incoherent) Modes_SetErreur(LED_ERR_INCOHERENT);
  else if (err_sd)     Modes_SetErreur(LED_ERR_SD_ECRITURE);
  else                 Modes_ClearErreur();
  // TODO : LED_ERR_SD_PLEINE quand l'écriture SD saura détecter une carte pleine
}

void Modes_SetErreur(LedEtat_t err)  { erreur = (int)err; maj_led(); }
void Modes_ClearErreur(void)         { erreur = -1;       maj_led(); }