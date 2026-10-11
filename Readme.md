# Store enrouleur — XIAO ESP32-C3 (commande par page web)

Commande d'un store enrouleur motorisé par un moteur pas à pas, pilotée depuis n'importe quel navigateur (téléphone, tablette, ordinateur). Version **autonome, sans Blynk** : la carte héberge elle-même sa page web.

Sketch : `StoreEnrouleur_Web_XIAO_C3.ino`

---

## Fonctionnalités

- **Page web intégrée** : Monter / Stop / Descendre, curseur « Aller à X % », barre de position, état en direct (rafraîchi toutes les 500 ms).
- **Accès par nom** : `http://store.local` (mDNS) ou par l'adresse IP.
- **Point d'accès de secours** : si la box est introuvable au démarrage, la carte crée son propre réseau Wi-Fi et reste accessible sur `http://192.168.5.1`.
- **Calibration en 3 clics** depuis la page web (position haute puis position basse).
- **Butées logicielles** haute (0 %) et basse (100 %).
- **Mémoire non volatile (NVS)** : la course et la dernière position sont conservées après une coupure de courant.
- **Moteur piloté par temporisateur matériel** (`esp_timer`, toutes les 3 ms) : la rotation reste régulière même quand le Wi-Fi est occupé.
- **Bobines coupées à l'arrêt** : le moteur et le driver ne chauffent pas.
- **API HTTP simple** (`/api/status`, `/api/cmd`) utilisable par une domotique ou un script.

---

## Matériel

| Élément | Remarque |
|---|---|
| Seeed Studio **XIAO ESP32-C3** | Carte de commande |
| Moteur pas à pas unipolaire 4 bobines (type **28BYJ-48**) | Piloté en demi-pas (séquence 8 phases) |
| Driver **ULN2003** | Carte d'interface du moteur |
| Alimentation 5 V | Dimensionnée pour la carte + le moteur |
| Mécanique d'entraînement du store | Adaptée au tube d'enroulement |

### Câblage

| XIAO ESP32-C3 | GPIO | Driver ULN2003 |
|---|---|---|
| D2 | GPIO4 | IN1 |
| D3 | GPIO5 | IN2 |
| D4 | GPIO6 | IN3 |
| D5 | GPIO7 | IN4 |
| 5V | — | + (alimentation moteur) |
| GND | — | − (masse commune obligatoire) |

> Pour un moteur qui consomme beaucoup, alimentez le driver par une alimentation 5 V séparée et **reliez les masses** (GND) entre elles.

---

## Installation

### 1. Environnement Arduino IDE

1. Installer le paquet de cartes **esp32** d'Espressif (Gestionnaire de cartes).
2. Sélectionner la carte **XIAO_ESP32C3**.
3. Pour voir le moniteur série : **Outils > USB CDC On Boot > Enabled**.

Aucune bibliothèque externe à installer : `WiFi`, `WebServer`, `ESPmDNS`, `Preferences` et `esp_timer` font partie du paquet ESP32.

### 2. Fichier `arduino_secrets.h`

Créer ce fichier dans le même dossier que le sketch :

```cpp
#define SECRET_SSID     "NomDeVotreBox"
#define SECRET_PASS     "MotDePasseDeVotreBox"
#define SECRET_AP_SSID  "Store-ESP32"   // Réseau de secours créé par la carte
#define SECRET_AP_PASS  "store1234"     // 8 caractères minimum
```

> Ne publiez pas ce fichier (ajoutez-le à votre `.gitignore`).

### 3. Téléversement

Brancher la carte en USB, téléverser, puis ouvrir le moniteur série à **115200 bauds**. L'adresse IP s'affiche :

```
=== XIAO ESP32-C3 Store enrouleur (web) ===
[NVS] Course = 0 pas, position = 0
[WIFI] Connexion......
[WIFI] CONNECTÉ, IP : 192.168.1.42
[WIFI] Page web : http://store.local
[WEB] Serveur démarré
```

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

## Accès réseau

| Situation | Adresse |
|---|---|
| Carte connectée à la box | `http://store.local` ou l'IP affichée sur le port série |
| Box introuvable (15 s) → point d'accès de secours | Se connecter au réseau `SECRET_AP_SSID` puis `http://192.168.5.1` |

Le point d'accès de secours n'est choisi **qu'au démarrage** : une fois la box revenue, redémarrez la carte pour qu'elle s'y reconnecte.

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

Réponses : `200 OK` si la commande est acceptée, `409` avec un message d'explication si elle est refusée.

Exemple de `/api/status` :

```json
{"pct":40,"step":1638,"travel":4096,"dir":0,"cal":false,"top":false,"ready":true}
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

Exemple en ligne de commande :

```bash
curl "http://store.local/api/cmd?a=goto&p=50"
```

---

## Réglages dans le code

| Constante | Valeur | Rôle |
|---|---|---|
| `HOSTNAME` | `"store"` | Nom réseau → `http://store.local` |
| `AP_IP` | `192.168.5.1` | Adresse de la carte en point d'accès |
| `STEP_PERIOD_US` | `3000` | Durée d'un demi-pas (µs) : plus petit = plus rapide |
| `IN1`…`IN4` | `D2`…`D5` | Broches du driver |

- **Sens inversé** (« Monter » fait descendre) : intervertir les broches dans le code (`IN1 D5`, `IN2 D4`, `IN3 D3`, `IN4 D2`), puis refaire la calibration.
- **Wi-Fi instable** sur certains C3 : décommenter `WiFi.setTxPower(WIFI_POWER_8_5dBm);` dans `startWiFi()`.

---

## Données mémorisées (NVS, espace `roller`)

| Clé | Contenu | Écrite |
|---|---|---|
| `travelSteps` | Course totale en pas | À la fin d'une calibration |
| `position` | Dernière position en pas | À chaque arrêt du moteur (hors calibration) |

---

## Limites connues

- **Pas de capteur de fin de course** : la position est comptée en pas. Une coupure de courant pendant un mouvement, un store bougé à la main ou un moteur qui « saute des pas » décale la position → refaire la calibration.
- **Pas de couple de maintien** à l'arrêt (bobines coupées) : la mécanique doit retenir seule le store.
- **Pas d'authentification** : toute personne connectée au même réseau peut commander le store.

---

## Historique

- Version initiale : ESP32-S3 + application Blynk.
- Version actuelle : XIAO ESP32-C3, page web autonome, point d'accès de secours, moteur sur temporisateur matériel.
