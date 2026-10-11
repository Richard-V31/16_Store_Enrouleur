# Stores enrouleurs — XIAO ESP32-C3 — 2 moteurs indépendants (v4)

Commande de **deux stores enrouleurs** par moteurs pas à pas (driver ULN2803) depuis une **page web** servie directement par un **Seeed Studio XIAO ESP32-C3**. Aucun service externe (pas de Blynk, pas de cloud) : la carte se pilote depuis n'importe quel navigateur du réseau local.

Fichier du croquis : `StoreEnrouleur_Web_XIAO_C3_2moteursIndependants.ino`

---

## Fonctions

- **Deux moteurs totalement indépendants** : position, sens, butées, calibration et sauvegarde propres à chacun.
- **Commande groupée « Les deux moteurs »** : Monter / Stop / Descendre / Aller à X % pour les deux stores en un clic.
- **Aller à une position** en pourcentage (0 % = haut, 100 % = bas).
- **Butées logicielles** haute et basse définies par une calibration en 3 étapes, moteur par moteur.
- **Sauvegarde de la position en mémoire non volatile (NVS)** à chaque arrêt et tous les 500 pas pendant le mouvement (protection contre les coupures de courant).
- **Moteurs pilotés par un temporisateur matériel** (`esp_timer`, 3 ms par demi-pas) : la rotation reste régulière même quand le Wi-Fi est occupé.
- **Bobines coupées à l'arrêt** : les moteurs ne chauffent pas.
- **Mise à jour à distance (OTA)** depuis l'Arduino IDE (port réseau) ou depuis le navigateur (`/update`), protégée par **un seul mot de passe**.
- **Point d'accès de secours** (192.168.5.1) si la box est introuvable.
- **Accès par nom** : `http://store.local` (mDNS).
- **Indicateur de force du signal Wi-Fi** sur la page.
- **Reprise automatique** de la calibration de la v3.1 au premier démarrage.

---

## Matériel

| Élément | Quantité | Remarque |
|---|---|---|
| Seeed Studio XIAO ESP32-C3 | 1 | avec son antenne externe branchée |
| Moteur pas à pas 5 V unipolaire (type 28BYJ-48) | 1 ou 2 | |
| Driver ULN2803 SOP18 | 1  | un pour les 2 moteurs |
| Alimentation 5 V | 1 | **1 A minimum** pour deux moteurs |
| Fils de liaison | — | masse commune obligatoire |

---

## Câblage

| Driver | IN1 | IN2 | IN3 | IN4 |
|---|---|---|---|---|
| Moteur 1 (ULN2803 n°1a) | D2 (GPIO4) | D3 (GPIO5) | D4 (GPIO6) | D5 (GPIO7) |
| Moteur 2 (ULN2803 n°1b) | D1 (GPIO3) | D6 (GPIO21) | D7 (GPIO20) | D10 (GPIO10) |

- Alimentation 5 V externe sur les **+ / −** des deux drivers.
- **GND de l'alimentation relié au GND du XIAO** (masse commune).
- Ne pas alimenter les moteurs par la broche 5 V du XIAO.
- Broches restées libres : **D0, D8, D9** (broches de démarrage : à réserver à des boutons avec résistance de rappel au +3,3 V).
- D6/D7 étant le port série matériel, le moniteur série passe par l'**USB intégré** (voir réglages ci-dessous).

---

## Installation

### 1. Arduino IDE

1. Installer le paquet **esp32 d'Espressif** (Gestionnaire de cartes).
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

| Constante | Défaut | Rôle |
|---|---|---|
| `MOTOR2_ENABLED` | `true` | `false` = fonctionnement avec un seul moteur |
| `MOT1_INVERT` / `MOT2_INVERT` | `false` | inverse le sens d'un moteur sans recâbler |
| `MOT1_NAME` / `MOT2_NAME` | `"Moteur 1"` / `"Moteur 2"` | noms affichés sur la page (ex. « Store gauche ») |
| `STEP_PERIOD_US` | `3000` | durée d'un demi-pas en µs (vitesse) |
| `SAVE_EVERY_STEPS` | `500` | sauvegarde intermédiaire pendant le mouvement |
| `HOSTNAME` | `"store"` | nom réseau → `http://store.local` |
| `AP_IP` | `192.168.5.1` | adresse en mode point d'accès |

