# Stores enrouleurs — XIAO ESP32-C3 / S3 — v7.4

Commande d'un ou deux stores enrouleurs par moteurs pas à pas 28BYJ-48, depuis une page web servie par la carte elle-même. Aucune application ni service extérieur (plus de Blynk) : un navigateur suffit, sur téléphone ou ordinateur.

Plusieurs cartes peuvent être installées dans la maison : chacune garde sa propre page, et la page **« Toutes les cartes »** (`/tous`) les réunit toutes sur un seul écran.

---

## Fonctions

- **1 ou 2 moteurs par carte**, en deux modes au choix (sur la page web) :
  - **Indépendants** : deux stores séparés, chacun avec sa position, ses butées, sa calibration, sa vitesse et son nom.
  - **Synchronisés** : un seul store entraîné des deux côtés ; les deux moteurs font exactement les mêmes pas. Une seule calibration. Carte « Alignement » pour remettre le store droit.
- **Monter / Stop / Descendre** et **Aller à X %**, avec butées haute et basse logicielles.
- **Calibration** en 3 clics (Démarrer, Valider HAUT, Valider BAS).
- **Rampe** d'accélération au départ et de décélération avant l'arrivée.
- **Réglages sur la page** : nom de chaque store, sens de rotation, vitesse (curseur), mode indépendants / synchronisés (protégé par mot de passe).
- **Mémoire permanente** (NVS) : position sauvegardée pendant le mouvement, calibration, réglages et compteurs conservés après une coupure de courant.
- **Page d'informations** : version, durée de fonctionnement, cause du dernier démarrage, réseau et signal Wi-Fi, mémoire, compteurs d'usure de chaque moteur.
- **Mise à jour sans câble (OTA)** : depuis l'Arduino IDE, ou depuis le navigateur (`/update`), protégée par mot de passe.
- **Point d'accès de secours** si la box est introuvable (adresse `192.168.5.1`), avec retour automatique vers la box toutes les 2 minutes.
- **Plusieurs cartes** : découverte automatique des autres cartes (mDNS), page commune `/tous` avec « Tout monter / Tout arrêter / Tout descendre / Aller à X % ».
- **Icône et nom propres à chaque carte** pour le raccourci « Ajouter à l'écran d'accueil » du téléphone.

## Matériel

| Élément | Quantité | Remarque |
|---|---|---|
| Seeed Studio XIAO ESP32-C3 (ou XIAO ESP32-S3) | 1 par carte | Même brochage, même programme |
| Moteur pas à pas 28BYJ-48 (5 V) | 1 ou 2 | 4096 demi-pas par tour de l'axe de sortie |
| Driver ULN2003 | 1 par moteur | Carte fournie avec le 28BYJ-48 |
| Alimentation 5 V | 1 | **1 A minimum** pour 2 moteurs |

La masse (GND) de l'alimentation doit être **commune** avec celle du XIAO.

## Câblage

| Driver | IN1 | IN2 | IN3 | IN4 |
|---|---|---|---|---|
| Moteur 1 (ULN2003 n°1) | D2 | D3 | D4 | D5 |
| Moteur 2 (ULN2003 n°2) | D1 | D6 | D7 | D10 |

- Alimentation 5 V externe sur le + des deux drivers, GND commun avec le XIAO.
- Broches libres : D0, D8, D9 (sur le C3 ce sont des broches de démarrage : à réserver à des boutons avec résistance de rappel au +3,3 V).
- D6/D7 servent au moteur 2 : le moniteur série passe donc par l'USB (voir ci-dessous).

## Installation

### 1. Arduino IDE

- Paquet **esp32** d'Espressif (versions 2.x et 3.x acceptées).
- Outils > Type de carte : **XIAO_ESP32C3** (ou **XIAO_ESP32S3**).
- Outils > USB CDC On Boot : **Enabled** (obligatoire pour le moniteur série).
- Outils > Partition Scheme : un schéma **avec OTA** (celui par défaut convient).

