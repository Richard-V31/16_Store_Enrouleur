# Stores enrouleurs — XIAO ESP32-C3 — 2 moteurs indépendants ou synchronisés (v7.2)

Commande de **deux stores enrouleurs** (ou d'**un grand store entraîné des deux côtés**) par moteurs pas à pas (driver ULN2803) depuis une **page web** servie directement par un **Seeed Studio XIAO ESP32-C3**. Aucun service externe (pas de Blynk, pas de cloud) : la carte se pilote depuis n'importe quel navigateur du réseau local.

Fichier du croquis : `StoreEnrouleur_Web_XIAO_C3_2moteurs.ino`

---

## Fonctions

- **Deux modes de fonctionnement au choix**, depuis la page web (v7), enregistrés en NVS :
  - **Indépendants** : deux stores séparés ; position, sens, vitesse, nom, butées, calibration et sauvegarde propres à chaque moteur.
  - **Synchronisés** : un seul store entraîné par les deux moteurs, qui font exactement les mêmes demi-pas (même rampe, même instant). Une seule calibration, une seule position, la vitesse du moteur 1 ; chaque moteur garde son propre sens (moteurs montés face à face).
- **Alignement des deux côtés** (mode synchronisé) : un seul moteur tourne, store à l'arrêt, pour remettre le store droit sans changer la position affichée.
- **Commande groupée « Les deux Stores »** (mode indépendant) : Monter / Stop / Descendre / Aller à X % pour les deux stores en un clic.
- **Page web en pleine largeur** qui s'adapte au mode et au nombre de moteurs :
  - indépendants : les commandes de chaque store sont dans un **cadre « Moteur gauche » / « Moteur droit »** (v6.2), côte à côte sur 2 colonnes (boutons ▲ ■ ▼) ;
  - synchronisés : un seul cadre « Moteur gauche + Moteur droit — synchronisés » en pleine largeur (boutons « ▲ Monter / ■ Stop / ▼ Descendre ») ;
  - 1 moteur : un seul cadre en pleine largeur.
  
  La calibration et le menu « Réglage Moteurs » sont toujours en pleine largeur, sous les commandes.
- **Menu « Réglage Moteurs »** :
  - **choisir le fonctionnement** : Indépendants / Synchronisés (v7, avec 2 moteurs), **protégé par le mot de passe OTA** (v7.1) ;
  - **renommer chaque store** (« Salon », « Chambre »…) sans recompiler ;
  - **inverser le sens de rotation** de chaque moteur (efface la calibration, à refaire) ;
  - **régler la vitesse** par un curseur, de ≈ 1,5 à ≈ 7,3 tr/min.
  
  Mode, noms, sens et vitesses sont enregistrés en NVS et retrouvés au redémarrage.
- **Rampe d'accélération et de décélération** (v6) : démarrage en douceur, ralentissement avant l'arrivée (butée ou position demandée). En synchronisé, la même rampe s'applique aux deux moteurs.
- **Aller à une position** en pourcentage (0 % = haut, 100 % = bas). Le curseur démarre sur la position actuelle du store.
- **Butées logicielles** haute et basse définies par une calibration en 3 étapes.
- **Sauvegarde de la position en mémoire non volatile (NVS)** à chaque arrêt et tous les 500 pas pendant le mouvement (protection contre les coupures de courant).
- **Page d'informations** `/info` (v6) : version, mode des moteurs, durée de fonctionnement, cause du dernier démarrage, réseau, mémoire, et **compteurs par moteur** (mouvements, tours, cycles complets), avec remise à zéro.
- **Moteurs pilotés par un temporisateur matériel** (`esp_timer`, top toutes les 0,5 ms) : la rotation reste régulière même quand le Wi-Fi est occupé.
- **Bobines coupées à l'arrêt** : les moteurs ne chauffent pas.
- **Mise à jour à distance (OTA)** depuis l'Arduino IDE (port réseau) ou depuis le navigateur (`/update`), protégée par **un seul mot de passe**.
- **Point d'accès de secours** (192.168.5.1) si la box est introuvable, avec **retour automatique sur la box** (nouvel essai toutes les 2 minutes).
- **Accès par nom** : `http://store.local` (mDNS).
- **Indicateur de force du signal Wi-Fi** sur la page.

---

## Matériel

| Élément | Quantité | Remarque |
|---|---|---|
| Seeed Studio XIAO ESP32-C3 | 1 | avec son antenne externe branchée |
| Moteur pas à pas 5 V unipolaire (type 28BYJ-48) | 1 ou 2 | en synchronisé : un de chaque côté du store |
| Driver ULN2803 SOP18 | 1 | un pour les 2 moteurs |
| Alimentation 5 V | 1 | **1 A minimum** pour deux moteurs |
| Fils de liaison | — | masse commune obligatoire |

---

## Câblage

| Driver | IN1 | IN2 | IN3 | IN4 |
|---|---|---|---|---|
| Moteur 1 — gauche (ULN2803 n°1a) | D2 (GPIO4) | D3 (GPIO5) | D4 (GPIO6) | D5 (GPIO7) |
| Moteur 2 — droit (ULN2803 n°1b) | D1 (GPIO3) | D6 (GPIO21) | D7 (GPIO20) | D10 (GPIO10) |

- Le câblage est **le même dans les deux modes** : seul le réglage sur la page change.
- Alimentation 5 V externe sur les **+ / −** des deux drivers.
- **GND de l'alimentation relié au GND du XIAO** (masse commune).
- Ne pas alimenter les moteurs par la broche 5 V du XIAO.
- Broches restées libres : **D0, D8, D9** (broches de démarrage : à réserver à des boutons avec résistance de rappel au +3,3 V).
- D6/D7 étant le port série matériel, le moniteur série passe par l'**USB intégré** (voir réglages ci-dessous).

---

## Installation

### 1. Arduino IDE

1. Installer le paquet **esp32 d'Espressif** (Gestionnaire de cartes). Testé avec la version **3.3.12**.
2. Carte : **XIAO_ESP32C3**.
3. **Outils > USB CDC On Boot > Enabled** (obligatoire pour voir le moniteur série).
4. **Outils > Partition Scheme** : un schéma avec OTA (celui par défaut convient).

Aucune bibliothèque supplémentaire : `WiFi`, `WebServer`, `ESPmDNS`, `Preferences`, `ArduinoOTA`, `Update` font partie du paquet ESP32.

### 2. Fichier `arduino_secrets.h`

À placer dans le dossier du croquis :

```cpp
#define SECRET_SSID      "NomDeLaBox"
#define SECRET_PASS      "MotDePasseWiFi"
#define SECRET_AP_SSID   "Store-Secours"
#define SECRET_AP_PASS   "8caracteresMini"
#define SECRET_OTA_PASS  "MonMotDePasseOTA"   // facultatif (défaut : "store-ota", à changer)
```

### 3. Réglages dans le programme

Le **mode**, le **sens**, la **vitesse** et le **nom** se règlent sur la page web. Les constantes correspondantes ne servent que de **valeurs de départ**, au tout premier démarrage (ou pour le nom, quand on l'efface sur la page).

| Constante | Défaut | Rôle |
|---|---|---|
| `FW_VERSION` | `"7.2"` | version affichée sur la page d'informations (à changer à chaque version) |
| `MOTOR2_ENABLED` | `true` | `false` = fonctionnement avec un seul moteur (la page passe en une colonne, pas de mode synchronisé) |
| `SYNC_DEFAULT` | `false` | mode au premier démarrage : `false` = indépendants, `true` = synchronisés (ensuite : menu « Réglage Moteurs ») |
| `MOT1_INVERT` / `MOT2_INVERT` | `false` | sens de départ (ensuite : menu « Réglage Moteurs ») |
| `MOT1_NAME` / `MOT2_NAME` | `"Store 1"` / `"Store 2"` | nom de départ (ensuite : menu « Réglage Moteurs ») |
| `MOT1_SIDE` / `MOT2_SIDE` | `"Moteur gauche"` / `"Moteur droit"` | titres des cadres sur la page (côté, emplacement…) |
| `STEP_PERIOD_US` | `3000` | vitesse de départ, en µs par demi-pas (ensuite : curseur) |
| `SPEED_MIN_US` / `SPEED_MAX_US` | `2000` / `10000` | limites du curseur de vitesse (le plus rapide / le plus lent) |
| `RAMP_START_US` | `6000` | vitesse de départ et d'arrivée de la rampe (aussi vitesse de l'alignement) |
| `RAMP_STEPS` | `24` | demi-pas par cran de 0,5 ms : plus grand = rampe plus douce ; `0` = sans rampe |
| `JOG_MAX` | `4096` | alignement : déplacement maximal en attente pour un moteur (1 tour) |
| `SAVE_EVERY_STEPS` | `500` | sauvegarde intermédiaire pendant le mouvement |
| `HOSTNAME` | `"store"` | nom réseau → `http://store.local` |
| `AP_IP` | `192.168.5.1` | adresse en mode point d'accès |
| `STA_RETRY_MS` | `120000` | en mode secours, intervalle entre deux essais de connexion à la box |

### 4. Premier téléversement

Téléverser par USB, ouvrir le moniteur série à **115200 bauds** et relever l'adresse IP affichée.

---

## Utilisation

1. Ouvrir `http://store.local` (ou l'adresse IP).
2. **Choisir le mode** (avec 2 moteurs) : menu **Réglage Moteurs > Fonctionnement**, cliquer sur **Indépendants** ou **Synchronisés**, puis saisir le **mot de passe OTA** dans la fenêtre qui s'ouvre.
3. **Vérifier le sens de chaque moteur** (▲ doit faire monter) et l'inverser si besoin dans **Réglage Moteurs** — une inversion efface la calibration.
4. En synchronisé seulement : **aligner** le store (carte « Alignement des deux côtés »).
5. **Calibrer** (carte « Calibration », en bas de la page, à déplier) :
   1. **Démarrer** (les butées sont désactivées) ;
   2. **Monter** (▲) jusqu'en haut, **Stop** (■), **Valider HAUT** ;
   3. **Descendre** (▼) jusqu'en bas, **Stop** (■), **Valider BAS**.
   
   En indépendant : une calibration par moteur. En synchronisé : une seule, commune.
6. Utiliser ensuite Monter / Stop / Descendre / Aller à X %.

Disposition de la page, de haut en bas :

| 2 moteurs indépendants | 2 moteurs synchronisés | 1 moteur |
|---|---|---|
| Connexion | Connexion | Connexion |
| Les deux Stores | — | — |
| Cadre « Moteur gauche » \| cadre « Moteur droit » (boutons ▲ ■ ▼) | Cadre « Moteur gauche + Moteur droit — synchronisés » (boutons avec texte) | Cadre « Moteur » (boutons avec texte) |
| Calibration — Store 1 | Calibration — Store 1 + Store 2 (commune) | Calibration — Store 1 |
| Calibration — Store 2 | Alignement des deux côtés | — |
| Réglage Moteurs (Fonctionnement ; Store 1 \| Store 2) | Réglage Moteurs (Fonctionnement ; Store 1 \| Store 2) | Réglage Moteurs (Store 1) |
| liens « Informations » et « Mise à jour du programme » | idem | idem |

Sur les boutons réduits aux symboles, le nom de la commande apparaît en info-bulle au survol. La page se reconstruit d'elle-même quand on change de mode.

### Mode synchronisé

| Point | Fonctionnement |
|---|---|
| Mouvement | les deux moteurs font chaque demi-pas en même temps, dans la même tâche du temporisateur : ils ne peuvent pas se décaler pendant un mouvement |
| Commandes | toutes les commandes (Monter, Stop, Descendre, Aller, calibration) pilotent les deux moteurs |
| Calibration | une seule, commune aux deux moteurs |
| Vitesse | celle du moteur 1 (le curseur du moteur 2 est masqué) |
| Sens | propre à chaque moteur ; une inversion efface la calibration commune |
| Passage indépendants → synchronisés | moteurs arrêtés, **calibration effacée** (à refaire une fois) |
| Passage synchronisés → indépendants | chaque moteur **garde la calibration commune** |

### Alignement des deux côtés (mode synchronisé)

Si le store n'est pas droit (un côté plus enroulé que l'autre), déplier la carte **Alignement des deux côtés**. Pour chaque moteur :

| Bouton | Effet |
|---|---|
| ▲▲ / ▼▼ | 1/8 de tour (512 demi-pas) vers le haut / le bas |
| ▲ / ▼ | petit pas (64 demi-pas, ≈ 6°) |

- Un seul moteur tourne, lentement (6 ms par demi-pas), **store à l'arrêt** seulement.
- La position affichée ne change pas. Les clics s'additionnent (1 tour maximum en attente).
- Un mouvement du store ou **Stop** annule l'alignement en cours. L'état affiche « Alignement en cours ».
- À faire de préférence **avant la calibration**.

### Menu « Réglage Moteurs »

Toucher le titre « Réglage Moteurs » pour le déplier.

| Réglage | Fonctionnement |
|---|---|
| **Fonctionnement** | cliquer sur **Indépendants** ou **Synchronisés** : une fenêtre rappelle les conséquences et demande le **mot de passe OTA** (`SECRET_OTA_PASS`). **Valider** (ou Entrée) applique, **Annuler** (ou Échap) ferme sans rien changer. Mot de passe faux : « Mot de passe incorrect : mode inchangé », la fenêtre reste ouverte. Visible seulement avec 2 moteurs. Les autres réglages ne demandent pas de mot de passe. |
| **Nom** | saisir le nom puis **Renommer**. 32 octets maximum (une lettre accentuée compte pour 2) ; les caractères `" \ < > &` sont retirés ; un nom vide redonne le nom par défaut. |
| **Sens** | **Inverser le sens** puis confirmer. Le moteur s'arrête et **sa calibration est effacée** (en synchronisé : la calibration commune). |
| **Vitesse** | curseur de « Lent, plus de force » à « Rapide », par crans de 0,5 ms (affichage en tr/min et en ms par demi-pas). Appliquée tout de suite, même en mouvement ; enregistrée quand on relâche le curseur. En synchronisé : seul le curseur du moteur 1 est affiché. |

Plus le moteur est lent, plus il a de couple. Si un moteur vibre sans tourner ou perd des pas, baisser sa vitesse puis recalibrer.

### Rampe

- Chaque mouvement démarre à 6 ms par demi-pas (≈ 2,4 tr/min), puis accélère d'un cran de 0,5 ms tous les 24 demi-pas jusqu'à la vitesse réglée.
- Avant l'arrivée (butée haute, butée basse ou position « Aller à »), le moteur ralentit de la même façon.
- Si la vitesse réglée est déjà plus lente que 6 ms, il n'y a pas de rampe.
- **Stop** reste un arrêt immédiat. Pas de décélération pendant la calibration (l'arrivée n'est pas connue).

### Page d'informations (`/info`)

Rafraîchie toutes les 2 s :

| Cadre | Contenu |
|---|---|
| Programme | version, date de compilation, durée de fonctionnement, cause du dernier démarrage, mode des moteurs (indépendants / synchronisés), processeur |
| Réseau | mode (box ou secours), nom du réseau, adresse IP, nom `.local`, signal, adresse MAC |
| Mémoire | mémoire libre, minimum atteint, taille du programme, place pour une mise à jour |
| Compteurs (un par moteur) | mouvements, tours du moteur, cycles complets montée + descente ; bouton **Remettre à zéro** |

En synchronisé, les compteurs des deux moteurs avancent ensemble ; les pas d'alignement sont comptés pour le seul moteur concerné.

La cause « **baisse de tension de l'alimentation** » signale une alimentation 5 V trop faible pour les moteurs.

---

## Mise à jour du programme (OTA)

- **Arduino IDE** : la carte apparaît dans *Outils > Port* sous le nom `store` (port réseau 3232). Téléverser normalement, saisir le mot de passe OTA.
- **Navigateur** : *Croquis > Exporter les binaires compilés*, puis ouvrir `http://store.local/update`, saisir le mot de passe OTA et envoyer le fichier **`.ino.bin`** (pas les fichiers `merged` ni `bootloader`).

Pendant une mise à jour, les moteurs sont arrêtés, les positions et compteurs sauvegardés et toute commande est refusée. Le mode, les noms, sens et vitesses sont conservés. Pour vérifier qu'une mise à jour a bien pris, regarder la version sur la page d'informations.

---

## API HTTP

| Adresse | Rôle |
|---|---|
| `GET /` | page de commande |
| `GET /info` | page d'informations |
| `GET /api/status` | état en JSON |
| `GET /api/cmd?a=<action>&m=<moteur>[&p=<valeur>]` | commande |
| `GET /api/set?m=<1 ou 2>[&inv=<0 ou 1>][&per=<µs>][&name=<nom>]` | réglages d'un moteur |
| `POST /api/set` avec `sync=<0 ou 1>&pw=<mot de passe OTA>` | mode : 0 = indépendants, 1 = synchronisés (401 si mot de passe faux) |
| `GET /api/info[?reset=<1 ou 2>]` | informations en JSON (avec `reset` : remise à zéro des compteurs) |
| `GET /update` / `POST /update?pw=<mdp>` | mise à jour par le navigateur |

- `m` : `1` ou `2` = un moteur, `0` = les deux. En synchronisé, `m` est ignoré : toute commande pilote les deux moteurs.
- `a` : `up`, `down`, `stop`, `goto` (avec `p` = 0 à 100), `calstart`, `caltop`, `calbot` (en indépendant : `m` = 1 ou 2 obligatoire).
- `a=jog` (synchronisé seulement) : alignement, `m` = 1 ou 2, `p` = nombre de demi-pas (négatif = monter), limité à ±4096.
- `per` : durée d'un demi-pas en µs, ramenée entre 2000 et 10000 et arrondie à 500.

Exemples :
- `http://store.local/api/cmd?a=goto&m=0&p=30` → les deux stores à 30 %.
- `curl -d "sync=1&pw=MonMotDePasseOTA" http://store.local/api/set` → passage en moteurs synchronisés (POST : le mot de passe ne figure pas dans l'adresse ; un GET avec `&pw=` fonctionne aussi).
- `http://store.local/api/cmd?a=jog&m=2&p=-64` → synchronisé : le moteur 2 seul remonte d'un petit pas.
- `http://store.local/api/set?m=1&per=5000` → moteur 1 à 5 ms par demi-pas (≈ 2,9 tr/min).
- `http://store.local/api/set?m=2&name=Chambre` → le store 2 s'appelle « Chambre ».

Réponse de `/api/status` :

```json
{"ota":false,"ap":false,"sync":false,"rssi":-58,"pmin":2000,"pmax":10000,"ptick":500,"spr":4096,"m":[
 {"name":"Salon","side":"Moteur gauche","pct":42,"step":1700,"travel":4050,"dir":0,"cal":false,"top":false,"ready":true,"inv":false,"per":3000,"jog":0},
 {"name":"Chambre","side":"Moteur droit","pct":0,"step":0,"travel":3980,"dir":-1,"cal":false,"top":false,"ready":true,"inv":true,"per":5000,"jog":0}]}
```

`sync` indique le mode ; `side` le titre du cadre ; `jog` les demi-pas d'alignement restant à faire.

---

## Mémoire NVS (espace « roller »)

| Clé | Contenu |
|---|---|
| `sync` | mode : indépendants / synchronisés (v7) |
| `travel1` / `travel2` | course totale en pas |
| `pos1` / `pos2` | dernière position |
| `inv1` / `inv2` | sens (v5) |
| `per1` / `per2` | vitesse en µs par demi-pas (v5) |
| `name1` / `name2` | nom (v6) |
| `steps1` / `steps2`, `moves1` / `moves2` | compteurs (v6) |
| `travelSteps` / `position` | anciennes clés v3.1, lues seulement si les nouvelles n'existent pas |

En synchronisé, la course et la position du moteur 1 font foi ; celles du moteur 2 en sont la copie.

---

## Mise à niveau

- **Depuis la v7.0 / v7.1** : aucune action ; le changement de mode demande le mot de passe OTA, saisi dans une fenêtre.
- **Depuis la v6.x** : aucune action. La carte démarre en mode `SYNC_DEFAULT` (indépendants) avec les calibrations, noms, sens, vitesses et compteurs existants.
- **Depuis la v5** : aucune action. Les noms partent de `MOT1_NAME` / `MOT2_NAME`, les compteurs partent de zéro. Calibrations, sens et vitesses sont conservés.
- **Depuis la v4.x** : aucune action. Le sens et la vitesse partent de `MOTx_INVERT` et `STEP_PERIOD_US` au premier démarrage, puis se règlent sur la page. Les calibrations sont conservées.
- **Depuis la v3.1** : au premier démarrage, la course (`travelSteps`) et la position (`position`) communes de la v3.1 sont recopiées pour chacun des deux moteurs. Pour retrouver le fonctionnement de la v3 (un store entraîné des deux côtés), passer en **Synchronisés** puis calibrer.

---

## Dépannage rapide

| Symptôme | Solution |
|---|---|
| Un moteur tourne à l'envers | « Réglage Moteurs » > **Inverser le sens**, puis recalibrer |
| « non calibré » | lancer la calibration (normal aussi juste après une inversion de sens ou un passage en synchronisé) |
| Synchronisé : store de travers | carte **Alignement des deux côtés**, store à l'arrêt |
| Synchronisé : un côté s'enroule de plus en plus | un moteur perd des pas : baisser la vitesse, vérifier la mécanique et l'alimentation, puis réaligner |
| Synchronisé : les deux côtés tournent en sens contraire | inverser le sens d'**un seul** moteur, puis recalibrer |
| « l'alignement ne sert qu'en mode synchronisé » | passer en synchronisé, ou régler chaque moteur séparément |
| « arrêtez le store avant d'aligner » | **Stop** puis recommencer l'alignement |
| « Mot de passe incorrect : mode inchangé » | saisir le mot de passe OTA (`SECRET_OTA_PASS`, défaut `store-ota`) |
| Moteur qui vibre sans tourner | baisser sa vitesse ; vérifier l'ordre IN1→IN4 et l'alimentation 5 V |
| Store qui peine à monter | baisser la vitesse (plus de couple) ; rampe plus douce (`RAMP_STEPS` plus grand) |
| `store.local` ne répond pas | utiliser l'adresse IP (mDNS non géré par certains Android) |
| La carte n'apparaît pas dans *Outils > Port* | redémarrer l'IDE ; ou passer par `/update` |
| Position décalée après blocage du store | recalibrer (moteur en boucle ouverte, sans capteur) |
| Page « Informations » : dernier démarrage = baisse de tension | alimentation 5 V plus puissante ou vitesse plus lente |
| Carte restée en point d'accès de secours | attendre : elle réessaie la box toutes les 2 min (seulement si aucun téléphone n'est connecté à son réseau) |
| Ancienne disposition après mise à jour | recharger la page |
| Moniteur série muet | *USB CDC On Boot > Enabled* |

La notice détaillée (PDF) reprend chacun de ces points pas à pas.

---

## Historique

- **v2** : sauvegarde de la position pendant le mouvement, OTA.
- **v3** : deuxième moteur sur 4 GPIO séparés, alignement des deux côtés.
- **v3.1** : OTA réparée (puissance Wi-Fi réduite, veille désactivée, série non bloquante), `/update` avec le seul mot de passe, affichage du signal Wi-Fi.
- **v4** : moteurs indépendants (position, butées, calibration, sauvegarde), commandes groupées « les deux », suppression de l'alignement.
- **v4.1** : page web en pleine largeur ; avec 2 moteurs, commandes côte à côte sur 2 colonnes ; avec 1 moteur, pleine largeur.
- **v4.2** : avec 2 moteurs, boutons des moteurs réduits aux symboles ▲ ■ ▼ (texte conservé avec 1 moteur) ; calibration sortie des cartes des moteurs, toujours en pleine largeur.
- **v5** : menu « Réglage Moteurs » (inversion du sens et curseur de vitesse par moteur, enregistrés en NVS) ; retour automatique du point d'accès de secours vers la box ; curseur « Aller à » sur la position actuelle.
- **v6** : rampe d'accélération et de décélération ; renommage des stores depuis la page ; page d'informations avec compteurs par moteur.
- **v6.1** : page d'informations : cycles complets affichés en nombre entier.
- **v6.2** : commandes de chaque store entourées d'un cadre « Moteur gauche » / « Moteur droit » (titres réglables par `MOT1_SIDE` / `MOT2_SIDE`).
- **v7** : choix **Indépendants / Synchronisés** sur la page (enregistré en NVS) ; en synchronisé, les deux moteurs font les mêmes demi-pas, une seule calibration, vitesse du moteur 1 ; retour de l'**alignement des deux côtés** ; mode affiché sur la page d'informations ; API `sync` et `jog`.
- **v7.1** : le choix **Indépendants / Synchronisés** demande le mot de passe OTA (`SECRET_OTA_PASS`), envoyé en POST.
- **v7.2** : le mot de passe du changement de mode se saisit dans une fenêtre (popup) au clic sur **Indépendants** ou **Synchronisés** ; le champ du menu et la confirmation séparée sont supprimés.
