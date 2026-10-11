# Store enrouleur — XIAO ESP32-C3 — deux moteurs synchronisés — v3.1

Commande d'un store enrouleur entraîné par **deux moteurs pas à pas synchronisés** (un de chaque côté du tube), pilotée depuis n'importe quel navigateur (téléphone, tablette, ordinateur). Version **autonome, sans Blynk** : la carte héberge elle-même sa page web, et le programme peut être **mis à jour par le Wi-Fi (OTA)**.

Sketch : `StoreEnrouleur_Web_XIAO_C3_2moteursSynchro.ino`

---

## Fonctionnalités

- **Deux moteurs sur des broches séparées**, pilotés par le même temporisateur : ils font chacun leur demi-pas au même instant.
- **Alignement des deux côtés** depuis la page web : un seul moteur tourne de 10, 100 ou 500 pas pour remettre la barre de charge horizontale, sans modifier la position du store.
- **Sens de chaque moteur réglable dans le programme** (`MOT1_INVERT`, `MOT2_INVERT`) : pas de fils à croiser pour un moteur monté en miroir.
- **Mode un seul moteur** possible (`MOTOR2_ENABLED = false`).
- **Page web intégrée** : Monter / Stop / Descendre, curseur « Aller à X % », barre de position, état en direct et **force du signal Wi-Fi** (rafraîchis toutes les 500 ms).
- **Accès par nom** : `http://store.local` (mDNS) ou par l'adresse IP.
- **Point d'accès de secours** : si la box est introuvable au démarrage, la carte crée son propre réseau Wi-Fi et reste accessible sur `http://192.168.5.1`.
- **Calibration en 3 clics** depuis la page web (position haute, alignement si besoin, puis position basse).
- **Butées logicielles** haute (0 %) et basse (100 %).
- **Mémoire non volatile (NVS)** : la course et la position sont conservées après une coupure de courant. La position est enregistrée à chaque arrêt **et pendant le mouvement, tous les 500 pas (≈ 1,5 s)**.
- **Mise à jour OTA**, protégée par un mot de passe :
  - depuis l'**Arduino IDE** (port réseau « store ») ;
  - depuis un **navigateur** (`http://store.local/update`, envoi du fichier `.bin`).
- **Moteurs pilotés par temporisateur matériel** (`esp_timer`, toutes les 3 ms) : rotation régulière même quand le Wi-Fi est occupé. Le temporisateur est suspendu pendant une mise à jour.
- **Bobines coupées à l'arrêt** : moteurs et drivers ne chauffent pas.
- **API HTTP simple** (`/api/status`, `/api/cmd`) utilisable par une domotique ou un script.

---

## Matériel

| Élément | Remarque |
|---|---|
| Seeed Studio **XIAO ESP32-C3** | Carte de commande |
| 2 moteurs pas à pas unipolaires 4 bobines (type **28BYJ-48**) | Pilotés en demi-pas (séquence 8 phases) |
| 2 drivers **ULN2003** | Un par moteur |
| **Alimentation 5 V externe, 1 A minimum** | Pour les deux drivers (les deux moteurs consomment ensemble ≈ 0,5 A) |
| Mécanique d'entraînement du store | Un moteur à chaque extrémité du tube |

### Câblage

| Moteur 1 — XIAO | GPIO | Driver n°1 | Fil moteur |
|---|---|---|---|
| D2 | GPIO4 | IN1 | Bleu |
| D3 | GPIO5 | IN2 | Rose |
| D4 | GPIO6 | IN3 | Jaune |
| D5 | GPIO7 | IN4 | Orange |

| Moteur 2 — XIAO | GPIO | Driver n°2 | Fil moteur |
|---|---|---|---|
| D1 | GPIO3 | IN1 | Bleu |
| D6 | GPIO21 | IN2 | Rose |
| D7 | GPIO20 | IN3 | Jaune |
| D10 | GPIO10 | IN4 | Orange |

| Alimentation | Raccordement |
|---|---|
| + 5 V externe | + des deux drivers (fil rouge commun des moteurs) |
| GND externe | − des deux drivers **et** GND du XIAO (masse commune obligatoire) |
| XIAO | Alimenté par l'USB, ou par sa broche 5V depuis la même alimentation externe |

