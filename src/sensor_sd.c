#include "sensor_sd.h"
#include "sd_archive.h"
#include "sd_logger.h"
#include "modes.h"
#include "grove_rtc_ds1307.h"
#include "board_pins.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <string.h>



extern SPI_HandleTypeDef SD_SPI_HANDLE;






#define TAILLE_SECTEUR     512U
#define CMD16              16U
#define CMD17              17U
#define CMD24              24U
#define TOKEN_DEBUT_BLOC   0xFEU
#define TIMEOUT_JETON_MS   200U
#define TIMEOUT_OCCUPE_MS  500U

static bool carte_prete = false;
static bool carte_sdhc  = false;

static uint8_t spi_octet(uint8_t tx)
{
  uint8_t rx = 0xFF;
  (void)HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &tx, &rx, 1, 100);
  return rx;
}

static void cs_bas(void)  { HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET); }

static void cs_haut(void)
{
  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
  (void)spi_octet(0xFF);
}


static bool attendre_prete(uint32_t timeout_ms)
{
  uint32_t debut = HAL_GetTick();

  while ((HAL_GetTick() - debut) < timeout_ms)
  {
    if (spi_octet(0xFF) == 0xFF) return true;
  }
  return false;
}


static uint8_t sd_commande(uint8_t cmd, uint32_t arg)
{
  uint8_t trame[6];

  cs_haut();
  cs_bas();
  if (!attendre_prete(TIMEOUT_OCCUPE_MS))
  {
    cs_haut();
    return 0xFF;
  }

  trame[0] = (uint8_t)(0x40U | cmd);
  trame[1] = (uint8_t)(arg >> 24);
  trame[2] = (uint8_t)(arg >> 16);
  trame[3] = (uint8_t)(arg >> 8);
  trame[4] = (uint8_t)arg;
  trame[5] = 0x01U;

  for (int i = 0; i < 6; i++) (void)spi_octet(trame[i]);

  uint8_t r1 = 0xFF;
  for (int i = 0; i < 10; i++)
  {
    r1 = spi_octet(0xFF);
    if ((r1 & 0x80U) == 0U) break;
  }
  return r1;
}

static uint32_t sd_adresse(uint32_t lba)
{
  return carte_sdhc ? lba : (lba * TAILLE_SECTEUR);
}

static bool sd_demarrer(void)
{
  carte_prete = false;

  if (!SDLogger_Init()) return false;
  carte_sdhc = (SDLogger_GetCardType() == SDLOGGER_CARD_SDHC);


  if (!carte_sdhc)
  {
    uint8_t r1 = sd_commande(CMD16, TAILLE_SECTEUR);
    cs_haut();
    if (r1 != 0x00U) return false;
  }

  carte_prete = true;
  return true;
}

static bool sd_lire(uint32_t lba, uint8_t *buf)
{
  if (!carte_prete) return false;

  if (sd_commande(CMD17, sd_adresse(lba)) != 0x00U)
  {
    cs_haut();
    return false;
  }


  uint8_t jeton = 0xFF;
  uint32_t debut = HAL_GetTick();
  while ((HAL_GetTick() - debut) < TIMEOUT_JETON_MS)
  {
    jeton = spi_octet(0xFF);
    if (jeton != 0xFFU) break;
  }
  if (jeton != TOKEN_DEBUT_BLOC)
  {
    cs_haut();
    return false;
  }

  for (uint32_t i = 0; i < TAILLE_SECTEUR; i++) buf[i] = spi_octet(0xFF);
  (void)spi_octet(0xFF);
  (void)spi_octet(0xFF);

  cs_haut();
  return true;
}