Aucune bibliothèque à installer : tout est inclus dans le paquet ESP32.

### 2. Fichier `arduino_secrets.h`

À placer dans le même dossier que le `.ino` :

```cpp
#define SECRET_SSID     "NomDeVotreBox"
#define SECRET_PASS     "MotDePasseWiFi"
#define SECRET_AP_SSID  "Store1-Secours"     // différent sur chaque carte
#define SECRET_AP_PASS  "8caracteresmini"
#define SECRET_OTA_PASS "MotDePasseOTA"      // mise à jour + changement de mode
```

### 3. Réglages à faire dans le `.ino`

Les constantes marquées 🚩 dans le programme. Les principales :

| Constante | Rôle | Valeur par défaut |
|---|---|---|
| `HOSTNAME` | Nom réseau → `http://<nom>.local` — **différent sur chaque carte** | `"store"` |
| `PAGE_TITLE` | Titre de la page et du raccourci | `"Stores"` |
| `ICON_COLOR` | Couleur de l'icône (`#RRGGBB`) | `"#3d6fb6"` |
| `ALL_TITLE` | Titre de la page `/tous` | `"Tous les stores"` |
| `FIXED_PEERS` | IP des autres cartes si la box bloque la découverte | `""` |
| `MOTOR2_ENABLED` | `false` = un seul moteur | `true` |
| `SYNC_DEFAULT` | Mode au tout premier démarrage | `false` (indépendants) |
| `MOT1_SIDE` / `MOT2_SIDE` | Titres des cadres des moteurs | `"Store Gauche"` / `"Store Droit"` |
| `RAMP_START_US` / `RAMP_STEPS` | Douceur de la rampe | `6000` / `24` |

Le sens, la vitesse, les noms et le mode se règlent ensuite **sur la page web** et sont enregistrés dans la carte.

### 4. Premier téléversement

Par câble USB. Le moniteur série (115200 bauds) affiche l'adresse IP. Les téléversements suivants peuvent se faire par OTA.

### 5. Plusieurs cartes

Même programme sur chaque carte ; changer avant chaque téléversement `HOSTNAME`, `PAGE_TITLE`, `ICON_COLOR` et `SECRET_AP_SSID`. Exemple pour 3 cartes : `store1` / `store2` / `store3`.

## Adresses

| Adresse | Contenu |
|---|---|
| `http://store.local/` | Page de la carte (commandes, calibration, réglages) |
| `http://store.local/tous` | Toutes les cartes sur une page |
| `http://store.local/info` | Informations et compteurs |
| `http://store.local/update` | Mise à jour du programme par le navigateur |
| `http://192.168.5.1/` | Même page, en mode point d'accès de secours |

Si `.local` ne fonctionne pas (certains Android), utiliser l'adresse IP, de préférence réservée dans la box.

## Documentation

- **Notice technique** (PDF) : fonctionnement interne, réglages, mémoire, réseau, API, dépannage.
- **Manuel d'utilisation de la page web** (PDF) : toutes les commandes, pas à pas.

## Historique résumé

| Version | Apport principal |
|---|---|
| v2 | Sauvegarde de la position pendant le mouvement, OTA |
| v3 – v3.1 | Deuxième moteur ; OTA fiabilisée (puissance Wi-Fi réduite), page `/update` |
| v4 – v4.2 | Moteurs indépendants ; page sur 2 colonnes |
| v5 | Réglage du sens et de la vitesse sur la page ; point d'accès de secours avec retour vers la box |
| v6 – v6.2 | Rampe ; renommage ; page `/info` et compteurs ; cadres gauche / droit |
| v7 – v7.2 | Mode synchronisé, alignement ; changement de mode protégé par mot de passe (fenêtre) |
| v7.3 – v7.3.4 | Page plus fluide (file d'attente des requêtes) ; titre et icône propres à chaque carte |
| **v7.4** | **Page `/tous` : toutes les cartes sur une page, découverte automatique** |

Le détail de chaque version est en tête du fichier `.ino`.
