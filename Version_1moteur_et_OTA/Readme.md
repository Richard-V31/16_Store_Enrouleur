# Store enrouleur — XIAO ESP32-C3 (commande par page web) — v2.3

Commande d'un store enrouleur motorisé par un moteur pas à pas, pilotée depuis n'importe quel navigateur (téléphone, tablette, ordinateur). Version **autonome, sans Blynk** : la carte héberge elle-même sa page web, et le programme peut être **mis à jour par le Wi-Fi (OTA)**.

Sketch : `StoreEnrouleur_Web_XIAO_C3_OTA.ino`

---

## Fonctionnalités

- **Page web intégrée** : Monter / Stop / Descendre, curseur « Aller à X % », barre de position, état en direct et **force du signal Wi-Fi** (rafraîchis toutes les 500 ms).
- **Accès par nom** : `http://store.local` (mDNS) ou par l'adresse IP.
- **Point d'accès de secours** : si la box est introuvable au démarrage, la carte crée son propre réseau Wi-Fi et reste accessible sur `http://192.168.5.1`.
- **Calibration en 3 clics** depuis la page web (position haute puis position basse).
- **Butées logicielles** haute (0 %) et basse (100 %).
- **Mémoire non volatile (NVS)** : la course et la position sont conservées après une coupure de courant. La position est enregistrée à chaque arrêt **et pendant le mouvement, tous les 500 pas (≈ 1,5 s)**.
- **Mise à jour OTA**, protégée par un mot de passe :
  - depuis l'**Arduino IDE** (port réseau « store ») ;
  - depuis un **navigateur** (`http://store.local/update`, envoi du fichier `.bin`).
- **Moteur piloté par temporisateur matériel** (`esp_timer`, toutes les 3 ms) : la rotation reste régulière même quand le Wi-Fi est occupé. Le temporisateur est suspendu pendant une mise à jour.
- **Bobines coupées à l'arrêt** : le moteur et le driver ne chauffent pas.
- **Deuxième moteur synchronisé** possible, sans modifier le programme (driver en parallèle).
- **API HTTP simple** (`/api/status`, `/api/cmd`) utilisable par une domotique ou un script.

---

## Matériel

| Élément | Remarque |
|---|---|
| Seeed Studio **XIAO ESP32-C3** | Carte de commande |
| Moteur pas à pas unipolaire 4 bobines (type **28BYJ-48**) | Piloté en demi-pas (séquence 8 phases) |
| Driver **ULN2003** | Carte d'interface du moteur |
| Alimentation 5 V | Dimensionnée pour la carte + le(s) moteur(s) |
| Mécanique d'entraînement du store | Adaptée au tube d'enroulement |

### Câblage

| XIAO ESP32-C3 | GPIO | Driver ULN2003 | Fil moteur |
|---|---|---|---|
| D2 | GPIO4 | IN1 | Bleu |
| D3 | GPIO5 | IN2 | Rose |
| D4 | GPIO6 | IN3 | Jaune |
| D5 | GPIO7 | IN4 | Orange |
| 5V | — | + (alimentation moteur) | Rouge (commun) |
| GND | — | − (masse commune obligatoire) | — |

> Pour un moteur qui consomme beaucoup (ou deux moteurs), alimentez le(s) driver(s) par une alimentation 5 V séparée et **reliez les masses** (GND) entre elles.

### Deuxième moteur synchronisé (option)

Brancher un 2e driver ULN2003 **en parallèle** sur les mêmes broches D2 à D5 : les deux moteurs reçoivent les mêmes impulsions. Aucune modification du programme.

| Montage du 2e moteur | Câblage du 2e driver |
|---|---|
| Même sens que le 1er | D2→IN1, D3→IN2, D4→IN3, D5→IN4 |
| Sens inverse (moteur monté en miroir) | D2→IN4, D3→IN3, D4→IN2, D5→IN1 |

> Deux 28BYJ-48 consomment ensemble environ 0,5 A : utilisez une alimentation 5 V séparée, masses reliées.

### Variante ESP32-C3 Mini

Les broches de l'ESP32-C3 Mini sont prêtes en commentaire dans le sketch (`IN1 4`, `IN2 5`, `IN3 6`, `IN4 7`) : commenter le bloc XIAO, décommenter le bloc Mini, choisir la carte correspondante.

---

## Installation

### 1. Environnement Arduino IDE

1. Installer le paquet de cartes **esp32** d'Espressif (Gestionnaire de cartes).
2. Sélectionner la carte **XIAO_ESP32C3**.
3. Pour voir le moniteur série : **Outils > USB CDC On Boot > Enabled**.
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
=== XIAO ESP32-C3 Store enrouleur (web) v2.3 ===
[NVS] Course = 0 pas, position = 0
[WIFI] Connexion......
[WIFI] CONNECTÉ, IP : 192.168.1.42, signal : -58 dBm
[WIFI] Page web : http://store.local
[OTA] Prêt (Arduino IDE)
[WEB] Serveur démarré
```

Le programme fonctionne aussi sans moniteur série ouvert : le port série USB est non bloquant.

---

## Première utilisation

1. Ouvrir `http://store.local` (ou l'IP affichée).
2. Au premier démarrage, l'état indique **« Arrêt — non calibré »** : seuls les boutons de calibration fonctionnent.
3. Faire la calibration :
   1. **Démarrer**.
   2. **Monter** jusqu'en haut, **Stop**, **Valider HAUT**.
   3. **Descendre** jusqu'en bas, **Stop**, **Valider BAS**.