static bool sd_ecrire(uint32_t lba, const uint8_t *buf)
{
  if (!carte_prete) return false;

  if (sd_commande(CMD24, sd_adresse(lba)) != 0x00U)
  {
    cs_haut();
    return false;
  }

  (void)spi_octet(0xFF);
  (void)spi_octet(TOKEN_DEBUT_BLOC);
  for (uint32_t i = 0; i < TAILLE_SECTEUR; i++) (void)spi_octet(buf[i]);
  (void)spi_octet(0xFF);
  (void)spi_octet(0xFF);


  uint8_t reponse = spi_octet(0xFF);
  if ((reponse & 0x1FU) != 0x05U)
  {
    cs_haut();
    return false;
  }


  bool ok = attendre_prete(TIMEOUT_OCCUPE_MS);
  cs_haut();
  return ok;
}






#define TAILLE_ENTREE    32U
#define ENTREES_SECTEUR  (TAILLE_SECTEUR / TAILLE_ENTREE)
#define ATTR_VOLUME      0x08U
#define ATTR_DOSSIER     0x10U
#define ATTR_ARCHIVE     0x20U
#define ATTR_LFN         0x0FU
#define FIN_REPERTOIRE   0x00U
#define ENTREE_SUPPRIMEE 0xE5U
#define FIN_DE_CHAINE    0x0FFFFFF8UL

typedef enum
{
  FAT_OK = 0,
  FAT_NON_TROUVE,
  FAT_EXISTE,
  FAT_NOM,
  FAT_IO,
  FAT_FORMAT,
  FAT_PLEINE
} FatEtat;


static struct
{
  bool     monte;
  uint8_t  secteurs_cluster;
  uint8_t  nb_fats;
  uint32_t fat_lba;
  uint32_t taille_fat;
  uint32_t racine_cluster;
  uint32_t donnees_lba;
  uint32_t nb_clusters;
  uint32_t fsinfo_lba;
  bool     fsinfo_invalide;
  uint32_t prochain_libre;
} fs;

static uint8_t  sec[TAILLE_SECTEUR];
static uint8_t  fatsec[TAILLE_SECTEUR];
static uint32_t fatsec_lba = 0xFFFFFFFFUL;

static uint16_t date_fat  = 0x0021U;
static uint16_t heure_fat = 0U;




static uint16_t lire16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static uint32_t lire32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void ecrire16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static void ecrire32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static uint32_t cluster_lba(uint32_t cluster)
{
  return fs.donnees_lba + (cluster - 2U) * fs.secteurs_cluster;
}

static bool cluster_valide(uint32_t c) { return c >= 2U && c < (fs.nb_clusters + 2U); }


static bool caractere_valide(uint8_t c)
{
  return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '~';
}

static uint8_t majuscule(char c) { return (uint8_t)((c >= 'a' && c <= 'z') ? c - 32 : c); }


static bool nom_83(const char *nom, uint8_t out[11])
{
  if (nom == NULL) return false;
  memset(out, ' ', 11);

  int i = 0, n = 0;
  for (; nom[i] != '\0' && nom[i] != '.'; i++, n++)
  {
    uint8_t c = majuscule(nom[i]);
    if (n >= 8 || !caractere_valide(c)) return false;
    out[n] = c;
  }
  if (n == 0) return false;

  if (nom[i] == '.')
  {
    int e = 0;
    for (i++; nom[i] != '\0'; i++, e++)
    {
      uint8_t c = majuscule(nom[i]);
      if (e >= 3 || !caractere_valide(c)) return false;
      out[8 + e] = c;
    }
  }
  return true;
}




static bool charger_fat(uint32_t lba)
{
  if (fatsec_lba == lba) return true;
  if (!sd_lire(lba, fatsec)) { fatsec_lba = 0xFFFFFFFFUL; return false; }
  fatsec_lba = lba;
  return true;
}


static FatEtat fat_get(uint32_t cluster, uint32_t *suivant)
{
  uint32_t decalage = cluster * 4U;

  if (!charger_fat(fs.fat_lba + decalage / TAILLE_SECTEUR)) return FAT_IO;

  *suivant = lire32(fatsec + decalage % TAILLE_SECTEUR) & 0x0FFFFFFFUL;
  return FAT_OK;
}