### 4. Premier téléversement

Téléverser par USB, ouvrir le moniteur série à **115200 bauds** et relever l'adresse IP affichée.

---

## Utilisation

1. Ouvrir `http://store.local` (ou l'adresse IP).
2. **Calibrer chaque moteur** (section « Calibration » de sa carte) :
   1. **Démarrer** (les butées sont désactivées) ;
   2. **Monter** jusqu'en haut, **Stop**, **Valider HAUT** ;
   3. **Descendre** jusqu'en bas, **Stop**, **Valider BAS**.
3. Utiliser ensuite Monter / Stop / Descendre / Aller à X %, par moteur ou pour les deux à la fois.

La calibration est conservée après une coupure de courant. Elle se fait toujours **moteur par moteur**.

---

## Mise à jour du programme (OTA)

- **Arduino IDE** : la carte apparaît dans *Outils > Port* sous le nom `store` (port réseau 3232). Téléverser normalement, saisir le mot de passe OTA.
- **Navigateur** : *Croquis > Exporter les binaires compilés*, puis ouvrir `http://store.local/update`, saisir le mot de passe OTA et envoyer le fichier **`.ino.bin`** (pas les fichiers `merged` ni `bootloader`).

Pendant une mise à jour, les moteurs sont arrêtés, les positions sauvegardées et toute commande est refusée.

---

## API HTTP

| Adresse | Rôle |
|---|---|
| `GET /` | page de commande |
| `GET /api/status` | état en JSON |
| `GET /api/cmd?a=<action>&m=<moteur>[&p=<0-100>]` | commande |
| `GET /update` / `POST /update?pw=<mdp>` | mise à jour par le navigateur |

- `m` : `1` ou `2` = un moteur, `0` = les deux.
- `a` : `up`, `down`, `stop`, `goto` (avec `p`), `calstart`, `caltop`, `calbot` (calibration : `m` = 1 ou 2 obligatoire).

Exemple : `http://store.local/api/cmd?a=goto&m=0&p=30` → les deux stores à 30 %.

Réponse de `/api/status` :

```json
{"ota":false,"rssi":-58,"m":[
 {"name":"Moteur 1","pct":42,"step":1700,"travel":4050,"dir":0,"cal":false,"top":false,"ready":true},
 {"name":"Moteur 2","pct":0,"step":0,"travel":3980,"dir":-1,"cal":false,"top":false,"ready":true}]}
```

---

## Mise à niveau depuis la v3.1

Au premier démarrage, la course (`travelSteps`) et la position (`position`) communes de la v3.1 sont recopiées pour chacun des deux moteurs (nouvelles clés `travel1`/`pos1`, `travel2`/`pos2`). Aucune recalibration n'est nécessaire ; on peut ensuite recalibrer chaque moteur séparément si besoin.

---

## Dépannage rapide

| Symptôme | Solution |
|---|---|
| Un moteur tourne à l'envers | `MOTx_INVERT = true` puis recalibrer ce moteur |
| « non calibré » | lancer la calibration du moteur concerné |
| Moteur qui vibre sans tourner | vérifier l'ordre IN1→IN4 et l'alimentation 5 V |
| `store.local` ne répond pas | utiliser l'adresse IP (mDNS non géré par certains Android) |
| La carte n'apparaît pas dans *Outils > Port* | redémarrer l'IDE ; ou passer par `/update` |
| Position décalée après blocage du store | recalibrer (moteur en boucle ouverte, sans capteur) |
| Moniteur série muet | *USB CDC On Boot > Enabled* |

La notice détaillée (PDF) reprend chacun de ces points pas à pas.

---

## Historique

- **v2** : sauvegarde de la position pendant le mouvement, OTA.
- **v3** : deuxième moteur sur 4 GPIO séparés, alignement des deux côtés.
- **v3.1** : OTA réparée (puissance Wi-Fi réduite, veille désactivée, série non bloquante), `/update` avec le seul mot de passe, affichage du signal Wi-Fi.
- **v4** : moteurs indépendants (position, butées, calibration, sauvegarde), commandes groupées « les deux », suppression de l'alignement.
