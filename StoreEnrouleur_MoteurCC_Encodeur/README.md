# Stores enrouleurs — XIAO ESP32-C3 · moteurs CC à encodeur (v5.2)

Commande de **un ou deux stores enrouleurs indépendants** depuis une **page web** hébergée par la carte, sans application ni service en ligne (pas de Blynk).

Chaque store est entraîné par un **motoréducteur à courant continu 6 V avec encodeur magnétique** (type GM12 / N20, ex. GBMQ-GM12BY20) piloté par un **DRV8833**. La position est mesurée par l'encodeur, ce qui permet d'aller à un pourcentage précis et de détecter un blocage.

---

## Fonctionnalités

- Page web responsive (téléphone, tablette, PC) : `http://store.local` ou adresse IP
- Deux stores **indépendants** + commande groupée « les deux »
- Monter / Stop / Descendre, et **Aller à X %** (0 % = haut, 100 % = bas)
- **Calibration** des butées haute et basse depuis la page, moteur par moteur
- Position mesurée par **encodeur en quadrature ×4** (interruptions)
- **PWM 20 kHz** (inaudible), rampe d'accélération, **ralentissement** avant l'arrêt
- **Freinage actif** à l'arrêt
- Sécurités : **détection de blocage** (encodeur immobile 400 ms), **sens moteur/encodeur incohérent** et **défaut du DRV8833** (surintensité / surchauffe, lu sur nFAULT)
- Inversion de sens rapprochée gérée (pause freinée de 200 ms)
- **Position sauvegardée** en mémoire non volatile (pendant le mouvement et à l'arrêt) : elle est retrouvée après une coupure de courant
- **Mise à jour OTA** : depuis l'Arduino IDE (port réseau) ou par le navigateur (`/update`), protégée par mot de passe
- **Point d'accès de secours** (`192.168.5.1`) si la box est introuvable, avec **retour automatique sur la box** (nouvelle tentative toutes les 5 min)
- **Plusieurs cartes** possibles sur le même réseau (voir « Plusieurs ESP32 »)
- **Icône** de la page dans l'onglet du navigateur et sur l'écran d'accueil du téléphone (`favicon.h`)
- Indication de la **force du signal Wi-Fi**
- **API HTTP** simple pour une domotique (`/api/status`, `/api/cmd`)

---

## Matériel

| Élément | Référence / valeur |
|---|---|
| Microcontrôleur | Seeed Studio XIAO ESP32-C3 |
| Pont en H double | DRV8833 (boîtier DRV8833PWP ou module) |
| Moteurs | 2 × motoréducteur CC 6 V avec encodeur magnétique (GM12 / N20) |
| Alimentation | 5 V externe, **1 A minimum** |
| Condensateurs | 10 µF céramique + 100 µF électrolytique sur VM ; 2,2 µF sur VINT ; 10 nF entre VCP et VM |
| Résistances | 2 × 10 kΩ (rappel de nSLEEP et nFAULT au 3,3 V) ; 2 × 0,2 Ω sur AISEN/BISEN (facultatives) |

## Câblage

| XIAO | Vers | Rôle |
|---|---|---|
| D2 | DRV8833 AIN1 (16) | Moteur 1 — commande 1 |
| D3 | DRV8833 AIN2 (15) | Moteur 1 — commande 2 |
| D6 | DRV8833 BIN1 (9) | Moteur 2 — commande 1 |
| D7 | DRV8833 BIN2 (10) | Moteur 2 — commande 2 |
| D4 | Encodeur 1 C1 (voie A) | Comptage moteur 1 |
| D5 | Encodeur 1 C2 (voie B) | Comptage moteur 1 |
| D1 | Encodeur 2 C1 (voie A) | Comptage moteur 2 |
| D10 | Encodeur 2 C2 (voie B) | Comptage moteur 2 |
| D0 | DRV8833 nFAULT (8), en direct | Défaut du pont en H (nouveau en v5.1) — la 10 kΩ nFAULT → 3V3 reste en place ; nSLEEP inchangé |
| 3V3 | VCC des deux encodeurs, nSLEEP et nFAULT (via 10 kΩ) | Alimentation logique |
| 5V | +5 V externe | Alimentation de la carte |
| GND | Masse commune | — |

Sorties DRV8833 : AOUT1 (2) / AOUT2 (4) → fils M1 / M2 du moteur 1 ; BOUT1 (7) / BOUT2 (5) → fils M1 / M2 du moteur 2. VM (12) → +5 V externe.

> ⚠ **Encodeurs en 3,3 V uniquement** : les broches du ESP32-C3 ne tolèrent pas le 5 V.
> ⚠ Ne rien brancher sur **D8 et D9** : ce sont des broches de démarrage. D0 en est une aussi, mais nFAULT y convient car sa résistance de rappel le tient à l'état haut.
> Sans fil vers D0, mettre `NFAULT_ENABLED = false`.

---

## Installation

1. **Arduino IDE** avec le paquet **esp32 d'Espressif** (2.x ou 3.x).
2. Le dossier du croquis contient trois fichiers : `StoreEnrouleur_MoteurCC_Encodeur.ino`, **`favicon.h`** (icône de la page, fourni) et `arduino_secrets.h`, à créer :

   ```cpp
   #define SECRET_SSID      "NomDeLaBox"
   #define SECRET_PASS      "MotDePasseWiFi"
   #define SECRET_AP_SSID   "Stores-Secours"
   #define SECRET_AP_PASS   "8caracteresMini"
   #define SECRET_OTA_PASS  "MotDePasseOTA"
   ```

   Avec plusieurs cartes, donner à chacune un `SECRET_AP_SSID` différent (ex. `"Store-Salon"`, `"Store-Chambre"`).

3. Réglages de l'IDE :
   - Carte : **XIAO_ESP32C3**
   - Outils > **USB CDC On Boot > Enabled** (obligatoire : D6/D7 servent au moteur 2)
   - Outils > Partition Scheme : un schéma **avec OTA** (celui par défaut convient)
4. Téléverser par USB, ouvrir le moniteur série à **115200 bauds** et noter l'adresse IP.
5. Ouvrir `http://store.local` (ou l'IP) et **calibrer chaque moteur** (voir ci-dessous).

## Première mise en service (pour chaque moteur)

1. Carte « Calibration » → **Démarrer**, puis un appui bref sur **▼ Descendre** :
   - le store **monte** au lieu de descendre → `MOTx_INVERT = true`
   - le store descend mais s'arrête avec **« sens incohérent »** → `ENCx_INVERT = true`
   - (re-téléverser après modification, puis recommencer)
2. Monter tout en haut, **Stop**, **Valider HAUT**.
3. Descendre tout en bas, **Stop**, **Valider BAS**.

> Les calibrations des versions à moteurs pas à pas (v4 et antérieures) ne sont **pas reprises** : la v5 compte en impulsions d'encodeur.

---

## Réglages principaux (en tête du programme)

| Constante | Défaut | Rôle |
|---|---|---|
| `MOTOR2_ENABLED` | `true` | `false` = un seul store |
| `MOT1_INVERT` / `MOT2_INVERT` | `false` | Inverse le sens de rotation |
| `ENC1_INVERT` / `ENC2_INVERT` | `false` | Inverse le sens de comptage |
| `MOT1_NAME` / `MOT2_NAME` | `"Store 1"` / `"Store 2"` | Noms affichés |
| `HOSTNAME` | `"store"` | Nom réseau (`http://store.local`, port OTA) — **unique par carte** |
| `SPEED_MAX` | 1023 | Vitesse normale (0–1023) |
| `SPEED_CAL` | 600 | Vitesse pendant la calibration |
| `SPEED_MIN` | 400 | Vitesse minimale de démarrage (augmenter si « BLOCAGE » au départ) |
| `RAMP_STEP` | 40 | Accélération (par 10 ms) |
| `SLOW_ZONE_PCT` / `SLOW_ZONE_MIN` | 5 % / 40 imp. | Zone de ralentissement avant l'arrêt |
| `STALL_MS` | 400 ms | Délai de détection de blocage |
| `WRONG_DIR_COUNTS` | 60 imp. | Seuil de sens incohérent |
| `NFAULT_ENABLED` | `true` | Surveillance de nFAULT sur D0 (`false` si non câblé) |
| `AP_RETRY_MS` | 5 min | Intervalle des tentatives de retour sur la box en mode secours |

---

## Adresses web

| Adresse | Usage |
|---|---|
| `/` | Page de commande |
| `/update` | Mise à jour du programme par le navigateur |
| `/api/status` | État en JSON |
| `/api/cmd?a=<action>&m=<moteur>[&p=<%>]` | Commande |
| `/favicon.ico`, `/favicon.png`, `/apple-touch-icon.png` | Icône de la page (gardée 7 jours en cache par le navigateur) |

`/update/check?pw=…` sert à la page `/update` pour vérifier le mot de passe avant l'envoi.

`m` : `1`, `2`, ou `0` = les deux. `a` : `up`, `down`, `stop`, `goto` (avec `p` = 0 à 100), `calstart`, `caltop`, `calbot` (moteur 1 ou 2 obligatoire).

Exemple : `http://store.local/api/cmd?a=goto&m=0&p=30` → les deux stores à 30 %.

## Mise à jour OTA

- **Arduino IDE** : Outils > Port → `store at <IP>`, téléverser, saisir le mot de passe OTA.
- **Navigateur** : Croquis > *Exporter les binaires compilés*, puis `http://store.local/update`, mot de passe OTA et fichier **`.ino.bin`** (pas `merged` ni `bootloader`). Le mot de passe est vérifié avant l'envoi du fichier.

Les moteurs sont arrêtés et les positions sauvegardées avant l'écriture.

## Point d'accès de secours

Si la box n'est pas trouvée en 15 s, la carte crée le réseau `SECRET_AP_SSID`. S'y connecter puis ouvrir `http://192.168.5.1`.

Toutes les 5 min, si **personne n'est connecté** au point d'accès, la carte retente la box pendant 15 s ; dès qu'elle la retrouve, elle coupe le point d'accès et redevient joignable sur `http://store.local`.

Sur un téléphone, accepter de **rester connecté** au réseau « sans Internet » (sinon il repasse en 4G/5G) et utiliser l'adresse IP plutôt que `.local`.

## Plusieurs ESP32

Chaque carte en mode secours crée **son propre réseau**, isolé des autres : toutes peuvent garder l'adresse `192.168.5.1`.

| Réglage | Sur chaque carte | Pourquoi |
|---|---|---|
| `SECRET_AP_SSID` | **différent** | Savoir à quelle carte on se connecte en mode secours |
| `HOSTNAME` | **différent** | Sur la box, deux cartes `store` se disputeraient `store.local` et l'OTA pourrait viser la mauvaise carte |
| `AP_IP` | identique possible | Réseaux de secours séparés |
| Mots de passe | identiques possibles | Au choix |

**Accès en mode secours** : se connecter au réseau Wi-Fi de la carte voulue (ex. « Store-Salon »), ouvrir `http://192.168.5.1` ; pour l'autre carte, changer de réseau Wi-Fi et recharger la même adresse. Un appareil ne commande qu'une carte à la fois en mode secours.

> La plage `192.168.5.x` ne doit pas être celle de la box (une Livebox utilise en général `192.168.1.x`).

## Icône de la page

`favicon.h` contient une image PNG 180 × 180 px stockée en flash. Pour la remplacer, régénérer `favicon.h` à partir d'un autre PNG carré en gardant les noms `favicon_png` et `favicon_png_len`. Après un changement, vider le cache du navigateur ou recréer le raccourci de l'écran d'accueil.

---

## Documentation

- **Notice d'utilisation de la page web** — utilisation quotidienne.
- **Manuel complet** — montage, installation, calibration, fonctionnement interne, réglages, API, dépannage.

## Historique

| Version | Évolution |
|---|---|
| v2 | Sauvegarde de la position pendant le mouvement, OTA |
| v3 | Deuxième moteur, alignement des deux côtés |
| v3.1 | OTA réparée, page `/update`, force du signal Wi-Fi |
| v4 | Moteurs indépendants |
| v4.1 / v4.2 | Mise en page 2 colonnes, boutons ▲ ■ ▼ |
| v5 | Moteurs CC à encodeur + DRV8833 : PWM 20 kHz, rampe, ralentissement, frein actif, détection de blocage et de sens incohérent |
| v5.1 | Mot de passe de `/update` vérifié avant l'envoi, message d'échec simplifié, retour automatique sur la box en mode secours, lecture de nFAULT sur D0. Calibration v5 conservée |
| **v5.2** | Icône de la page web (`favicon.h`) : onglet et écran d'accueil du téléphone. Documentation : plusieurs ESP32, accès en mode secours. Calibration conservée |