static FatEtat fat_set(uint32_t cluster, uint32_t valeur)
{
  uint32_t decalage = cluster * 4U;
  uint32_t lba = fs.fat_lba + decalage / TAILLE_SECTEUR;

  if (!charger_fat(lba)) return FAT_IO;

  uint8_t *p = fatsec + decalage % TAILLE_SECTEUR;
  ecrire32(p, (lire32(p) & 0xF0000000UL) | (valeur & 0x0FFFFFFFUL));

  for (uint8_t f = 0; f < fs.nb_fats; f++)
  {
    if (!sd_ecrire(lba + f * fs.taille_fat, fatsec)) { fatsec_lba = 0xFFFFFFFFUL; return FAT_IO; }
  }
  return FAT_OK;
}



static void fsinfo_invalider(void)
{
  if (fs.fsinfo_lba == 0U || fs.fsinfo_invalide) return;
  fs.fsinfo_invalide = true;

  if (!sd_lire(fs.fsinfo_lba, sec)) return;
  if (lire32(sec) != 0x41615252UL || lire32(sec + 484) != 0x61417272UL) return;

  ecrire32(sec + 488, 0xFFFFFFFFUL);
  ecrire32(sec + 492, 0xFFFFFFFFUL);
  (void)sd_ecrire(fs.fsinfo_lba, sec);
}


static FatEtat fat_alloc(uint32_t *nouveau)
{
  fsinfo_invalider();

  uint32_t c = cluster_valide(fs.prochain_libre) ? fs.prochain_libre : 2U;

  for (uint32_t essai = 0; essai < fs.nb_clusters; essai++)
  {
    uint32_t v;
    FatEtat st = fat_get(c, &v);
    if (st != FAT_OK) return st;

    if (v == 0U)
    {
      st = fat_set(c, 0x0FFFFFFFUL);
      if (st != FAT_OK) return st;
      fs.prochain_libre = c + 1U;
      *nouveau = c;
      return FAT_OK;
    }

    c++;
    if (c >= fs.nb_clusters + 2U) c = 2U;
  }
  return FAT_PLEINE;
}




typedef struct { uint32_t lba; uint8_t index; } Emplacement;

typedef struct
{
  bool        trouve;
  Emplacement pos;
  uint8_t     entree[TAILLE_ENTREE];
  bool        libre;
  Emplacement pos_libre;
  uint32_t    dernier_cluster;
} Recherche;


static int examiner_secteur(uint32_t lba, const uint8_t nom[11], Recherche *r)
{
  if (!sd_lire(lba, sec)) return -1;

  for (uint8_t i = 0; i < ENTREES_SECTEUR; i++)
  {
    const uint8_t *e = sec + i * TAILLE_ENTREE;

    if (e[0] == FIN_REPERTOIRE)
    {
      if (!r->libre) { r->libre = true; r->pos_libre.lba = lba; r->pos_libre.index = i; }
      return 1;
    }
    if (e[0] == ENTREE_SUPPRIMEE)
    {
      if (!r->libre) { r->libre = true; r->pos_libre.lba = lba; r->pos_libre.index = i; }
      continue;
    }
    if ((e[11] & 0x3FU) == ATTR_LFN) continue;
    if (e[11] & (ATTR_VOLUME | ATTR_DOSSIER)) continue;

    if (memcmp(e, nom, 11) == 0)
    {
      r->trouve = true;
      r->pos.lba = lba;
      r->pos.index = i;
      memcpy(r->entree, e, TAILLE_ENTREE);
      return 1;
    }
  }
  return 0;
}


static FatEtat chercher(const uint8_t nom[11], Recherche *r)
{
  memset(r, 0, sizeof(*r));

  uint32_t cl = fs.racine_cluster;
  uint32_t garde = 0;

  while (cluster_valide(cl) && garde++ < fs.nb_clusters)
  {
    for (uint8_t s = 0; s < fs.secteurs_cluster; s++)
    {
      int res = examiner_secteur(cluster_lba(cl) + s, nom, r);
      if (res < 0) return FAT_IO;
      if (res > 0) return FAT_OK;
    }

    r->dernier_cluster = cl;

    uint32_t suivant;
    FatEtat st = fat_get(cl, &suivant);
    if (st != FAT_OK) return st;
    if (suivant >= FIN_DE_CHAINE) break;
    cl = suivant;
  }
  return FAT_OK;
}