> **D6 et D7** sont normalement le port série matériel (TX/RX) : ici ils servent au moteur 2, le moniteur série passe donc par l'USB (**USB CDC On Boot = Enabled**, obligatoire).
> Broches restées libres : **D0, D8, D9**. Ce sont des broches de démarrage de l'ESP32-C3 : à réserver à des boutons avec résistance de rappel au +3,3 V, jamais à des sorties forcées au niveau bas au démarrage.

---

## Installation

### 1. Environnement Arduino IDE

1. Installer le paquet de cartes **esp32** d'Espressif (Gestionnaire de cartes).
2. Sélectionner la carte **XIAO_ESP32C3**.
3. **Outils > USB CDC On Boot > Enabled** (obligatoire, voir ci-dessus).
4. Pour l'OTA : **Outils > Partition Scheme** → un schéma avec OTA (celui par défaut convient).

Aucune bibliothèque externe à installer : `WiFi`, `WebServer`, `ESPmDNS`, `Preferences`, `ArduinoOTA`, `Update` et `esp_timer` font partie du paquet ESP32.

### 2. Fichier `arduino_secrets.h`

Créer ce fichier dans le même dossier que le sketch :

```cpp
#define SECRET_SSID      "NomDeVotreBox"
#define SECRET_PASS      "MotDePasseDeVotreBox"
#define SECRET_AP_SSID   "Store-ESP32"   // Réseau de secours créé par la carte
#define SECRET_AP_PASS   "store1234"     // 8 caractères minimum
#define SECRET_OTA_PASS  "MonMotDePasseOTA"   // Mot de passe des mises à jour
```

> Si `SECRET_OTA_PASS` est absent, le mot de passe OTA par défaut est `store-ota` : **à changer**.
> Ne publiez pas ce fichier (ajoutez-le à votre `.gitignore`).

### 3. Premier téléversement (USB)

Le premier téléversement se fait obligatoirement par USB. Ouvrir ensuite le moniteur série à **115200 bauds** :

```
=== XIAO ESP32-C3 Store enrouleur (web) v3.1 - 2 moteurs ===
[NVS] Course = 0 pas, position = 0
[WIFI] Connexion......
[WIFI] CONNECTÉ, IP : 192.168.1.42, signal : -58 dBm
[WIFI] Page web : http://store.local
[OTA] Prêt (Arduino IDE)
[WEB] Serveur démarré
```

Le programme fonctionne aussi sans moniteur série ouvert : le port série USB est non bloquant.

### 4. Sens de rotation (avant d'accoupler le store)

Moteurs posés sur la table, appuyer sur **Monter** pendant la calibration : les deux axes doivent tourner dans le sens qui **enroule** le store de leur côté.

- Un seul moteur tourne à l'envers (cas habituel d'un moteur monté en miroir) : mettre `MOT2_INVERT` (ou `MOT1_INVERT`) à `true`.
- Les deux tournent à l'envers : inverser les deux réglages.

Téléverser, puis refaire la calibration. Aucun fil à modifier.

---

## Première utilisation

