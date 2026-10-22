# Station météo PACA

Station météo embarquée basée sur une carte STM32 Nucleo-L476RG. Le système
mesure les conditions environnementales, les horodate, récupère la position GPS
et enregistre les mesures sur une carte SD.

## Fonctionnalités

- Mesure de la température, de l'humidité et de la pression avec le BME680.
- Mesure de la résistance du gaz du BME680.
- Mesure de la luminosité avec détection du niveau faible, moyen ou fort.
- Acquisition de la position GPS.
- Horodatage avec une RTC DS1307.
- Enregistrement des mesures sur carte SD au format CSV.
- Configuration par port série.
- Quatre modes de fonctionnement.
- Signalisation des états et des erreurs par LED RGB.
- Sauvegarde des paramètres en mémoire Flash.

## Architecture du projet

```text
src/
  main.c             Boucle principale et acquisition des mesures
  modes.c            Modes, boutons, console et configuration Flash
  sensor_bme680.c    Lecture et conversion du BME680
  sensor_gps.c       Lecture et validation du GPS
  sensor_light.c     Lecture du capteur de luminosité
  sensor_rtc.c       Lecture et réglage de la RTC
  sensor_sd.c        Accès SPI, FAT32 et écriture sur carte SD
  sd_archive.c       Rotation des fichiers CSV pleins
  status_led.c       Motifs et états de la LED RGB
  board.c            Initialisation de la carte et port série
  setup.c            Configuration des périphériques STM32

include/             Interfaces et structures partagées
lib/cesi_grove/      Pilotes matériels Grove
UML-Diagram/         Diagrammes de conception
```

## Matériel et communications

| Composant | Fonction | Interface |
|---|---|---|
| STM32 Nucleo-L476RG | Contrôleur principal | — |
| BME680 | Température, humidité, pression, gaz | I2C |
| GPS Air530Z | Latitude et longitude | UART |
| RTC DS1307 | Date et heure | I2C |
| Capteur de luminosité | Luminosité | I2C |
| Carte SD | Stockage des mesures | SPI |
| Boutons poussoirs | Sélection des modes | GPIO |
| LED RGB | États et erreurs | GPIO |

## Modes de fonctionnement

### Mode standard

Le système réalise les mesures à l'intervalle configuré, les affiche sur le
port série et les écrit sur la carte SD. La LED est verte fixe.

### Mode économique

Le délai entre deux mesures est doublé et le GPS est interrogé une mesure sur
deux. La LED est bleue fixe.

### Mode maintenance

Les mesures restent visibles sur le port série, mais aucune donnée n'est
écrite sur la carte SD. La carte peut être retirée pendant ce mode. La LED est
orange-rouge fixe.

### Mode configuration

Le mode est sélectionné au démarrage en maintenant le bouton rouge. Les
paramètres sont modifiables avec la console série. Après 30 minutes sans
activité, le système revient automatiquement au mode standard. La LED est
orange fixe.

Les changements de mode se font avec un appui long de cinq secondes :

- bouton rouge en mode standard : mode maintenance ;
- bouton vert en mode standard : mode économique ;
- bouton rouge en mode économique : mode standard ;
- bouton rouge en mode maintenance : retour au mode précédent.

## Configuration par port série

Le port série fonctionne à `115200 bauds`. Les commandes sont saisies sous la
forme `PARAMETRE=VALEUR`.

Exemples :

```text
VERSION
LOG_INTERVAL=10
FILE_MAX_SIZE=4096
TEMP_AIR=1
MIN_TEMP_AIR=-10
MAX_TEMP_AIR=60
HYGR=1
HYGR_MINT=0
HYGR_MAXT=50
PRESSURE=1
PRESSURE_MIN=850
PRESSURE_MAX=1080
LUMIN=1
LUMIN_LOW=255
LUMIN_HIGH=768
TIMEOUT=5
RESET
```