static FatEtat creer_entree(const uint8_t nom[11], Recherche *r)
{
  if (!r->libre)
  {

    if (!cluster_valide(r->dernier_cluster)) return FAT_FORMAT;

    uint32_t nouveau;
    FatEtat st = fat_alloc(&nouveau);
    if (st != FAT_OK) return st;

    memset(sec, 0, TAILLE_SECTEUR);
    for (uint8_t s = 0; s < fs.secteurs_cluster; s++)
    {
      if (!sd_ecrire(cluster_lba(nouveau) + s, sec)) return FAT_IO;
    }
    st = fat_set(r->dernier_cluster, nouveau);
    if (st != FAT_OK) return st;

    r->pos_libre.lba = cluster_lba(nouveau);
    r->pos_libre.index = 0;
    r->libre = true;
  }

  if (!sd_lire(r->pos_libre.lba, sec)) return FAT_IO;

  uint8_t *e = sec + r->pos_libre.index * TAILLE_ENTREE;
  memset(e, 0, TAILLE_ENTREE);
  memcpy(e, nom, 11);
  e[11] = ATTR_ARCHIVE;
  ecrire16(e + 14, heure_fat);
  ecrire16(e + 16, date_fat);
  ecrire16(e + 18, date_fat);
  ecrire16(e + 22, heure_fat);
  ecrire16(e + 24, date_fat);

  if (!sd_ecrire(r->pos_libre.lba, sec)) return FAT_IO;

  r->pos = r->pos_libre;
  memcpy(r->entree, e, TAILLE_ENTREE);
  r->trouve = true;
  return FAT_OK;
}




static bool est_secteur_demarrage(const uint8_t *b)
{
  if (b[510] != 0x55U || b[511] != 0xAAU) return false;
  if (b[0] != 0xEBU && b[0] != 0xE9U) return false;
  if (lire16(b + 11) != TAILLE_SECTEUR) return false;
  if (b[13] == 0U || (b[13] & (b[13] - 1U)) != 0U) return false;
  return b[16] != 0U && lire16(b + 14) != 0U;
}


static FatEtat fat_monter(void)
{
  memset(&fs, 0, sizeof(fs));
  fatsec_lba = 0xFFFFFFFFUL;

  if (!sd_demarrer()) return FAT_IO;
  if (!sd_lire(0U, sec)) return FAT_IO;


  uint32_t debut_partition = 0U;
  if (!est_secteur_demarrage(sec))
  {
    if (sec[510] != 0x55U || sec[511] != 0xAAU) return FAT_FORMAT;

    bool trouve = false;
    for (int i = 0; i < 4 && !trouve; i++)
    {
      const uint8_t *p = sec + 446 + 16 * i;
      if (p[4] == 0x0B || p[4] == 0x0C)
      {
        debut_partition = lire32(p + 8);
        trouve = true;
      }
    }
    if (!trouve) return FAT_FORMAT;

    if (!sd_lire(debut_partition, sec)) return FAT_IO;
    if (!est_secteur_demarrage(sec)) return FAT_FORMAT;
  }


  if (lire16(sec + 22) != 0U || lire16(sec + 17) != 0U) return FAT_FORMAT;

  uint32_t reserves = lire16(sec + 14);
  uint32_t total    = lire16(sec + 19) ? lire16(sec + 19) : lire32(sec + 32);

  fs.secteurs_cluster = sec[13];
  fs.nb_fats          = sec[16];
  fs.taille_fat       = lire32(sec + 36);
  fs.racine_cluster   = lire32(sec + 44);

  uint16_t info = lire16(sec + 48);
  fs.fsinfo_lba = (info != 0U && info != 0xFFFFU) ? (debut_partition + info) : 0U;

  uint32_t debut_donnees_rel = reserves + fs.nb_fats * fs.taille_fat;
  if (fs.taille_fat == 0U || total <= debut_donnees_rel) return FAT_FORMAT;

  fs.nb_clusters = (total - debut_donnees_rel) / fs.secteurs_cluster;
  fs.fat_lba     = debut_partition + reserves;
  fs.donnees_lba = debut_partition + debut_donnees_rel;
  if (!cluster_valide(fs.racine_cluster)) return FAT_FORMAT;

  fs.prochain_libre = 2U;
  fs.monte = true;
  return FAT_OK;
}

