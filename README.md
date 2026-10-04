# LIVRABLE 1 — ANALYSE DU SYSTÈME

## 1. EXIGENCES

![contraintes et exigences.](images/exigences.png)
### DESCRIPTION 
Ce diagramme d'exigences regroupe les neuf besoins fondamentaux de la station météo embarquée. Chaque exigence possède un identifiant unique (de `REQ-01` à `REQ-09`) pour faciliter le suivi tout au long du projet.

#### Nous avons les mesures environnementales
- **Mesure de la température** : le système mesure la température de l’air.
- **Mesure de la pression** : le système mesure la pression atmosphérique.
- **Mesure de l’hygrométrie** : le système mesure l’hygrométrie.
- **Mesure de la luminosité** : le système mesure la luminosité et permet de déterminer son niveau.

#### Ensuite nous avons la géolocalisation et l'horodatage de la station météo
- **Acquisition GPS** : le système récupère les données du GPS.
- **Horodatage** : les mesures sont enregistrées avec la date et l’heure.

#### Pour finir nous avons l'enregistrement et l'interfaçage
- **Stockage des données** : les mesures sont enregistrées sur une carte SD.
- **Contrôle utilisateur** : l’utilisateur peut accéder aux différents modes de fonctionnement à l’aide des boutons poussoirs.
- **Signalisation de l’état** : une LED permet d’indiquer l’état du système et de signaler certaines erreurs.


## 2. DIAGRAMME DE CAS D’UTILISATION

### Description

Ce diagramme représente la manière dont l'utilisateur à bord interagit avec la station météo. Nous l'avons construit en trouvant d'abord l'acteur principal, puis en regroupant l'ensemble des actions possibles autour des modes de fonctionnement, de la consultation des données et des réglages du système

![Diagramme de cas d’utilisation](images/diagramme_cas_utilisation.png)

### Résumé de notre démarche de modélisation
Pour concevoir ce diagramme, nous avons suivi trois étapes :
1. **Identification de l'acteur** : nous avons défini un unique acteur principal, le Membre de l'équipage, qui manipule la station directement sur le bateau
2. **Définition des fonctionnalités principales** : nous avons listé les actions indispensables comme la prise de mesure, la sauvegarde sur carte SD, la configuration et la surveillance
3. **Mise en place des règles matérielles** : nous avons associé les changements de modes aux boutons physiques.


### Description du diagramme

#### A. Gestion des modes de fonctionnement
Le système s'articule autour du **Mode Standard** et propose trois autres modes spécifiques accessibles avec les boutons poussoirs[cite: 11] :
* **Démarrer mode standard** : le mode principal dans lequel la station effectue ses mesures et enregistrements normaux[cite: 11].
* **Basculer mode économique** : activé par un **appui de 5 secondes sur le bouton vert** pour réduire la fréquence des relevés et économiser l'énergie[cite: 11].
* **Basculer mode maintenance** : activé par un **appui de 5 secondes sur le bouton rouge** pour permettre les vérifications techniques sur le matériel[cite: 11].
* **Basculer mode configuration** : activé au démarrage en maintenant le **bouton rouge enfoncé**[cite: 11]. Un **retour automatique en mode standard** s'effectue après **30 minutes sans activité** pour éviter de laisser la station bloquée[cite: 11].

#### B. Mesures, affichage et sauvegarde
* **Consulter les mesures** : permet de lire directement les valeurs météo courantes[cite: 11].
* **Acquérir les données des capteurs** : traitement interne qui lit les capteurs avant la consultation ou l'enregistrement[cite: 11].
* **Enregistrer les données sur la carte SD** : sauvegarde automatiquement les relevés pour un traitement ultérieur[cite: 11].
* **Consulter les données via l'interface série** : permet de lire le flux de données en direct en branchant un ordinateur[cite: 11].

#### C. Réglages et surveillance
* **Configurer les paramètres** : permet d'ajuster les options de la station[cite: 11].
* **Surveiller l'état du système** : permet au membre de l'équipage de vérifier le bon fonctionnement général de la station
## 3. DIAGRAMME D’ACTIVITÉ

### Description

Ce diagramme représente le déroulement des opérations réalisées par la station météo, depuis le démarrage jusqu’à la réalisation périodique des mesures.

![Diagramme d’activité](images/Diagramme_d_activite.drawio.png)


## 4. DIAGRAMME DE COMPOSANTS

### Description

Ce diagramme représente les différents composants matériels du **Worldwide Weather Watcher** ainsi que leurs interactions avec la carte **STM32 Nucleo**.

![Diagramme de composants](images/Diagramme_de_composants.drawio.png)


## 5. DIAGRAMME DE SÉQUENCE

### Description

Ce diagramme représente l’ordre chronologique des échanges entre les différents composants du système lors de l’acquisition et du traitement des données.
![Diagramme de composants](images/diagramme_sequence.png)

## SIGNALISATION 

![Signalisation LED_RGB](images/SignalisationLED_RGB.png)

## 7. Gestion et stockage des données

- Les mesures sont enregistrées sur une **carte SD**.
- L’ensemble des mesures est enregistré sur **une seule ligne horodatée**.
- L’intervalle entre deux mesures est de **10 minutes par défaut**, configurable avec `LOG_INTERVAL`.
- Si un capteur ne répond pas dans le délai `TIMEOUT` (**30 s par défaut**), la donnée correspondante est enregistrée comme `NA`.
- La taille maximale du fichier est définie par `FILE_MAX_SIZE` (**2 ko par défaut**).
- Les fichiers utilisent le format de nommage `200531_0.LOG` :
  - `20` : année
  - `05` : mois
  - `31` : jour
  - `0` : numéro de révision
- Le système écrit toujours dans le fichier de **révision 0**.
- Lorsque le fichier est plein, le système crée une **copie avec un numéro de révision adapté**, puis recommence à enregistrer les données dans le fichier de révision 0.
- En **mode maintenance**, les données ne sont plus écrites sur la carte SD mais peuvent être consultées directement depuis le **port série**.
- La carte SD peut alors être retirée et replacée en toute sécurité.
- En cas de carte SD pleine ou d’erreur d’accès/écriture, le système utilise le signal lumineux prévu.
  
## SOURCES
• Sujet du projet Worldwide Weather Watcher fourni dans la demande.
• Document « A2 – Projet Système embarqué – Modes de fonctionnement » fourni avec la demande.
• https://lucid.co/fr/diagramme/uml