Les paramètres sont contrôlés par des limites, puis sauvegardés dans la
dernière page de la mémoire Flash. `RESET` restaure les valeurs par défaut.

## Format des mesures

Une mesure est enregistrée sur une ligne, avec des champs séparés par des
points-virgules :

```text
date;latitude,longitude;temperature;humidite;pression;gaz;luminosite
```

Exemple :

```text
2026-10-22 20:57:28;43.2965,5.3698;22.4;58.2;1013.5;12500.0;FAIBLE
```

Une donnée indisponible est enregistrée sous la forme `NA`. La valeur du gaz
correspond à la résistance du capteur BME680, exprimée en ohms.

## Archivage automatique des fichiers

Les fichiers sont enregistrés sur la carte SD avec des noms courts compatibles
FAT 8.3 :

```text
AAMMJJ_0.CSV
```

Par exemple, le fichier actif du 22 octobre 2026 est `261022_0.CSV`.
Lorsque sa taille dépasse `FILE_MAX_SIZE`, le programme C de
[src/sd_archive.c](src/sd_archive.c) renomme le fichier plein avec le premier
numéro de révision disponible :

```text
261022_1.CSV
261022_2.CSV
261022_3.CSV
```

Le fichier `261022_0.CSV` est recréé automatiquement lors de la prochaine
écriture. Le contenu n'est pas recopié : l'archivage modifie directement le
nom de l'entrée dans le répertoire FAT.

La fonction principale est :

```c
bool SDArchive_Rotate(const char *nom_fichier);
```

Elle vérifie le nom du fichier, recherche une révision libre de 1 à 9 et
appelle `SensorSD_Rename()`. Si les neuf révisions sont occupées ou si la carte
SD n'est pas accessible, la fonction renvoie `false` et le système signale
l'erreur par la LED.

## Gestion des erreurs

Une erreur est signalée après deux échecs consécutifs afin d'éviter les faux
positifs :

| État | Motif LED |
|---|---|
| Erreur RTC | Rouge et bleu |
| Erreur GPS | Rouge et orange |
| Erreur capteur | Rouge et vert |
| Mesure incohérente | Clignotement rouge/vert |
| Carte SD pleine | Rouge et blanc |
| Erreur d'écriture SD | Clignotement rouge/blanc |

## Compilation et téléversement

Le projet utilise PlatformIO et le framework STM32Cube.

```bash
pio run
pio run --target upload
pio device monitor
```

La configuration de la carte et du port série se trouve dans
[platformio.ini](platformio.ini).

## Diagrammes UML

- [Diagramme des exigences](UML-Diagram/requirement-diagram.png)
- [Diagramme des cas d'utilisation](UML-Diagram/use-case-diagram.png)
- [Diagramme d'activité](UML-Diagram/activity-diagram.png)
- [Diagramme de composants](UML-Diagram/component-diagram.png)
- [Diagramme de séquence](UML-Diagram/sequence-diagram.png)
- [Signalisation LED](UML-Diagram/RGB-LED-signaling.png)

## Références

- Sujet du projet *Worldwide Weather Watcher*.
- Document « A2 – Projet Système embarqué – Modes de fonctionnement ».
- [Documentation PlatformIO](https://docs.platformio.org/).
- [Documentation UML](https://lucid.co/fr/diagramme/uml).

## Équipe

| Membre | Rôle | GitHub |
|---|---|---|
| Aymane TAHANI | Diagrammes d'activité et d'exigences, rédaction | [@ayamithn](https://github.com/ayamithn) |
| Alexandre ALIAS | Diagramme de composants | [@QLEEEX](https://github.com/QLEEEX) |
| Pierre DE JOUVENCEL | Diagramme de cas d'utilisation | [@pierrejouvencel08-ship-it](https://github.com/pierrejouvencel08-ship-it) |
| Clément BOUYSSOU | Diagramme de séquence, rédaction | [@clement-bsso](https://github.com/clement-bsso) |