static void fat_demonter(void)
{
  fs.monte = false;
  fatsec_lba = 0xFFFFFFFFUL;
}


static void fat_regler_heure(uint16_t annee, uint8_t mois, uint8_t jour,
                             uint8_t heure, uint8_t minute, uint8_t seconde)
{
  if (annee < 1980U || annee > 2107U || mois < 1U || mois > 12U || jour < 1U || jour > 31U ||
      heure > 23U || minute > 59U || seconde > 59U) return;

  date_fat  = (uint16_t)(((annee - 1980U) << 9) | (mois << 5) | jour);
  heure_fat = (uint16_t)((heure << 11) | (minute << 5) | (seconde / 2U));
}


static FatEtat fat_taille(const char *nom, uint32_t *taille)
{
  uint8_t n11[11];
  Recherche r;

  if (!fs.monte) return FAT_IO;
  if (!nom_83(nom, n11)) return FAT_NOM;

  FatEtat st = chercher(n11, &r);
  if (st != FAT_OK) return st;
  if (!r.trouve) return FAT_NON_TROUVE;

  *taille = lire32(r.entree + 28);
  return FAT_OK;
}


static FatEtat fat_ajouter(const char *nom, const void *donnees, uint32_t longueur)
{
  uint8_t n11[11];
  Recherche r;

  if (!fs.monte) return FAT_IO;
  if (!nom_83(nom, n11)) return FAT_NOM;


  FatEtat st = chercher(n11, &r);
  if (st != FAT_OK) return st;
  if (!r.trouve)
  {
    st = creer_entree(n11, &r);
    if (st != FAT_OK) return st;
  }

  uint32_t taille = lire32(r.entree + 28);
  uint32_t debut  = ((uint32_t)lire16(r.entree + 20) << 16) | lire16(r.entree + 26);
  if (debut == 0U && taille > 0U) return FAT_FORMAT;

  uint32_t octets_cluster = (uint32_t)fs.secteurs_cluster * TAILLE_SECTEUR;
  uint32_t pos = taille;
  uint32_t cl  = debut;


  if (debut != 0U && pos > 0U)
  {
    uint32_t index = (pos - 1U) / octets_cluster;
    for (uint32_t i = 0; i < index; i++)
    {
      uint32_t suivant;
      st = fat_get(cl, &suivant);
      if (st != FAT_OK) return st;
      if (suivant >= FIN_DE_CHAINE || !cluster_valide(suivant)) return FAT_FORMAT;
      cl = suivant;
    }
  }


  const uint8_t *p = (const uint8_t *)donnees;
  uint32_t reste = longueur;

  while (reste > 0U)
  {
    uint32_t dans_cluster = pos % octets_cluster;


    if (debut == 0U || (pos > 0U && dans_cluster == 0U))
    {
      uint32_t nouveau;
      st = fat_alloc(&nouveau);
      if (st != FAT_OK) return st;

      if (debut == 0U) debut = nouveau;
      else
      {
        st = fat_set(cl, nouveau);
        if (st != FAT_OK) return st;
      }
      cl = nouveau;
    }

    uint32_t lba = cluster_lba(cl) + dans_cluster / TAILLE_SECTEUR;
    uint32_t decalage = dans_cluster % TAILLE_SECTEUR;

    if (decalage == 0U) memset(sec, 0, TAILLE_SECTEUR);
    else if (!sd_lire(lba, sec)) return FAT_IO;

    uint32_t n = TAILLE_SECTEUR - decalage;
    if (n > reste) n = reste;
    memcpy(sec + decalage, p, n);

    if (!sd_ecrire(lba, sec)) return FAT_IO;

    p += n;
    pos += n;
    reste -= n;
  }


  if (!sd_lire(r.pos.lba, sec)) return FAT_IO;

  uint8_t *e = sec + r.pos.index * TAILLE_ENTREE;
  ecrire16(e + 20, (uint16_t)(debut >> 16));
  ecrire16(e + 26, (uint16_t)debut);
  ecrire32(e + 28, pos);
  ecrire16(e + 18, date_fat);
  ecrire16(e + 22, heure_fat);
  ecrire16(e + 24, date_fat);

  return sd_ecrire(r.pos.lba, sec) ? FAT_OK : FAT_IO;
}