1. Ouvrir `http://store.local` (ou l'IP affichée).
2. Au premier démarrage, l'état indique **« Arrêt — non calibré »** : seuls la calibration et l'alignement fonctionnent.
3. Faire la calibration :
   1. **Démarrer**.
   2. **Monter** jusqu'en haut, **Stop**.
   3. Si la barre de charge n'est pas horizontale, **aligner** le côté en retard (voir ci-dessous).
   4. **Valider HAUT**.
   5. **Descendre** jusqu'en bas, **Stop**, **Valider BAS**.
4. Le store est prêt : Monter / Descendre / Aller à X % sont actifs.

0 % = store **en haut** (enroulé), 100 % = store **en bas** (déroulé).

La notice d'utilisation complète détaille chaque étape, les réglages et le dépannage.

---

## Alignement des deux côtés

Si un moteur a perdu des pas (point dur, store bougé à la main), la barre de charge n'est plus horizontale. La zone **« Alignement des deux côtés »** fait tourner **un seul moteur** :

1. Store **à l'arrêt**.
2. Choisir le nombre de pas : **10** (réglage fin), **100** (par défaut) ou **500**.
3. Appuyer sur **▲ Côté 1 / ▼ Côté 1** ou **▲ Côté 2 / ▼ Côté 2** autant de fois que nécessaire.

- Pendant l'alignement, l'état affiche « Alignement du côté 1 » (ou 2) ; **Stop** l'interrompt.
- L'alignement ne change **pas** la position du store (pourcentage) : il rattrape uniquement le décalage entre les deux côtés.
- Il fonctionne aussi pendant une calibration et avant toute calibration.
- **Aucune butée** pendant l'alignement : chaque commande est limitée à 2000 pas par sécurité (`TRIM_MAX_STEPS`).
- Côté 1 = moteur 1 (D2 à D5), côté 2 = moteur 2 (D1, D6, D7, D10). Repérez quel côté du store correspond à chaque moteur.

---

## Mise à jour du programme (OTA)

Pendant une mise à jour, les moteurs sont arrêtés, la position est enregistrée et toutes les commandes sont refusées (la page affiche « Mise à jour en cours »). Une calibration ou un alignement en cours est abandonné. En cas d'échec, la carte reprend son fonctionnement normal sur l'ancien programme.

### Depuis l'Arduino IDE

1. Ordinateur sur le même réseau que la carte.
2. **Outils > Port** → choisir le port réseau **store** (adresse IP de la carte).
3. **Téléverser**, puis saisir le mot de passe OTA quand l'IDE le demande.

### Depuis un navigateur

1. Dans l'Arduino IDE : **Croquis > Exporter les binaires compilés**.
2. Ouvrir `http://store.local/update` (lien « Mise à jour du programme » en bas de la page du store).
3. Saisir le mot de passe OTA, choisir le fichier **`StoreEnrouleur_Web_XIAO_C3_2moteursSynchro.ino.bin`** (pas les fichiers `merged` ni `bootloader`), **Envoyer**.
4. La carte redémarre ; la page revient automatiquement sur le store après 10 s.

| Réponse de `/update` | Signification |
|---|---|
| `200 OK` | Mise à jour réussie, redémarrage |
| `401 Mot de passe incorrect` | Mot de passe OTA erroné, rien n'est écrit |
| `500 Échec : fichier invalide` | Fichier non reconnu ou incomplet |

> Si l'OTA reste bloquée à 0 % : signal trop faible ou émetteur saturé (voir « Puissance Wi-Fi » ci-dessous). Rapprocher la carte de la box, ou repasser par l'USB.

---

## Accès réseau

| Situation | Adresse |
|---|---|
| Carte connectée à la box | `http://store.local` ou l'IP affichée sur le port série |
| Box introuvable (15 s) → point d'accès de secours | Se connecter au réseau `SECRET_AP_SSID` puis `http://192.168.5.1` |
| Mise à jour par navigateur | `http://store.local/update` (ou `http://192.168.5.1/update` en secours) |

Le point d'accès de secours n'est choisi **qu'au démarrage** : une fois la box revenue, redémarrez la carte pour qu'elle s'y reconnecte.

### Signal Wi-Fi

| RSSI | Qualité |
|---|---|
| au-dessus de −67 dBm | Bon |
| de −67 à −75 dBm | Moyen |
| en dessous de −75 dBm | Faible (lenteurs, OTA instable) |

En mode point d'accès de secours, la valeur affichée est « -- ».

---

## API HTTP

| Requête | Effet |
|---|---|
| `GET /` | Page web |
| `GET /api/status` | État au format JSON |
| `GET /api/cmd?a=up` | Monter (jusqu'à Stop ou la butée haute) |
| `GET /api/cmd?a=down` | Descendre (jusqu'à Stop ou la butée basse) |
| `GET /api/cmd?a=stop` | Arrêt immédiat (mouvement et alignement) |
| `GET /api/cmd?a=goto&p=40` | Aller à 40 % |
| `GET /api/cmd?a=trim&m=2&n=-100` | Alignement : moteur 2 seul, 100 pas vers le haut (`n` > 0 = vers le bas, limité à ±2000) |
| `GET /api/cmd?a=calstart` | Démarrer la calibration |
| `GET /api/cmd?a=caltop` | Valider la position haute |
| `GET /api/cmd?a=calbot` | Valider la position basse |
| `GET /update` | Page de mise à jour |
| `POST /update?pw=…` | Envoi d'un programme `.bin` (champ `firmware`) |

Réponses aux commandes : `200 OK` si la commande est acceptée, `409` avec un message d'explication si elle est refusée (y compris pendant une mise à jour ou un alignement).

Exemple de `/api/status` :

```json
{"pct":40,"step":1638,"travel":4096,"dir":0,"cal":false,"top":false,"ready":true,"ota":false,"trim":0,"rssi":-58}
```

| Champ | Signification |
|---|---|
| `pct` | Position en % (0 = haut, 100 = bas) |
| `step` | Position en pas |
| `travel` | Course totale calibrée en pas (0 = non calibré) |
| `dir` | `1` descente, `-1` montée, `0` arrêt |
| `cal` | Calibration en cours |
| `top` | Position haute validée pendant la calibration |
| `ready` | Store calibré |
| `ota` | Mise à jour du programme en cours |
| `trim` | Moteur en cours d'alignement (`0` aucun, `1` ou `2`) |
| `rssi` | Force du signal de la box en dBm (`0` en mode point d'accès) |

Exemple en ligne de commande :

```bash
curl "http://store.local/api/cmd?a=goto&p=50"
```

---

## Réglages dans le code

| Constante | Valeur | Rôle |
|---|---|---|
| `HOSTNAME` | `"store"` | Nom réseau → `http://store.local` et port OTA « store » |
| `AP_IP` | `192.168.5.1` | Adresse de la carte en point d'accès |
| `OTA_PORT` | `3232` | Port réseau de l'OTA Arduino |
| `M1_PINS` | `{ D2, D3, D4, D5 }` | Broches du driver n°1 (IN1 à IN4) |
| `M2_PINS` | `{ D1, D6, D7, D10 }` | Broches du driver n°2 (IN1 à IN4) |
| `MOTOR2_ENABLED` | `true` | `false` = fonctionnement avec un seul moteur |
| `MOT1_INVERT` | `false` | `true` = inverse le sens du moteur 1 |
| `MOT2_INVERT` | `false` | `true` = inverse le sens du moteur 2 (moteur monté en miroir) |
| `STEP_PERIOD_US` | `3000` | Durée d'un demi-pas (µs) : plus petit = plus rapide |
| `SAVE_EVERY_STEPS` | `500` | Sauvegarde de la position pendant le mouvement (en pas) |
| `TRIM_MAX_STEPS` | `2000` | Nombre de pas maximum par commande d'alignement |

- **Puissance Wi-Fi** : réduite à `WIFI_POWER_8_5dBm` dans `startWiFi()` (mode box et mode secours). À pleine puissance, l'émetteur du XIAO C3 sature et l'OTA reste bloquée à 0 %. Si la portée est insuffisante, essayer `WIFI_POWER_11dBm` ou `WIFI_POWER_13dBm`.
- **Veille Wi-Fi** désactivée (`WiFi.setSleep(false)`) : page web et OTA plus rapides.

---

## Données mémorisées (NVS, espace `roller`)

| Clé | Contenu | Écrite |
|---|---|---|
| `travelSteps` | Course totale en pas | À la fin d'une calibration |
| `position` | Dernière position en pas | À chaque arrêt, tous les 500 pas pendant le mouvement, et avant une mise à jour (hors calibration) |

Une mise à jour OTA ne modifie pas la NVS : calibration et position sont conservées. L'espace `roller` est le même que dans la version un moteur : passer d'une version à l'autre conserve la calibration (à refaire si la mécanique change).

---

## Limites connues

- **Pas de capteur de fin de course** : la position est comptée en pas. Après une coupure de courant pendant un mouvement, l'erreur est d'au plus ~500 pas ; un store bougé à la main ou un moteur qui « saute des pas » décale aussi la position → refaire la calibration si l'écart est gênant.
- **Pas de détection du décalage entre les deux côtés** : si un moteur perd des pas, la carte ne s'en aperçoit pas → corriger avec l'alignement.
- **Pas de butée pendant l'alignement** (limité à 2000 pas par commande).
- **Pas de couple de maintien** à l'arrêt (bobines coupées) : la mécanique doit retenir seule le store.
- **Pas d'authentification** sur la page et l'API : toute personne connectée au même réseau peut commander le store. Seule la mise à jour est protégée par mot de passe (transmis en clair, HTTP non chiffré).

---

## Historique

- Version initiale : ESP32-S3 + application Blynk.
- v1 : XIAO ESP32-C3, page web autonome, point d'accès de secours, moteur sur temporisateur matériel.
- v2 : sauvegarde de la position pendant le mouvement, mise à jour OTA (Arduino IDE et navigateur).
- v3 : deuxième moteur sur 4 GPIO séparés, sens de chaque moteur réglable, alignement des deux côtés depuis la page web.
- v3.1 : OTA réparée (puissance Wi-Fi réduite, veille Wi-Fi désactivée, port série non bloquant), page `/update` avec le seul mot de passe OTA, affichage de la force du signal Wi-Fi.