4. Le store est prêt : Monter / Descendre / Aller à X % sont actifs.

0 % = store **en haut** (enroulé), 100 % = store **en bas** (déroulé).

La notice d'utilisation complète détaille chaque étape, les réglages et le dépannage.

---

## Mise à jour du programme (OTA)

Pendant une mise à jour, le moteur est arrêté, sa position est enregistrée et toutes les commandes sont refusées (la page affiche « Mise à jour en cours »). Une calibration en cours est abandonnée. En cas d'échec, la carte reprend son fonctionnement normal sur l'ancien programme.

### Depuis l'Arduino IDE

1. Ordinateur sur le même réseau que la carte.
2. **Outils > Port** → choisir le port réseau **store** (adresse IP de la carte).
3. **Téléverser**, puis saisir le mot de passe OTA quand l'IDE le demande.

### Depuis un navigateur

1. Dans l'Arduino IDE : **Croquis > Exporter les binaires compilés**.
2. Ouvrir `http://store.local/update` (lien « Mise à jour du programme » en bas de la page du store).
3. Saisir le mot de passe OTA, choisir le fichier **`StoreEnrouleur_Web_XIAO_C3_OTA.ino.bin`** (pas les fichiers `merged` ni `bootloader`), **Envoyer**.
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

La page affiche la force du signal reçu de la box (RSSI, en dBm) :

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
| `GET /api/cmd?a=stop` | Arrêt immédiat |
| `GET /api/cmd?a=goto&p=40` | Aller à 40 % |
| `GET /api/cmd?a=calstart` | Démarrer la calibration |
| `GET /api/cmd?a=caltop` | Valider la position haute |
| `GET /api/cmd?a=calbot` | Valider la position basse |
| `GET /update` | Page de mise à jour |
| `POST /update?pw=…` | Envoi d'un programme `.bin` (champ `firmware`) |

Réponses aux commandes : `200 OK` si la commande est acceptée, `409` avec un message d'explication si elle est refusée (y compris pendant une mise à jour).

Exemple de `/api/status` :

```json
{"pct":40,"step":1638,"travel":4096,"dir":0,"cal":false,"top":false,"ready":true,"ota":false,"rssi":-58}
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
| `STEP_PERIOD_US` | `3000` | Durée d'un demi-pas (µs) : plus petit = plus rapide |
| `SAVE_EVERY_STEPS` | `500` | Sauvegarde de la position pendant le mouvement (en pas) |
| `IN1`…`IN4` | `D2`…`D5` | Broches du driver |

- **Sens inversé** (« Monter » fait descendre) : intervertir les broches dans le code (`IN1 D5`, `IN2 D4`, `IN3 D3`, `IN4 D2`), puis refaire la calibration.
- **Puissance Wi-Fi** : réduite à `WIFI_POWER_8_5dBm` dans `startWiFi()` (mode box et mode secours). À pleine puissance, l'émetteur du XIAO C3 sature et l'OTA reste bloquée à 0 %. Si la portée est insuffisante, essayer `WIFI_POWER_11dBm` ou `WIFI_POWER_13dBm`.
- **Économie d'énergie Wi-Fi** désactivée (`WiFi.setSleep(false)`) : page web et OTA plus rapides.

---

## Données mémorisées (NVS, espace `roller`)

| Clé | Contenu | Écrite |
|---|---|---|
| `travelSteps` | Course totale en pas | À la fin d'une calibration |
| `position` | Dernière position en pas | À chaque arrêt, tous les 500 pas pendant le mouvement, et avant une mise à jour (hors calibration) |

Une mise à jour OTA ne modifie pas la NVS : calibration et position sont conservées.

---

## Limites connues

- **Pas de capteur de fin de course** : la position est comptée en pas. Après une coupure de courant pendant un mouvement, l'erreur est d'au plus ~500 pas ; un store bougé à la main ou un moteur qui « saute des pas » décale aussi la position → refaire la calibration si l'écart est gênant.
- **Pas de couple de maintien** à l'arrêt (bobines coupées) : la mécanique doit retenir seule le store.
- **Pas d'authentification** sur la page et l'API : toute personne connectée au même réseau peut commander le store. Seule la mise à jour est protégée par mot de passe (transmis en clair, HTTP non chiffré).
- **Usure de la flash** : la sauvegarde pendant le mouvement multiplie les écritures NVS, ce qui reste négligeable pour un usage domestique.

---

## Historique

- Version initiale : ESP32-S3 + application Blynk.
- v1 : XIAO ESP32-C3, page web autonome, point d'accès de secours, moteur sur temporisateur matériel.
- v2 : sauvegarde de la position pendant le mouvement, mise à jour OTA (Arduino IDE et navigateur).
- v2.1 : OTA accélérée (économie d'énergie Wi-Fi désactivée, temporisateur du moteur suspendu pendant la mise à jour).
- v2.2 : port série USB non bloquant (cause de la lenteur de l'OTA), page `/update` protégée par le seul mot de passe OTA.
- v2.3 : puissance d'émission réduite (OTA bloquée à 0 % sur XIAO C3), affichage de la force du signal Wi-Fi.
