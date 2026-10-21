# Stores enrouleurs — XIAO ESP32-C3 · moteurs CC 12 V à encodeur (v8.1)

Commande de **un ou deux stores enrouleurs** (deux stores indépendants, ou un store entraîné des deux côtés) depuis une **page web** hébergée par la carte, sans application ni service en ligne (pas de Blynk).

Chaque store est entraîné par un **motoréducteur JGB37-520 12 V avec encodeur magnétique** (rapport 1:270, environ 30 à 37 tr/min) piloté par un **module DRV8876** (un module par moteur). La position est mesurée par l'encodeur, ce qui permet d'aller à un pourcentage précis et de détecter un blocage. Le courant de chaque moteur est **plafonné par le driver** (environ 1,3 A).

La **v8** reprend les options de la version à moteurs pas à pas v7.3.4 (réglages depuis la page, mode synchronisé, page d'informations, titre de page par carte), adaptées aux moteurs à courant continu. Câblage et calibrations de la v6 conservés.

---

## Fonctionnalités

- Page web responsive (téléphone, tablette, PC) : `http://store.local` ou adresse IP
- Deux stores **indépendants** + commande groupée « les deux », ou **synchronisés** : un seul store entraîné des deux côtés, les encodeurs gardant les deux côtés à la même position (écart corrigé en permanence, arrêt « DÉSYNCHRONISÉS » au-delà de 1/8 de tour) ; carte **Alignement des deux côtés**
- Changement de mode protégé par le **mot de passe OTA** (fenêtre sur la page, envoi en POST)
- Menu **Réglage Moteurs** : nom de chaque store, **sens**, **sens de comptage de l'encodeur** et **vitesse maximale** (40 à 100 %), enregistrés dans la carte — plus besoin de re-téléverser
- Monter / Stop / Descendre, et **Aller à X %** (0 % = haut, 100 % = bas)
- **Calibration** des butées haute et basse depuis la page, moteur par moteur (une seule en synchronisé)
- Position mesurée par **encodeur en quadrature ×4** (interruptions) : environ **11 880 impulsions par tour** du tube
- **PWM 20 kHz** (inaudible), rampe d'accélération, **ralentissement** avant l'arrêt
- **Freinage actif** à l'arrêt
- **Limitation de courant** par le DRV8876 (réglée par VREF)
- Sécurités : **détection de blocage** (encodeur immobile 400 ms), **sens moteur/encodeur incohérent**, **désynchronisation** et **défaut driver** (surintensité / surchauffe, lu sur FUT)
- Inversion de sens rapprochée gérée (pause freinée de 200 ms)
- **Position sauvegardée** en mémoire non volatile (pendant le mouvement et à l'arrêt) : elle est retrouvée après une coupure de courant
- **Mise à jour OTA** : depuis l'Arduino IDE (port réseau) ou par le navigateur (`/update`), protégée par mot de passe
- **Point d'accès de secours** (`192.168.5.1`) si la box est introuvable, avec **retour automatique sur la box** (nouvelle tentative toutes les 2 min)
- **Plusieurs cartes** possibles sur le même réseau (voir « Plusieurs ESP32 »)
- **PAGE_TITLE** : titre de la page et nom du raccourci propres à chaque carte ; **icône** `favicon.h` dans l'onglet et sur l'écran d'accueil du téléphone (`/manifest.json`)
- Cadres **« Store Gauche » / « Store Droit »**, page fluide (une seule requête à la fois, abandon au bout de 2 s)
- **Page d'informations** `/info` : version, durée de fonctionnement, cause du dernier démarrage, réseau et **signal Wi-Fi**, mémoire, **compteurs** par moteur (mouvements, tours du tube, cycles complets)
- **API HTTP** simple pour une domotique (`/api/status`, `/api/cmd`, `/api/set`, `/api/info`)

---

## Matériel

| Élément | Référence / valeur |
|---|---|
| Microcontrôleur | Seeed Studio XIAO ESP32-C3 |
| Drivers moteur | 2 × module DRV8876 (carte rouge, sérigraphie : VM, GND, EN IN1, PH IN2, POE, SEP, VREF / VM, GND, OUT2, OUT1, IOE, FUT, CS) |
| Moteurs | 2 × motoréducteur JGB37-520 12 V, 1:270, encodeur à effet Hall 11 impulsions/tour moteur |
| Alimentation | Bloc 12 V, **3 A minimum** |
| Alimentation du XIAO | Petit convertisseur 12 V → 5 V (ou USB-C) |
| Condensateur | 220 à 470 µF / 25 V sur le 12 V, près des modules |
| Résistance | 1 × 10 kΩ (rappel de nFAULT au 3,3 V, commune aux deux modules) |

## Câblage

| XIAO | Vers | Rôle |
|---|---|---|
| D2 | Module 1 EN/IN1 | Moteur 1 — commande 1 |
| D3 | Module 1 PH/IN2 | Moteur 1 — commande 2 |
| D6 | Module 2 EN/IN1 | Moteur 2 — commande 1 |
| D7 | Module 2 PH/IN2 | Moteur 2 — commande 2 |
| D4 | Encodeur 1, fil jaune (voie A) | Comptage moteur 1 |
| D5 | Encodeur 1, fil vert (voie B) | Comptage moteur 1 |
| D1 | Encodeur 2, fil jaune (voie A) | Comptage moteur 2 |
| D10 | Encodeur 2, fil vert (voie B) | Comptage moteur 2 |
| D0 | FUT des deux modules reliés + 10 kΩ vers 3V3 | Défaut d'un driver |
| 3V3 | SEP et POE des deux modules, fils bleus des encodeurs, 10 kΩ de FUT, VREF (voir ci-dessous) | Alimentation logique |
| 5V | Sortie du convertisseur 12 V → 5 V | Alimentation de la carte |
| GND | Masse commune (alimentation, modules, IOE des deux modules, fils noirs des encodeurs) | — |

Sur chaque module. Quatre repères de la sérigraphie correspondent à des broches de la puce sous un autre nom (les traductions du vendeur, « Power Output Enable », « Separate »…, sont trompeuses) :

| Sérigraphie | Broche DRV8876 | Patte de la puce |
|---|---|---|
| POE | PMODE | 16 |
| SEP | nSLEEP | 3 |
| IOE | IMODE | 7 |
| FUT | nFAULT | 4 |


| Repère | Relier à |
|---|---|
| VM | +12 V |
| GND | Masse commune |
| EN/IN1, PH/IN2 | XIAO (tableau ci-dessus) |
| POE | 3V3 → mode PWM « IN/IN ». **Ne pas laisser en l'air** |
| SEP | 3V3 (la carte le tire à la masse : sinon le driver reste en veille) |
| IOE | GND (évite que nFAULT signale chaque limitation de courant comme un défaut) |
| VREF | Limitation de courant : I ≈ VREF / 2,49 → 3V3 donne environ **1,3 A**. **Mesurer VREF ↔ GND avant**, voir ci-dessous |
| FUT | Relié à celui de l'autre module, puis D0 (+ 10 kΩ vers 3V3) |
| CS | Non relié |
| OUT1 / OUT2 | Fil rouge / fil blanc du moteur |

> ⚠ **VREF : mesurer à l'ohmmètre entre VREF et GND, module débranché.** Le schéma de la carte montre une résistance R4 de 0 Ω entre VREF et la masse ; sur les photos du module, cet emplacement semble vide, mais seule la mesure le confirme.
> - **Environ 0 Ω** : ne **pas** relier VREF au 3V3 (court-circuit). Essayer d'abord sans rien brancher sur VREF. Si le moteur ne tourne pas ou très faiblement, dessouder R4, puis relier VREF au 3V3.
> - **Environ 10 kΩ ou plus** : relier VREF au 3V3.

> ⚠ **Encodeurs en 3,3 V uniquement** (fil bleu sur 3V3), même si la fiche du moteur conseille 5 V : les broches du ESP32-C3 ne tolèrent pas le 5 V.
> ⚠ Ne rien brancher sur **D8 et D9** : ce sont des broches de démarrage. D0 en est une aussi, mais nFAULT y convient car sa résistance de rappel le tient à l'état haut.
> Sans fil vers D0, mettre `NFAULT_ENABLED = false`.

---

## Installation

1. **Arduino IDE** avec le paquet **esp32 d'Espressif** (2.x ou 3.x).
2. Le dossier `StoreEnrouleur_JGB37_DRV8876` contient trois fichiers : `StoreEnrouleur_JGB37_DRV8876.ino`, **`favicon.h`** (icône de la page, fourni) et `arduino_secrets.h`, à créer :

   ```cpp
   #define SECRET_SSID      "NomDeLaBox"
   #define SECRET_PASS      "MotDePasseWiFi"
   #define SECRET_AP_SSID   "Stores-Secours"
   #define SECRET_AP_PASS   "8caracteresMini"
   #define SECRET_OTA_PASS  "MotDePasseOTA"
   ```

   `SECRET_OTA_PASS` protège aussi le changement de mode (indépendants / synchronisés).
   Avec plusieurs cartes, donner à chacune un `SECRET_AP_SSID`, un `HOSTNAME`, et un `PAGE_TITLE` différents.

3. Réglages de l'IDE :
   - Carte : **XIAO_ESP32C3**
   - Outils > **USB CDC On Boot > Enabled** (obligatoire : D6/D7 servent au moteur 2)
   - Outils > Partition Scheme : un schéma **avec OTA** (celui par défaut convient)
4. Téléverser par USB, ouvrir le moniteur série à **115200 bauds** et noter l'adresse IP.
5. Ouvrir `http://<HOSTNAME>.local` (ou l'IP) et **calibrer chaque moteur** (voir ci-dessous).

> Nom réseau : lettres minuscules, chiffres et tiret seulement (« store-codeur »). Avec un tiret bas (« store_codeur »), `.local` peut ne pas répondre sur certains appareils.

## Première mise en service (pour chaque moteur)

1. Carte « Calibration » → **Démarrer**, puis un appui bref sur **▼ Descendre** :
   - le store **monte** au lieu de descendre → Réglage Moteurs › **Inverser le sens**
   - le store descend mais s'arrête avec **« sens incohérent »** → Réglage Moteurs › **Inverser le comptage**
   - (chaque inversion efface la calibration : recliquer sur « Démarrer » et recommencer)
2. Monter tout en haut, **Stop**, **Valider HAUT**.
3. Descendre tout en bas, **Stop**, **Valider BAS**.

> **Depuis la v6** : la v8 garde le même espace mémoire (`rollerjgb`), donc calibrations, positions et sens sont **conservés**.
> **Depuis la v5** (moteurs 6 V) : calibrations non reprises, calibrer chaque store.
> **Mode synchronisé** : Réglage Moteurs › « Synchronisés » + mot de passe OTA ; la calibration est effacée, en refaire une seule pour les deux moteurs. Faire l'étape 1 côté par côté (moteurs face à face : sens réglable séparément) et utiliser **Alignement des deux côtés** pour remettre le store droit.

---

## Réglages principaux (en tête du programme)

Le sens, le comptage, la vitesse, les noms et le mode se règlent sur la page : les constantes correspondantes ne servent qu'au tout premier démarrage.

| Constante | Défaut | Rôle |
|---|---|---|
| `MOTOR2_ENABLED` | `false` | `true` = deux moteurs (et choix du mode synchronisé) |
| `SYNC_DEFAULT` | `false` | Mode au tout premier démarrage |
| `MOT1_INVERT` / `MOT2_INVERT`, `ENC1_INVERT` / `ENC2_INVERT` | `false` | Sens et comptage au premier démarrage |
| `MOT1_NAME` / `MOT2_NAME` | `"Store 1"` / `"Store 2"` | Noms par défaut |
| `MOT1_SIDE` / `MOT2_SIDE` | `"Store Gauche"` / `"Store Droit"` | Titres des cadres |
| `PAGE_TITLE` | `"Stores"` | Titre de la page et nom du raccourci — **différent sur chaque carte** |
| `HOSTNAME` | `"store_codeur"` | Nom réseau — **unique par carte** (préférer `"store-codeur"`) |
| `ENC_PPR` / `GEAR_RATIO` / `MOTOR_RPM` | 11 / 270 / 37 | Motoréducteur (affichage seulement) |
| `SPEED_PCT_DEFAULT` / `SPEED_PCT_MIN` | 100 / 40 | Vitesse maximale au premier démarrage, minimum du curseur (%) |
| `SPEED_CAL` | 600 | Vitesse pendant la calibration |
| `SPEED_MIN` | 400 | Vitesse minimale de démarrage, environ 4,8 V (augmenter si « BLOCAGE » au départ) |
| `SPEED_JOG` | 500 | Vitesse de l'alignement (synchronisé) |
| `RAMP_STEP` | 40 | Accélération (par 10 ms) |
| `SLOW_ZONE_PCT` / `SLOW_ZONE_MIN` | 5 % / 500 imp. | Zone de ralentissement avant l'arrêt |
| `SYNC_GAIN` / `SYNC_MAX_ERR` / `SYNC_TOL` | 3 / 1/8 de tour / 20 imp. | Correction de l'écart, arrêt « DÉSYNCHRONISÉS », rattrapage à l'arrivée |
| `STALL_MS` | 400 ms | Délai de détection de blocage |
| `WRONG_DIR_COUNTS` | 300 imp. | Seuil de sens incohérent |
| `NFAULT_ENABLED` | `true` | Surveillance de FUT sur D0 (`false` si non câblé) |
| `AP_RETRY_MS` | 2 min | Intervalle des tentatives de retour sur la box en mode secours |

---

## Adresses web

| Adresse | Usage |
|---|---|
| `/` | Page de commande |
| `/info` | Page d'informations |
| `/update` | Mise à jour du programme par le navigateur |
| `/api/status` | État en JSON |
| `/api/cmd?a=<action>&m=<moteur>[&p=<%>]` | Commande |
| `/api/set?m=<moteur>[&name=…][&inv=…][&enc=…][&spd=…]` | Réglages : nom, sens (0 ou 1), comptage (0 ou 1), vitesse (40 à 100 %). En POST, `sync` (0 ou 1) et `pw` (mot de passe OTA) changent le mode |
| `/api/info` | Informations et compteurs (`?reset=1` ou `?reset=2` : remise à zéro) |
| `/favicon.ico`, `/favicon.png`, `/apple-touch-icon.png`, `/icon.png` | Icône de la page (`favicon.h`, gardée 7 jours en cache par le navigateur) |
| `/manifest.json` | Nom et icône du raccourci du téléphone |

`/update/check?pw=…` sert à la page `/update` pour vérifier le mot de passe avant l'envoi.

`m` : `1`, `2`, ou `0` = les deux. `a` : `up`, `down`, `stop`, `goto` (avec `p` = 0 à 100), `calstart`, `caltop`, `calbot` (moteur 1 ou 2 obligatoire). En synchronisé, toutes pilotent les deux moteurs ; `jog` (avec `p` = impulsions, négatif = monter) aligne un seul côté.

Exemple : `http://store.local/api/cmd?a=goto&m=0&p=30` → les deux stores à 30 %.

## Mise à jour OTA

- **Arduino IDE** : Outils > Port → `store at <IP>`, téléverser, saisir le mot de passe OTA.
- **Navigateur** : Croquis > *Exporter les binaires compilés*, puis `http://store.local/update`, mot de passe OTA et fichier **`.ino.bin`** (pas `merged` ni `bootloader`). Le mot de passe est vérifié avant l'envoi du fichier.

Les moteurs sont arrêtés et les positions sauvegardées avant l'écriture.

## Point d'accès de secours

Si la box n'est pas trouvée en 15 s, la carte crée le réseau `SECRET_AP_SSID`. S'y connecter puis ouvrir `http://192.168.5.1`.

Toutes les 2 min, si **personne n'est connecté** au point d'accès, la carte retente la box pendant 15 s ; dès qu'elle la retrouve, elle coupe le point d'accès et redevient joignable sur `http://store.local`.

Sur un téléphone, accepter de **rester connecté** au réseau « sans Internet » (sinon il repasse en 4G/5G) et utiliser l'adresse IP plutôt que `.local`.

## Plusieurs ESP32

Chaque carte en mode secours crée **son propre réseau**, isolé des autres : toutes peuvent garder l'adresse `192.168.5.1`.

| Réglage | Sur chaque carte | Pourquoi |
|---|---|---|
| `SECRET_AP_SSID` | **différent** | Savoir à quelle carte on se connecte en mode secours |
| `HOSTNAME` | **différent** | Sur la box, deux cartes `store` se disputeraient `store.local` et l'OTA pourrait viser la mauvaise carte |
| `PAGE_TITLE` | **différent** | Un raccourci par carte sur le téléphone, reconnaissable |
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
| v5.2 | Icône de la page web (`favicon.h`) : onglet et écran d'accueil du téléphone. Documentation : plusieurs ESP32, accès en mode secours. Calibration conservée |
| v6 | Moteurs JGB37-520 12 V (1:270) et un module DRV8876 par moteur, avec limitation de courant. Zone de ralentissement (500 imp.) et seuil de sens incohérent (300 imp.) adaptés à la résolution. Message « DÉFAUT DRIVER ». Nouvel espace mémoire : **recalibration obligatoire** |
| v8 | Options de la v7.3.4 adaptées aux moteurs CC : menu Réglage Moteurs (nom, sens, comptage, vitesse), mode synchronisé avec correction par les encodeurs et alignement, changement de mode protégé par mot de passe, page `/info` et compteurs, cadres Store Gauche / Droit, file d'attente des requêtes, `PAGE_TITLE`, retour sur la box toutes les 2 min. Calibrations de la v6 conservées |
| **v8.1** | Retour de l'icône `favicon.h` à la place de l'icône dessinée par le programme (`ICON_COLOR` supprimé). Calibrations et réglages conservés |