static FatEtat fat_renommer(const char *ancien_nom, const char *nouveau_nom)
{
  uint8_t ancien[11], nouveau[11];
  Recherche r;

  if (!fs.monte) return FAT_IO;
  if (!nom_83(ancien_nom, ancien) || !nom_83(nouveau_nom, nouveau)) return FAT_NOM;

  FatEtat st = chercher(nouveau, &r);
  if (st != FAT_OK) return st;
  if (r.trouve) return FAT_EXISTE;

  st = chercher(ancien, &r);
  if (st != FAT_OK) return st;
  if (!r.trouve) return FAT_NON_TROUVE;

  if (!sd_lire(r.pos.lba, sec)) return FAT_IO;
  memcpy(sec + r.pos.index * TAILLE_ENTREE, nouveau, 11);
  return sd_ecrire(r.pos.lba, sec) ? FAT_OK : FAT_IO;
}







static uint16_t derniere_annee = 0;
static uint8_t  dernier_mois = 0, dernier_jour = 0;


static void nom_du_jour(char *nom)
{
  RTC_DateTime now;

  if (GroveRTC_GetDateTime(&now))
  {
    derniere_annee = now.year;
    dernier_mois = now.month;
    dernier_jour = now.day;
    fat_regler_heure(now.year, now.month, now.day, now.hours, now.minutes, now.seconds);
  }

  snprintf(nom, 13, "%02u%02u%02u_0.CSV",
           (unsigned)(derniere_annee % 100U), (unsigned)(dernier_mois % 100U), (unsigned)(dernier_jour % 100U));
}

static FatEtat ecrire_ligne(const char *ligne, uint32_t longueur)
{
  char nom[13];
  uint32_t taille = 0;

  if (!fs.monte)
  {
    FatEtat st = fat_monter();
    if (st != FAT_OK) return st;
  }

  nom_du_jour(nom);

  FatEtat st = fat_taille(nom, &taille);
  if (st == FAT_NON_TROUVE) taille = 0;
  else if (st != FAT_OK) return st;


  uint32_t max = (uint32_t)Modes_Config()->file_max_size;
  if (taille > 0U && (taille + longueur) > max)
  {
    if (!SDArchive_Rotate(nom)) return FAT_PLEINE;
  }

  return fat_ajouter(nom, ligne, longueur);
}

bool SensorSD_Init(void)
{
  return fat_monter() == FAT_OK;
}

bool SensorSD_WriteLine(const char *ligne)
{
  if (ligne == NULL) return false;

  uint32_t longueur = (uint32_t)strlen(ligne);
  if (longueur == 0U) return true;

  FatEtat st = ecrire_ligne(ligne, longueur);
  if (st == FAT_OK) return true;


  if (st == FAT_IO || st == FAT_FORMAT)
  {
    fat_demonter();
    return ecrire_ligne(ligne, longueur) == FAT_OK;
  }
  return false;
}



bool SensorSD_FileExists(const char *nom)
{
  uint32_t taille;

  if (!fs.monte && fat_monter() != FAT_OK) return false;
  return fat_taille(nom, &taille) == FAT_OK;
}

bool SensorSD_Rename(const char *ancien_nom, const char *nouveau_nom)
{
  if (!fs.monte && fat_monter() != FAT_OK) return false;
  return fat_renommer(ancien_nom, nouveau_nom) == FAT_OK;
}