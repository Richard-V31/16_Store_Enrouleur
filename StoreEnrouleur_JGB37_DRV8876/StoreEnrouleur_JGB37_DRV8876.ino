// ======================================================
// XIAO ESP32-C3 : Commande de stores enrouleurs par page web
// Version autonome, sans Blynk — DEUX MOTEURS CC À ENCODEUR, INDÉPENDANTS OU SYNCHRONISÉS
// v2 à v4.2 : sauvegarde de la position, OTA, deuxième moteur, moteurs indépendants,
//             mise en page 2 colonnes, boutons ▲ ■ ▼
// v5 : moteurs à courant continu avec encodeur magnétique (quadrature ×4, interruptions),
//      PWM 20 kHz, rampe, ralentissement, frein actif, détection de blocage et de sens incohérent
// v5.1 : /update avec vérification du mot de passe avant l'envoi, retour automatique sur la box
//        en mode secours, lecture de nFAULT sur D0
// v5.2 : icône de la page (favicon.h)
// v6 : motoréducteurs JGB37-520 12 V (1:270) + un module DRV8876 par moteur ;
//      SLOW_ZONE_MIN 500 et WRONG_DIR_COUNTS 300 ; message « DÉFAUT DRIVER »
// v8 : reprise des options de la version pas à pas v7.3.4, adaptées aux moteurs CC :
//      - menu « Réglage Moteurs » : nom de chaque store, sens du store, sens de comptage
//        de l'encodeur et vitesse maximale, réglés depuis la page et enregistrés en NVS
//        (plus besoin de re-téléverser pour MOTx_INVERT / ENCx_INVERT)
//      - fonctionnement « Indépendants » ou « Synchronisés » (un seul store entraîné
//        des deux côtés) : les deux moteurs suivent la même position, l'encodeur de chacun
//        corrige l'écart en temps réel ; écart trop grand → arrêt « désynchronisés ».
//        Carte « Alignement des deux côtés » pour remettre le store droit.
//        Le changement de mode demande le mot de passe OTA (fenêtre sur la page, envoi en POST).
//      - page d'informations /info : version, durée de fonctionnement, cause du dernier
//        démarrage, réseau (connexion, signal Wi-Fi), mémoire, compteurs de chaque moteur
//        (mouvements, tours du tube, cycles complets), avec remise à zéro
//      - cadres « Store Gauche » / « Store Droit » (MOT1_SIDE / MOT2_SIDE), nom et position centrés
//      - page plus fluide : une seule requête à la fois vers la carte (file d'attente),
//        abandon au bout de 2 s ; le curseur « Aller à » démarre sur la position actuelle
//      - PAGE_TITLE : titre de la page et nom du raccourci propres à chaque carte,
//        manifeste (raccourci du téléphone)
//      - point d'accès de secours : nouvel essai vers la box toutes les 2 min
//      Les calibrations et positions de la v6 sont CONSERVÉES (même espace NVS).
// v8.1 : retour de l'icône favicon.h (image PNG 180 × 180 en flash) à la place de l'icône
//        dessinée par le programme (la constante de couleur disparaît). favicon.h doit être
//        dans le dossier du croquis.
// ------------------------------------------------------
// Carte Arduino IDE : « XIAO_ESP32C3 » (paquet esp32 d'Espressif, 2.x ou 3.x)
// Pour voir le moniteur série : Outils > USB CDC On Boot > Enabled
//   (obligatoire ici : D6/D7 servent au moteur 2, le moniteur passe par l'USB)
// Pour l'OTA : Outils > Partition Scheme > un schéma avec OTA (celui par défaut convient)
// Page web : http://store.local  ou l'adresse IP affichée sur le port série
// Informations : http://store.local/info
// Mise à jour par le navigateur : http://store.local/update
// Si le Wi-Fi de la box est introuvable : réseau du point d'accès de secours
// (SECRET_AP_SSID / SECRET_AP_PASS), puis http://192.168.5.1
// ------------------------------------------------------
// Câblage (inchangé depuis la v6) :
//   Alimentation : bloc 12 V, 3 A minimum (deux moteurs) ; masse commune avec le XIAO
//     XIAO alimenté par un convertisseur 12 V → 5 V (broche 5V) ou par l'USB
//     Condensateur 220 à 470 µF / 25 V entre +12 V et GND, près des modules
//   Module DRV8876 rouge, UN PAR MOTEUR (repères VM, GND, EN IN1, PH IN2, POE, SEP, VREF /
//   VM, GND, OUT2, OUT1, IOE, FUT, CS). POE = PMODE, SEP = nSLEEP, IOE = IMODE, FUT = nFAULT
//     VM → +12 V, GND → masse commune
//     POE → 3V3 (mode PWM « IN/IN » ; ne jamais laisser en l'air)
//     SEP → 3V3 (sinon le driver reste en veille)
//     IOE → GND (évite les faux « DÉFAUT DRIVER » pendant la limitation de courant)
//     VREF → 3V3 après avoir mesuré VREF ↔ GND (≈ 0 Ω : ne pas relier, voir le manuel)
//       limitation : I ≈ VREF / 2,49 → environ 1,3 A
//     FUT des DEUX modules reliés ensemble → D0 + UNE résistance de 10 kΩ vers 3V3
//     CS → non relié
//     Moteur 1 : D2 → EN IN1, D3 → PH IN2 du module 1, OUT1 → fil rouge, OUT2 → fil blanc
//     Moteur 2 : D6 → EN IN1, D7 → PH IN2 du module 2, OUT1 → fil rouge, OUT2 → fil blanc
//   Encodeurs (alimentés en 3,3 V, JAMAIS en 5 V : broches du XIAO non tolérantes)
//     Moteur 1 : jaune (A) → D4, vert (B) → D5, bleu (VCC) → 3V3, noir (GND) → GND
//     Moteur 2 : jaune (A) → D1, vert (B) → D10, bleu (VCC) → 3V3, noir (GND) → GND
// Broches restées libres : D8, D9 (broches de démarrage : jamais d'encodeur dessus).
// ------------------------------------------------------
// PREMIÈRE MISE EN SERVICE (pour chaque moteur, page web) :
//   1. Calibration > « Démarrer », puis un appui bref sur ▼ Descendre :
//      - le store MONTE au lieu de descendre → Réglage Moteurs > « Inverser le sens »
//      - il descend mais s'arrête sur « sens incohérent » → « Inverser le comptage »
//      (chaque inversion efface la calibration : recliquer sur « Démarrer »)
//   2. Monter tout en haut, Stop, « Valider HAUT »
//   3. Descendre tout en bas, Stop, « Valider BAS »
// ------------------------------------------------------
// Commandes web : /api/cmd?a=<action>&m=<moteur>[&p=<pourcentage>]
//   m = 1 ou 2 : un seul moteur ; m = 0 : les deux moteurs
//   a = up, down, stop, goto (avec p = 0 à 100),
//       calstart, caltop, calbot (m = 1 ou 2 obligatoire)
//   En synchronisé, toutes ces commandes pilotent les deux moteurs ensemble.
//   a = jog (synchronisé seulement) : m = 1 ou 2, p = impulsions (négatif = monter)
// Réglages : /api/set?m=<1 ou 2>[&name=<nom>][&inv=<0|1>][&enc=<0|1>][&spd=<% vitesse>]
//            /api/set  (POST) sync=<0|1>&pw=<mot de passe OTA>
// Infos    : /api/info  (avec &reset=<1 ou 2> : remise à zéro des compteurs)
// ======================================================
#include "arduino_secrets.h"   // Fichier séparé contenant le nom du réseau (SSID) et le mot de passe WiFi
#include "favicon.h"           // Icône de la page web (PNG 180 × 180 stocké en flash)

#include <WiFi.h>          // Bibliothèque Wi-Fi de l'ESP32
#include <WebServer.h>     // Serveur web intégré au paquet ESP32 (aucune bibliothèque à installer)
#include <ESPmDNS.h>       // Permet d'accéder à la carte par le nom « store.local »
#include <Preferences.h>   // Stockage en mémoire non volatile (NVS)
#include <ArduinoOTA.h>    // Mise à jour par le réseau depuis l'Arduino IDE (intégré au paquet ESP32)
#include <Update.h>        // Écriture d'un nouveau programme en flash (mise à jour par le navigateur)
#include "esp_timer.h"     // Temporisateur de l'ESP32 (régulation des moteurs indépendante du réseau)
#include "esp_system.h"    // Cause du dernier démarrage (page d'informations)
#include "esp_arduino_version.h"   // Version du paquet ESP32 (la fonction PWM a changé en 3.x)
#include "soc/soc.h"       // Macro REG_READ (lecture directe d'un registre)
#include "soc/gpio_reg.h"  // Registre d'entrée des GPIO (lecture ultra-rapide des encodeurs)

#ifndef SECRET_OTA_PASS                     // Si le mot de passe OTA n'est pas dans arduino_secrets.h…
#define SECRET_OTA_PASS "store-ota"         // …mot de passe par défaut (À CHANGER dans arduino_secrets.h)
#endif                                      // Fin du test

const char* FW_VERSION = "8.1";             // Version du programme (affichée sur la page d'informations)

// ----------- WIFI -----------
char WIFI_SSID[] = SECRET_SSID;              // Nom du réseau Wi-Fi de la box
char WIFI_PASS[] = SECRET_PASS;             // Mot de passe du Wi-Fi
char AP_SSID[] = SECRET_AP_SSID;            // Nom du point d'accès de secours créé par la carte
char AP_PASS[]  = SECRET_AP_PASS;           // Mot de passe du point d'accès (8 caractères minimum)
const char* HOSTNAME  = "store_codeur";     // Nom réseau : la page sera sur http://store_codeur.local 🚩
const IPAddress AP_IP(192, 168, 5, 1);       // Adresse IP de la carte en mode point d'accès local
const IPAddress AP_MASK(255, 255, 255, 0);  // Masque de sous-réseau du point d'accès

// 🚨----------- APPARENCE (à changer sur chaque carte) -----------
// Avec plusieurs cartes, donnez à chacune un titre différent : il sert au titre
// de la page et au nom proposé pour le raccourci du téléphone (« Ajouter à l'écran
// d'accueil »). L'icône est celle de favicon.h.
// Titre : éviter les caractères < > & " et \ (ils casseraient la page).
const char* PAGE_TITLE = "Stores";          // Titre de la page et nom du raccourci 🚩

// ----------- OTA -----------
const char OTA_PASS[] = SECRET_OTA_PASS;    // Mot de passe OTA (Arduino IDE, page /update, changement de mode)
const uint16_t OTA_PORT = 3232;             // Port réseau de l'OTA Arduino (valeur standard)

// ----------- BROCHES (XIAO ESP32-C3) -----------
const uint8_t M1_IN1 = D2;    // GPIO4  → EN IN1 du module 1
const uint8_t M1_IN2 = D3;    // GPIO5  → PH IN2 du module 1
const uint8_t M1_ENC_A = D4;  // GPIO6  ← voie A (fil jaune) de l'encodeur 1
const uint8_t M1_ENC_B = D5;  // GPIO7  ← voie B (fil vert) de l'encodeur 1
const uint8_t M2_IN1 = D6;    // GPIO21 → EN IN1 du module 2
const uint8_t M2_IN2 = D7;    // GPIO20 → PH IN2 du module 2
const uint8_t M2_ENC_A = D1;  // GPIO3  ← voie A (fil jaune) de l'encodeur 2
const uint8_t M2_ENC_B = D10; // GPIO10 ← voie B (fil vert) de l'encodeur 2
const uint8_t DRV_FAULT = D0; // GPIO2  ← FUT (nFAULT) des deux modules reliés (0 = défaut)

// 🚨----------- RÉGLAGES DES MOTEURS -----------
// Nom, sens, sens de comptage, vitesse et mode (indépendants / synchronisés) se règlent
// sur la page web (menu « Réglage Moteurs »). Les valeurs ci-dessous ne servent qu'au
// tout premier démarrage, tant que rien n'a été enregistré depuis la page.
const bool MOTOR2_ENABLED = true;      // false = fonctionnement avec 1 moteur, true = 2 moteurs 🚩
const bool SYNC_DEFAULT   = false;      // Mode au premier démarrage : false = indépendants, true = synchronisés 🚩
const bool MOT1_INVERT    = false;      // Sens par défaut du moteur 1 (premier démarrage)
const bool MOT2_INVERT    = false;      // Sens par défaut du moteur 2 (premier démarrage)
const bool ENC1_INVERT    = false;      // Sens de comptage par défaut de l'encodeur 1 (premier démarrage)
const bool ENC2_INVERT    = false;      // Sens de comptage par défaut de l'encodeur 2 (premier démarrage)
const char* MOT1_NAME     = "Store 1";  // Nom par défaut (aussi repris si on efface le nom sur la page)
const char* MOT2_NAME     = "Store 2";  // Nom par défaut (aussi repris si on efface le nom sur la page)
const char* MOT1_SIDE     = "Store Gauche"; // Titre du cadre du moteur 1 sur la page (côté, emplacement…) 🚩
const char* MOT2_SIDE     = "Store Droit";  // Titre du cadre du moteur 2 sur la page 🚩
const uint8_t NB_MOTORS = MOTOR2_ENABLED ? 2 : 1;   // Nombre de moteurs réellement utilisés
const size_t MOTOR_NAME_LEN = 32;       // Longueur maximale d'un nom (en octets ; un accent compte pour 2)

// 🚨----------- CARACTÉRISTIQUES DU MOTORÉDUCTEUR (affichage seulement) -----------
// Ne servent qu'à afficher les tours et la vitesse estimée : la course réelle est
// mesurée à la calibration. À adapter si le rapport de réduction est différent.
const long  ENC_PPR     = 11;           // Impulsions de l'encodeur par tour du moteur (fiche technique) 🚩
const long  GEAR_RATIO  = 270;          // Rapport du réducteur (1:270 pour la version 37 tr/min) 🚩
const float MOTOR_RPM   = 37.0;         // Vitesse annoncée à 12 V, en tr/min (estimation sur la page) 🚩
const long  COUNTS_PER_REV = 4 * ENC_PPR * GEAR_RATIO;   // Impulsions par tour du tube (≈ 11 880)

// 🚨----------- RÉGLAGES DE VITESSE (PWM sur 0 à 1023) -----------
const int SPEED_PCT_DEFAULT = 100;      // Vitesse maximale par défaut, en % (réglable sur la page) 🚩
const int SPEED_PCT_MIN     = 40;       // Vitesse maximale la plus basse proposée sur la page (%)
const int SPEED_CAL  = 600;    // Vitesse pendant la calibration (plus lente, pour viser les butées) 🚩
const int SPEED_MIN  = 400;    // Vitesse minimale : doit suffire à faire DÉMARRER le store en montée 🚩
                               // (si « BLOCAGE » apparaît au démarrage, augmenter cette valeur)
const int SPEED_JOG  = 500;    // Vitesse de l'alignement des deux côtés (mode synchronisé)
const int RAMP_STEP  = 40;     // Accélération : augmentation de la vitesse toutes les 10 ms
const int SLOW_ZONE_PCT = 5;   // Ralentissement sur les derniers 5 % de la course avant la cible
const long SLOW_ZONE_MIN = 500;// …et au minimum sur les 500 dernières impulsions (≈ 1/24 de tour du tube)

// ----------- SYNCHRONISATION (deux moteurs sur un même store) -----------
const int  SYNC_GAIN    = 3;     // Correction : 3 points de PWM par impulsion d'écart entre les deux côtés
const long SYNC_MAX_ERR = COUNTS_PER_REV / 8;   // Écart maximal toléré (1/8 de tour) → arrêt « désynchronisés »
const long SYNC_TOL     = 20;                   // À l'arrivée, le suiveur rattrape s'il a plus de 20 impulsions de retard
const long JOG_BIG      = COUNTS_PER_REV / 8;   // Alignement : ▲▲ / ▼▼ = 1/8 de tour
const long JOG_SMALL    = COUNTS_PER_REV / 64;  // Alignement : ▲ / ▼ = petit pas
const long JOG_MAX      = COUNTS_PER_REV;       // Alignement maximal en attente (1 tour)

// ----------- SÉCURITÉS -----------
const uint32_t STALL_MS       = 400;  // Moteur alimenté mais encodeur immobile pendant 400 ms → blocage
const long WRONG_DIR_COUNTS   = 300;  // Recul de 300 impulsions (≈ 1/40 de tour) dans le mauvais sens → sens incohérent
const int  RESTART_WAIT_TICKS = 20;   // Pause freinée de 200 ms avant de repartir juste après un arrêt
const uint32_t RESTART_WAIT_MS = 300; // « Juste après un arrêt » = moins de 300 ms
const bool NFAULT_ENABLED     = true; // false = FUT (nFAULT) des modules non relié à D0 🚩
const int  NFAULT_TICKS       = 3;    // FUT bas pendant 30 ms → défaut d'un driver

// ----------- MODE SECOURS -----------
const uint32_t AP_RETRY_MS = 120000;  // En mode secours : nouvelle tentative vers la box toutes les 2 min
const uint32_t AP_TRY_MS   = 15000;   // Durée d'une tentative

// ----------- TEMPS -----------
const uint32_t CTRL_PERIOD_US = 10000;   // Régulation des moteurs toutes les 10 ms
const uint32_t CTRL_MS        = CTRL_PERIOD_US / 1000;   // La même période en millisecondes
const uint32_t SETTLE_MS      = 300;     // Après un arrêt, attente de la fin du glissement avant sauvegarde
const uint32_t SAVE_EVERY_MS  = 1500;    // Sauvegarde au plus toutes les 1,5 s si la position change

// ----------- PWM -----------
const uint32_t PWM_FREQ = 20000;   // 20 kHz : au-delà de l'audible (pas de sifflement du moteur)
const uint8_t  PWM_BITS = 10;      // Résolution 10 bits : rapport cyclique de 0 à 1023
const int      PWM_MAX  = 1023;    // Valeur maximale du rapport cyclique

// Codes d'erreur d'un moteur
const uint8_t FAULT_NONE  = 0;     // Pas d'erreur
const uint8_t FAULT_STALL = 1;     // Blocage détecté
const uint8_t FAULT_DIR   = 2;     // Sens moteur / encodeur incohérent
const uint8_t FAULT_DRV   = 3;     // Défaut signalé par un DRV8876 (surintensité / surchauffe)
const uint8_t FAULT_SYNC  = 4;     // Synchronisé : écart trop grand entre les deux côtés

// ----------- ÉTAT D'UN MOTEUR -----------
// Chaque moteur possède toutes ses variables : c'est ce qui le rend indépendant.
// En synchronisé, le moteur 1 « mène » (sens, cible, butées, calibration, rampe)
// et le moteur 2 suit sa position. Les variables partagées avec le temporisateur
// ou les interruptions sont « volatile ».
struct Motor {
  uint8_t in1, in2;               // Ses 2 broches de commande
  uint8_t encA, encB;             // Ses 2 broches d'encodeur
  volatile bool motInvert;        // Sens de rotation inversé ? (réglable sur la page)
  volatile bool encInvert;        // Sens de comptage inversé ? (réglable sur la page)
  const char* defName;            // Nom par défaut (MOTx_NAME)
  const char* side;               // Titre du cadre (MOTx_SIDE)
  const char* keyTravel;          // Clés NVS : course totale…
  const char* keyPos;             // …dernière position…
  const char* keyInv;             // …sens…
  const char* keyEnc;             // …sens de comptage…
  const char* keySpd;             // …vitesse (%)…
  const char* keyName;            // …nom…
  const char* keyDist;            // …compteur de distance (impulsions)…
  const char* keyMoves;           // …compteur de mouvements
  uint8_t ch1, ch2;               // Canaux PWM (utilisés seulement avec le paquet ESP32 2.x)
  char name[MOTOR_NAME_LEN + 1] = "";   // Nom affiché sur la page web (réglable)
  volatile int  maxDuty   = PWM_MAX; // Vitesse maximale réglée (PWM)
  volatile int  dir       = 0;    // Sens demandé : +1 = descente, -1 = montée, 0 = arrêt
  volatile long pos       = 0;    // Position en impulsions d'encodeur (0 = tout en haut)
  volatile long travel    = 0;    // Course totale haut→bas en impulsions (0 = non calibré)
  volatile long target    = -1;   // Position cible pour « aller à X % » (-1 = pas de cible)
  volatile bool cal       = false;// Vrai pendant sa calibration (butées désactivées)
  volatile bool stopEvent = false;// Signale au programme principal qu'il vient de s'arrêter
  volatile uint8_t fault  = 0;    // Dernière erreur (FAULT_…)
  volatile uint8_t encState = 0;  // Dernier état des voies A et B (pour la quadrature)
  volatile int  duty      = 0;    // Vitesse actuelle (0 à 1023)
  volatile int  wait      = 0;    // Nombre de périodes de pause freinée avant de démarrer
  volatile long startPos  = 0;    // Position au démarrage (détection de sens incohérent)
  volatile long stallRef  = 0;    // Dernière position vue (détection de blocage)
  volatile uint32_t stallMs = 0;  // Durée depuis laquelle l'encodeur n'a pas bougé
  volatile uint32_t lastHaltMs = 0;   // Instant du dernier arrêt
  // --- Alignement (synchronisé) : un seul moteur tourne, store à l'arrêt ---
  volatile bool jogActive = false;// Alignement en cours
  volatile long jogEnd    = 0;    // Position (encodeur) à atteindre
  volatile long jogBase   = 0;    // Position « logique » à rétablir à la fin (elle ne change pas)
  volatile int  jogDir    = 0;    // Sens de l'alignement en cours (+1 = descente)
  // --- Compteurs bruts (temporisateur) et totaux (boucle principale) ---
  volatile uint32_t distRaw = 0;  // Impulsions parcourues depuis le démarrage de la carte
  volatile uint32_t moveRaw = 0;  // Mouvements lancés depuis le démarrage de la carte
  volatile long cntRef    = 0;    // Dernière position prise en compte par le compteur de distance
  uint32_t distSeen       = 0;    // Valeur de distRaw déjà ajoutée au total
  uint32_t moveSeen       = 0;    // Valeur de moveRaw déjà ajoutée au total
  uint64_t totalDist      = 0;    // Distance cumulée en impulsions (gardée en NVS)
  uint32_t totalMoves     = 0;    // Mouvements cumulés (gardés en NVS)
  uint64_t savedDist      = 0;    // Dernier total de distance écrit en NVS
  // --- Utilisées uniquement par la boucle principale ---
  bool calTop             = false;// Vrai quand sa position haute a été validée
  long lastSaved          = 0;    // Dernière position écrite en NVS (évite les écritures inutiles)
  uint32_t lastSaveMs     = 0;    // Instant de la dernière écriture
  uint32_t stoppedAt      = 0;    // Instant où la boucle a vu l'arrêt
  bool pendingSave        = false;// Sauvegarde à faire une fois le glissement terminé
};

Motor motors[2] = {                                         // Les deux moteurs
  { M1_IN1, M1_IN2, M1_ENC_A, M1_ENC_B, MOT1_INVERT, ENC1_INVERT, MOT1_NAME, MOT1_SIDE,
    "travel1", "pos1", "inv1", "enc1", "spd1", "name1", "dist1", "moves1", 0, 1 },   // Moteur 1
  { M2_IN1, M2_IN2, M2_ENC_A, M2_ENC_B, MOT2_INVERT, ENC2_INVERT, MOT2_NAME, MOT2_SIDE,
    "travel2", "pos2", "inv2", "enc2", "spd2", "name2", "dist2", "moves2", 2, 3 }    // Moteur 2
};                                                          // Fin du tableau

volatile bool syncMode = false;         // Vrai = moteurs synchronisés (un seul store, deux moteurs)
volatile bool otaBusy = false;          // Vrai pendant une mise à jour : commandes des moteurs refusées
bool updateRefused = false;             // Vrai si l'envoi par /update n'a pas le bon mot de passe
volatile bool drvFault = false;         // Vrai tant qu'un driver signale un défaut (FUT bas)
volatile int  drvFaultCount = 0;        // Nombre de périodes consécutives avec FUT bas

bool apMode = false;                    // Vrai en mode point d'accès de secours
bool apTrying = false;                  // Vrai pendant une tentative de retour sur la box
uint32_t apLastTry = 0;                 // Instant de la dernière tentative
uint32_t apTryStart = 0;                // Début de la tentative en cours

// ----------- OBJETS -----------
WebServer server(80);              // Serveur web sur le port 80 (port HTTP standard)
Preferences prefs;                 // Accès à la mémoire non volatile
esp_timer_handle_t ctrlTimer;      // Poignée du temporisateur qui régule les moteurs

const char* PREF_NAMESPACE = "rollerjgb";   // Espace de noms du moteur JGB37 (le même qu'en v6)
const char* KEY_SYNC       = "sync";        // Clé NVS du mode (indépendants / synchronisés)

// ======================================================
// ENCODEURS (interruptions)
// ======================================================
// Table de décodage en quadrature : (ancien état << 2) | nouvel état → -1, 0 ou +1
// DRAM_ATTR : placée en RAM, lisible même pendant une écriture en flash
DRAM_ATTR static const int8_t QDEC[16] = { 0,-1, 1, 0,  1, 0, 0,-1,  -1, 0, 0, 1,  0, 1,-1, 0 };

void IRAM_ATTR encoderISR(void* arg) {                      // Appelée à chaque front sur la voie A ou B
  Motor* m = (Motor*)arg;                                   // Moteur concerné
  uint32_t in = REG_READ(GPIO_IN_REG);                      // Lit d'un coup l'état de toutes les broches
  uint8_t s = (((in >> m->encA) & 1) << 1) | ((in >> m->encB) & 1);   // Nouvel état A et B (0 à 3)
  int8_t d = QDEC[(m->encState << 2) | s];                  // Sens du mouvement d'après le changement
  m->encState = s;                                          // Mémorise l'état pour le prochain front
  m->pos = m->pos + (m->encInvert ? -d : d);                // Met à jour la position
}                                                           // Fin de encoderISR

// ======================================================
// FONCTIONS DES MOTEURS
// ======================================================
void pwmAttach(uint8_t pin, uint8_t ch) {                   // Prépare une broche en sortie PWM
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pin, PWM_FREQ, PWM_BITS);                      // Paquet 3.x : canal choisi automatiquement
#else
  ledcSetup(ch, PWM_FREQ, PWM_BITS);                        // Paquet 2.x : on règle le canal…
  ledcAttachPin(pin, ch);                                   // …puis on y relie la broche
#endif
}                                                           // Fin de pwmAttach

void pwmWrite(uint8_t pin, uint8_t ch, uint32_t duty) {     // Fixe le rapport cyclique d'une broche
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);                                     // Paquet 3.x : par la broche
#else
  ledcWrite(ch, duty);                                      // Paquet 2.x : par le canal
#endif
}                                                           // Fin de pwmWrite

void writeOutputs(Motor& m, uint32_t d1, uint32_t d2) {     // Applique les deux sorties d'un moteur
  if (m.motInvert) { uint32_t t = d1; d1 = d2; d2 = t; }    // Inversion du sens : on échange les deux broches
  pwmWrite(m.in1, m.ch1, d1);                               // Broche IN1
  pwmWrite(m.in2, m.ch2, d2);                               // Broche IN2
}                                                           // Fin de writeOutputs

void brakeMotor(Motor& m) {                                 // Frein : IN1 et IN2 à l'état haut
  writeOutputs(m, PWM_MAX, PWM_MAX);                        // Moteur court-circuité : il résiste à la rotation
}                                                           // Fin de brakeMotor

void driveMotor(Motor& m, int speed) {                      // speed de -1023 (montée) à +1023 (descente)
  // Mode « décroissance lente » du DRV8876 (POE en IN/IN) : une entrée à 1, l'autre en PWM.
  if (speed > 0)      writeOutputs(m, PWM_MAX, PWM_MAX - speed);   // Descente
  else if (speed < 0) writeOutputs(m, PWM_MAX + speed, PWM_MAX);   // Montée
  else                brakeMotor(m);                               // Arrêt freiné
}                                                           // Fin de driveMotor

void setPos(Motor& m, long p) {                             // Change la position par programme
  m.pos = p;                                                // (calibration, alignement, chargement)
  m.cntRef = p;                                             // Le compteur de distance ne compte pas ce saut
}                                                           // Fin de setPos

void endJog(Motor& m) {                                     // Fin (ou abandon) d'un alignement
  if (!m.jogActive) return;
  m.jogActive = false;                                      // Plus d'alignement
  brakeMotor(m);                                            // Moteur freiné
  setPos(m, m.jogBase);                                     // La position logique ne change pas : seul le côté a bougé
  m.lastHaltMs = millis();
  m.stopEvent = true;                                       // La boucle sauvegarde les compteurs
}                                                           // Fin de endJog

void haltOne(Motor& m, uint8_t fault) {     // Arrêt immédiat d'UN moteur (aussi depuis le temporisateur)
  m.dir    = 0;                             // Plus de mouvement
  m.target = -1;                            // Annule une éventuelle cible
  m.duty   = 0;                             // Vitesse nulle
  m.wait   = 0;                             // Annule une pause en cours
  endJog(m);                                // Annule un alignement en cours
  brakeMotor(m);                            // Freinage actif
  m.fault  = fault;                         // Mémorise l'éventuelle erreur
  m.lastHaltMs = millis();                  // Instant de l'arrêt
  m.stopEvent = true;                       // Prévient la boucle principale (affichage + sauvegarde)
}                                           // Fin de haltOne

void haltMotor(Motor& m, uint8_t fault) {   // Arrêt d'un moteur ; en synchronisé, des deux
  if (syncMode) {                           // Un seul store : les deux côtés s'arrêtent ensemble
    for (uint8_t i = 0; i < 2; i++) haltOne(motors[i], (&motors[i] == &m) ? fault : FAULT_NONE);
  } else {
    haltOne(m, fault);
  }
}                                           // Fin de haltMotor

bool inCal(const Motor& m) {                                // Calibration en cours pour ce moteur ?
  return syncMode ? motors[0].cal : m.cal;                  // En synchronisé, c'est celle du moteur 1
}                                                           // Fin de inCal

void prepStart(Motor& m, int dir) {                         // Prépare UN moteur à démarrer dans un sens
  int old = m.dir;                                          // Sens actuel
  m.dir = 0;                                                // Fige le moteur le temps de tout préparer
  m.fault = FAULT_NONE;                                     // Efface l'erreur précédente
  endJog(m);                                                // Un mouvement annule l'alignement
  bool justStopped = (old == 0) && (millis() - m.lastHaltMs < RESTART_WAIT_MS);   // Il glisse peut-être encore
  if ((old != 0 && old != dir) || justStopped) {            // Inversion de sens ou arrêt tout récent :
    brakeMotor(m);                                          //  on freine d'abord…
    m.duty = 0;                                             //  …on repartira de la vitesse minimale…
    m.wait = RESTART_WAIT_TICKS;                            //  …après une courte pause
  } else if (old == 0) {                                    // Départ normal depuis l'arrêt
    m.duty = 0;                                             //  vitesse minimale puis rampe
    m.wait = 0;                                             //  sans pause
  }                                                         // Même sens qu'avant : la vitesse est conservée
  if (old == 0) m.moveRaw = m.moveRaw + 1;                  // Un mouvement de plus (compteurs)
  m.startPos = m.pos;                                       // Point de départ (contrôle du sens)
  m.stallRef = m.pos;                                       // Point de départ (contrôle du blocage)
  m.stallMs  = 0;                                           // Remise à zéro du chronomètre de blocage
}                                                           // Fin de prepStart

void startMove(Motor& m, int dir, long target) {            // Lance un moteur (en synchronisé : les deux)
  bool both = syncMode && (&m == &motors[0]);               // Synchronisé : le moteur 1 entraîne le 2
  prepStart(m, dir);
  if (both) {
    Motor& f = motors[1];
    prepStart(f, dir);
    int w = (m.wait > f.wait) ? m.wait : f.wait;            // Même pause pour les deux côtés
    m.wait = w; f.wait = w;
    f.duty = m.duty;
    f.target = -1;
    f.dir = dir;                                            // Le suiveur est marqué en mouvement
  }
  m.target   = target;                                      // Cible mémorisée AVANT de lancer le moteur
  m.dir      = dir;                                         // Lancement
}                                                           // Fin de startMove

bool checkWrongDir(Motor& m, int dir) {                     // L'encodeur compte-t-il à l'envers ?
  return (m.pos - m.startPos) * dir < -WRONG_DIR_COUNTS;
}                                                           // Fin de checkWrongDir

bool checkStall(Motor& m, int applied) {                    // Encodeur immobile alors que le moteur est alimenté ?
  long pos = m.pos;
  if (pos != m.stallRef || applied < SPEED_MIN) {           // Il bouge, ou on le retient volontairement :
    m.stallRef = pos;                                       //  tout va bien
    m.stallMs  = 0;
    return false;
  }
  uint32_t still = m.stallMs + CTRL_MS;                     // Durée d'immobilité mise à jour
  m.stallMs = still;
  return still >= STALL_MS;                                 // Immobile depuis trop longtemps
}                                                           // Fin de checkStall

// Régulation d'UN moteur (toutes les 10 ms). f = moteur suiveur en synchronisé.
void ctrlMotor(Motor& m, Motor* f = nullptr) {
  int dir = m.dir;                                          // Copie locale du sens
  if (dir == 0) return;                                     // À l'arrêt : le frein est déjà appliqué

  if (m.wait > 0) {                                         // Pause freinée en cours
    int w = m.wait - 1;                                     // Une période de pause de moins
    m.wait = w;
    if (w == 0) {                                           // Fin de la pause : nouveaux points de départ
      m.startPos = m.pos; m.stallRef = m.pos; m.stallMs = 0;
      if (f) { f->wait = 0; f->startPos = f->pos; f->stallRef = f->pos; f->stallMs = 0; }
    }
    return;                                                 // Le moteur reste freiné pendant la pause
  }

  if (checkWrongDir(m, dir)) { haltMotor(m, FAULT_DIR); return; }        // Sens incohérent
  if (f && checkWrongDir(*f, dir)) { haltMotor(*f, FAULT_DIR); return; } // …côté suiveur

  long pos = m.pos;                                         // Copie locale de la position
  bool hasLimit = true;                                     // Y a-t-il un point d'arrêt à viser ?
  long limit = 0;                                           // Position à laquelle s'arrêter
  if (m.target >= 0)  limit = m.target;                     // « Aller à X % »
  else if (!m.cal)    limit = (dir > 0) ? m.travel : 0;     // Butée basse ou haute
  else                hasLimit = false;                     // Calibration : mouvement libre

  int top = m.maxDuty;                                      // Vitesse maximale réglée sur la page
  int want = m.cal ? (SPEED_CAL < top ? SPEED_CAL : top) : top;   // Vitesse souhaitée
  if (want < SPEED_MIN) want = SPEED_MIN;
  if (hasLimit) {
    long dist = (limit - pos) * dir;                        // Distance restante (positive si pas encore arrivé)
    if (dist <= 0) {                                        // Arrivé (ou dépassé) :
      if (f && (pos - f->pos) * dir > SYNC_TOL) {           //  synchronisé : le suiveur en retard finit seul
        brakeMotor(m);                                      //  (le moteur 1 attend, freiné)
        if (checkStall(*f, SPEED_MIN)) { haltMotor(*f, FAULT_STALL); return; }
        driveMotor(*f, dir * SPEED_MIN);
        return;
      }
      haltMotor(m, FAULT_NONE);                             //  arrêt freiné
      return;
    }
    long zone = m.travel * SLOW_ZONE_PCT / 100;             // Longueur de la zone de ralentissement
    if (zone < SLOW_ZONE_MIN) zone = SLOW_ZONE_MIN;         // Pas trop courte
    if (dist < zone)                                        // Dans la zone : vitesse proportionnelle à la distance
      want = SPEED_MIN + (int)((long)(want - SPEED_MIN) * dist / zone);
  }

  int d = m.duty;                                           // Vitesse actuelle
  if (d < SPEED_MIN) d = SPEED_MIN;                         // Démarrage directement à la vitesse minimale
  else d += RAMP_STEP;                                      // Puis accélération progressive
  if (d > want) d = want;                                   // Sans dépasser la vitesse souhaitée
  m.duty = d;                                               // Mémorise

  int d1 = d, d2 = d;                                       // Vitesses appliquées (moteur 1, suiveur)
  if (f) {                                                  // Synchronisé : correction de l'écart
    long err = (pos - f->pos) * dir;                        // > 0 : le suiveur est en retard
    if (err > SYNC_MAX_ERR || err < -SYNC_MAX_ERR) {        // Écart trop grand : on arrête tout
      haltMotor(err > 0 ? *f : m, FAULT_SYNC);              //  (le côté en retard est signalé)
      return;
    }
    long corr = err * SYNC_GAIN;                            // Correction proportionnelle à l'écart
    if (corr > PWM_MAX) corr = PWM_MAX;
    if (corr < -PWM_MAX) corr = -PWM_MAX;
    if (corr > 0) d1 = d - (int)corr;                       // Suiveur en retard : le moteur 1 ralentit
    else          d2 = d + (int)corr;                       // Suiveur en avance : il ralentit
    if (d1 < 0) d1 = 0;
    if (d2 < 0) d2 = 0;
    f->duty = d2;
  }

  if (checkStall(m, d1)) { haltMotor(m, FAULT_STALL); return; }          // Blocage
  if (f && checkStall(*f, d2)) { haltMotor(*f, FAULT_STALL); return; }   // Blocage côté suiveur

  driveMotor(m, dir * d1);                                  // Applique la vitesse dans le bon sens
  if (f) driveMotor(*f, dir * d2);
}                                                           // Fin de ctrlMotor

void ctrlJog(Motor& m) {                                    // Alignement : un seul moteur, lentement
  if (!m.jogActive) return;
  long pos = m.pos;
  int d = m.jogDir;                                         // Sens de l'alignement
  if ((m.jogEnd - pos) * d <= 0) {                          // Arrivé (ou dépassé)
    endJog(m);
    return;
  }
  long moved = pos - m.jogBase;                             // L'encodeur part dans le mauvais sens ?
  if (moved > JOG_MAX + WRONG_DIR_COUNTS || moved < -JOG_MAX - WRONG_DIR_COUNTS) {
    endJog(m);
    m.fault = FAULT_DIR;
    return;
  }
  if (checkStall(m, SPEED_JOG)) {                           // Bloqué pendant l'alignement
    endJog(m);
    m.fault = FAULT_STALL;
    return;
  }
  driveMotor(m, d * SPEED_JOG);                             // Tourne lentement
}                                                           // Fin de ctrlJog

void countDistance(Motor& m) {                              // Compteur de distance (impulsions parcourues)
  long p = m.pos;
  long dlt = p - m.cntRef;
  if (dlt < 0) dlt = -dlt;
  m.cntRef = p;
  m.distRaw = m.distRaw + (uint32_t)dlt;
}                                                           // Fin de countDistance

// Appelée toutes les 10 ms par le temporisateur, même si le Wi-Fi est occupé
void controlTick(void* arg) {
  if (NFAULT_ENABLED) {                                     // Surveillance des drivers
    if (digitalRead(DRV_FAULT) == LOW) {                    // FUT bas : surintensité ou surchauffe
      if (drvFaultCount < NFAULT_TICKS) drvFaultCount = drvFaultCount + 1;   // Filtre les parasites brefs
      if (drvFaultCount >= NFAULT_TICKS && !drvFault) {     // Défaut confirmé (30 ms) :
        drvFault = true;                                    //  mémorisé tant qu'il dure
        for (uint8_t i = 0; i < NB_MOTORS; i++) {           //  les DEUX moteurs sont arrêtés
          if (motors[i].dir != 0 || motors[i].jogActive) haltOne(motors[i], FAULT_DRV);
          motors[i].fault = FAULT_DRV;                      //  défaut affiché même sur un moteur arrêté
        }
      }
    } else {                                                // FUT haut : tout va bien
      drvFaultCount = 0;
      drvFault = false;                                     // Le défaut a disparu (le message reste jusqu'à la prochaine commande)
    }
  }
  if (syncMode && NB_MOTORS == 2) {                         // --- Synchronisés ---
    if (motors[0].dir != 0) ctrlMotor(motors[0], &motors[1]);   // Store en mouvement : les deux ensemble
    else { ctrlJog(motors[0]); ctrlJog(motors[1]); }        // Store à l'arrêt : alignement éventuel
  } else {                                                  // --- Indépendants ---
    for (uint8_t i = 0; i < NB_MOTORS; i++) ctrlMotor(motors[i]);
  }
  for (uint8_t i = 0; i < NB_MOTORS; i++) countDistance(motors[i]);   // Compteurs
}                                                           // Fin de controlTick

int positionPercent(const Motor& m) {                       // Position d'un moteur en %
  if (m.travel <= 0) return 0;                              // Non calibré : 0 %
  long pct = (100L * m.pos + m.travel / 2) / m.travel;      // Calcul arrondi au plus proche
  return constrain(pct, 0, 100);                            // Limité entre 0 et 100 (léger dépassement possible)
}                                                           // Fin de positionPercent

bool canMove(const Motor& m) {                              // Ce moteur a-t-il le droit de bouger ?
  return m.cal || m.travel > 0;                             // Oui si calibré, ou si sa calibration est en cours
}                                                           // Fin de canMove

int speedPct(const Motor& m) {                              // Vitesse maximale réglée, en %
  return (int)((m.maxDuty * 100L + PWM_MAX / 2) / PWM_MAX);
}                                                           // Fin de speedPct

void setSpeedPct(Motor& m, long pct) {                      // Règle la vitesse maximale d'un moteur
  pct = constrain(pct, (long)SPEED_PCT_MIN, 100L);          // Reste dans la plage autorisée
  m.maxDuty = (int)((pct * PWM_MAX + 50) / 100);            // % → PWM
}                                                           // Fin de setSpeedPct

void setName(Motor& m, String s) {                          // Change le nom d'un moteur (texte nettoyé)
  s.trim();                                                 // Retire les espaces au début et à la fin
  String clean;                                             // Nom sans caractères gênants
  for (size_t i = 0; i < s.length(); i++) {                 // Pour chaque octet du texte
    char c = s[i];
    if ((uint8_t)c < 0x20) continue;                        // Caractères de contrôle : refusés
    if (c == '"' || c == '\\' || c == '<' || c == '>' || c == '&') continue;   // Casseraient le JSON ou la page
    clean += c;                                             // Caractère accepté
  }
  if (clean.length() == 0) clean = m.defName;               // Nom vide : retour au nom par défaut
  size_t n = clean.length();                                // Longueur en octets
  if (n > MOTOR_NAME_LEN) {                                 // Trop long : on coupe…
    n = MOTOR_NAME_LEN;
    while (n > 0 && ((uint8_t)clean[n] & 0xC0) == 0x80) n--;   // …sans couper une lettre accentuée en deux
  }
  memcpy(m.name, clean.c_str(), n);                         // Copie du nom
  m.name[n] = '\0';                                         // Fin du texte
}                                                           // Fin de setName

// ----------- COMPTEURS -----------
// Le temporisateur incrémente distRaw et moveRaw ; la boucle principale
// reporte l'écart dans les totaux. Aucune variable n'est écrite des deux côtés.
void accumulateCounters(Motor& m) {                         // Ajoute aux totaux ce qui a été fait depuis la dernière fois
  uint32_t s = m.distRaw;                                   // Lecture du compteur brut de distance
  m.totalDist += (uint32_t)(s - m.distSeen);                // Ajoute l'écart
  m.distSeen = s;                                           // Mémorise la valeur prise en compte
  uint32_t v = m.moveRaw;                                   // Même chose pour les mouvements
  m.totalMoves += v - m.moveSeen;
  m.moveSeen = v;
}                                                           // Fin de accumulateCounters

// ======================================================
// NVS (MÉMOIRE NON VOLATILE)
// ======================================================
void loadSettings() {                                       // Relit tout ce qui est propre à chaque moteur
  prefs.begin(PREF_NAMESPACE, true);                        // Ouverture en lecture seule
  syncMode = (NB_MOTORS == 2) && prefs.getBool(KEY_SYNC, SYNC_DEFAULT);   // Mode enregistré (sinon SYNC_DEFAULT)
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Pour chaque moteur
    Motor& m = motors[i];                                   // Raccourci
    long travel = prefs.getLong(m.keyTravel, 0);            // Sa course (0 = jamais calibré)
    long pos    = prefs.getLong(m.keyPos, 0);               // Sa position
    if (travel < 0) travel = 0;                             // Sécurité : course jamais négative
    if (pos < 0) pos = 0;                                   // Sécurité : position jamais au-dessus du haut
    if (pos > travel) pos = travel;                         // Sécurité : position jamais au-delà du bas
    m.travel    = travel;                                   // Copie dans la variable partagée
    setPos(m, pos);
    m.lastSaved = pos;                                      // Rien à réécrire tant que la position ne change pas
    m.motInvert = prefs.getBool(m.keyInv, m.motInvert);     // Son sens (sinon MOTx_INVERT)
    m.encInvert = prefs.getBool(m.keyEnc, m.encInvert);     // Son sens de comptage (sinon ENCx_INVERT)
    setSpeedPct(m, prefs.getUChar(m.keySpd, SPEED_PCT_DEFAULT));   // Sa vitesse (sinon SPEED_PCT_DEFAULT)
    if (prefs.isKey(m.keyName)) setName(m, prefs.getString(m.keyName));   // Son nom enregistré…
    else                        setName(m, m.defName);      // …sinon le nom par défaut
    m.totalDist  = prefs.getULong64(m.keyDist, 0);          // Ses compteurs
    m.totalMoves = prefs.getULong(m.keyMoves, 0);
    m.savedDist  = m.totalDist;
    Serial.printf("[NVS] %s : course = %ld impulsions, position = %ld, sens %s, comptage %s, vitesse %d %%\n",
                  m.name, travel, pos, m.motInvert ? "inversé" : "normal",
                  m.encInvert ? "inversé" : "normal", speedPct(m));   // Débogage
  }                                                         // Fin de la boucle
  prefs.end();                                              // Fermeture
  Serial.printf("[NVS] Mode : moteurs %s\n", syncMode ? "synchronisés" : "indépendants");   // Débogage
}                                                           // Fin de loadSettings

void savePosition(Motor& m) {                               // Enregistre la position d'un moteur
  long pos = m.pos;                                         // Copie locale (la position peut changer pendant l'écriture)
  m.lastSaveMs = millis();                                  // Instant de la tentative
  if (pos == m.lastSaved) return;                           // Déjà enregistrée : on épargne la flash
  prefs.begin(PREF_NAMESPACE, false);                       // Ouverture en écriture
  prefs.putLong(m.keyPos, pos);                             // Écrit la position
  prefs.end();                                              // Fermeture
  m.lastSaved = pos;                                        // Mémorise ce qui vient d'être écrit
}                                                           // Fin de savePosition

void saveTravel(Motor& m) {                                 // Enregistre la course totale d'un moteur
  prefs.begin(PREF_NAMESPACE, false);                       // Ouverture en écriture
  prefs.putLong(m.keyTravel, m.travel);                     // Écrit la course
  prefs.end();                                              // Fermeture
  Serial.printf("[NVS] %s : course enregistrée = %ld impulsions\n", m.name, (long)m.travel);   // Débogage
}                                                           // Fin de saveTravel

void saveSettings(Motor& m) {                               // Enregistre nom, sens, comptage et vitesse d'un moteur
  prefs.begin(PREF_NAMESPACE, false);                       // Ouverture en écriture
  prefs.putBool(m.keyInv, m.motInvert);                     // Sens
  prefs.putBool(m.keyEnc, m.encInvert);                     // Sens de comptage
  prefs.putUChar(m.keySpd, (uint8_t)speedPct(m));           // Vitesse (%)
  prefs.putString(m.keyName, m.name);                       // Nom
  prefs.end();                                              // Fermeture
  Serial.printf("[NVS] %s : sens %s, comptage %s, vitesse %d %%\n", m.name,
                m.motInvert ? "inversé" : "normal", m.encInvert ? "inversé" : "normal", speedPct(m));   // Débogage
}                                                           // Fin de saveSettings

void saveCounters(Motor& m, bool force = false) {           // Enregistre les compteurs d'un moteur
  accumulateCounters(m);                                    // Totaux à jour
  if (!force && m.totalDist == m.savedDist) return;         // Rien de nouveau : on épargne la flash
  prefs.begin(PREF_NAMESPACE, false);                       // Ouverture en écriture
  prefs.putULong64(m.keyDist, m.totalDist);                 // Distance totale
  prefs.putULong(m.keyMoves, m.totalMoves);                 // Mouvements
  prefs.end();                                              // Fermeture
  m.savedDist = m.totalDist;                                // Mémorise ce qui vient d'être écrit
}                                                           // Fin de saveCounters

void clearCalibration(Motor& m) {                           // Efface la calibration d'un moteur (« non calibré »)
  m.cal    = false;                                         // Abandon d'une calibration en cours
  m.calTop = false;
  m.travel = 0;                                             // Plus de butées valables
  setPos(m, 0);
  saveTravel(m);                                            // Enregistre la course effacée
  savePosition(m);                                          // Enregistre la position remise à zéro
}                                                           // Fin de clearCalibration

// ======================================================
// PAGE WEB (HTML + CSS + JavaScript stockés en mémoire flash)
// @@TITLE@@ est remplacé à l'envoi par PAGE_TITLE.
// Les cadres des moteurs sont construits par le JavaScript à partir de la réponse
// de /api/status (un cadre par moteur, ou un seul pour les deux en synchronisé).
// ======================================================
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>@@TITLE@@</title>
<link rel="icon" type="image/png" href="/favicon.png">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="#1e2430">
<meta name="apple-mobile-web-app-title" content="@@TITLE@@">
<meta name="application-name" content="@@TITLE@@">
<style>
 *{box-sizing:border-box}
 body{font-family:sans-serif;background:#1e2430;color:#eee;margin:0;padding:16px}
 h1{font-size:1.4em;text-align:center}
 h2{font-size:1.15em;margin:0 0 8px}
 .card{background:#2b3240;border-radius:12px;padding:14px;margin-bottom:14px;min-width:0}
 .bar{height:22px;background:#444;border-radius:11px;overflow:hidden;margin-top:4px}
 .fill{height:100%;background:#4caf50;width:0%;transition:width .3s}
 .row{display:flex;gap:8px;margin-top:10px}
 button{flex:1;min-width:0;padding:16px 4px;font-size:1.1em;border:0;border-radius:10px;background:#3d6fb6;color:#fff}
 #motors{display:grid;grid-template-columns:1fr;gap:14px;margin-bottom:14px}
 #motors.two{grid-template-columns:1fr 1fr}
 .frame{border:2px solid #4a6a9a;border-radius:14px;padding:10px;background:#232a37;min-width:0}
 .ftitle{text-align:center;font-weight:bold;color:#8ab4f8;text-transform:uppercase;letter-spacing:.05em;margin:2px 0 10px}
 .frame .card{margin-bottom:0}
 #motors.two button{font-size:1.3em;padding:14px 2px}
 #motors.two .lbl{display:none}
 @media(max-width:600px){#motors.two{gap:8px} #motors.two .frame{padding:6px} #motors.two .card{padding:10px}}
 #regs{display:grid;grid-template-columns:1fr;gap:14px;margin-top:10px}
 #regs.two{grid-template-columns:1fr 1fr}
 #regs .card{background:#353d4d;margin-bottom:0}
 .calcard summary{margin-top:0}
 button:active{filter:brightness(1.3)}
 .stop{background:#c0392b}.cal{background:#7a5bb5;font-size:.95em}.both{background:#2e8b7a}
 input[type=range]{width:100%}
 input[type=text]{width:100%;padding:8px;font-size:1em;border-radius:8px;border:0;margin-top:6px}
 summary{cursor:pointer;margin-top:10px;color:#ccc}
 #msg{color:#ffb347;min-height:1.2em;text-align:center}
 .err{color:#ff6b5b}
 small{color:#aaa}
 a{color:#8ab4f8}
 .ends{display:flex;justify-content:space-between}
 #pop{display:none;position:fixed;inset:0;background:rgba(0,0,0,.65);z-index:10;align-items:center;justify-content:center;padding:16px}
 #pop.on{display:flex}
 #pop .box{background:#2b3240;border-radius:14px;padding:18px;width:100%;max-width:380px;box-shadow:0 8px 30px rgba(0,0,0,.5)}
 #pop h2{margin-bottom:10px}
 #pop p{margin:0 0 12px;color:#ccc;font-size:.95em;line-height:1.4}
 #pop input{width:100%;padding:10px;font-size:1.05em;border-radius:8px;border:0}
 #pop .perr{color:#ff7b6b;min-height:1.2em;margin-top:8px;font-size:.9em}
 .grey{background:#5a6272}
 #pop button{font-size:1.05em}
</style></head><body>
<h1>@@TITLE@@</h1>

<div class="card" id="all" style="display:none">
 <h2>Les deux Stores</h2>
 <div class="row">
  <button class="both" onclick="cmd('up',0)">▲ Monter</button>
  <button class="stop" onclick="cmd('stop',0)">■ Stop</button>
  <button class="both" onclick="cmd('down',0)">▼ Descendre</button>
 </div>
 <div style="margin-top:10px">Aller à : <b id="gv0">50</b> %</div>
 <input type="range" id="goto0" min="0" max="100" value="50" oninput="gv0.textContent=this.value">
 <div class="row"><button class="both" onclick="cmd('goto',0,document.getElementById('goto0').value)">Aller (les deux)</button></div>
</div>

<div id="motors"></div>
<div id="cals"></div>

<div class="card calcard">
 <details><summary>Réglage Moteurs</summary>
  <div id="modebox" style="display:none;margin:10px 0 4px">
   <div>Fonctionnement : <b id="mode">--</b></div>
   <div class="row">
    <button class="cal" onclick="setMode(0)">Indépendants</button>
    <button class="cal" onclick="setMode(1)">Synchronisés</button>
   </div>
   <small>Indépendants : deux stores séparés, chacun sa calibration.
   Synchronisés : un seul store entraîné des deux côtés ; les encodeurs gardent les deux
   côtés à la même position (une seule calibration, vitesse du premier moteur).</small>
  </div>
  <small>« Inverser le sens » : à utiliser si ▼ fait monter le store.
  « Inverser le comptage » : si le store s'arrête sur « sens incohérent ».
  Chaque inversion efface la calibration du moteur concerné
  (en synchronisé : la calibration commune) : il faudra la refaire.</small>
  <div id="regs"></div>
 </details>
</div>

<div id="pop">
 <div class="box">
  <h2 id="ptitle">--</h2>
  <p id="ptext">--</p>
  <input type="password" id="ppw" placeholder="Mot de passe" autocomplete="off"
   onkeydown="if(event.key==='Enter')popOk();if(event.key==='Escape')popClose();">
  <div class="perr" id="perr"></div>
  <div class="row">
   <button class="grey" onclick="popClose()">Annuler</button>
   <button class="cal" onclick="popOk()">Valider</button>
  </div>
 </div>
</div>

<div id="msg"></div>
<p style="text-align:center"><small><a href="/info">Informations</a> &nbsp;·&nbsp;
<a href="/update">Mise à jour du programme</a></small></p>

<script>
let built=false;                                           // Cadres des moteurs déjà construits ?
let bSync=null;                                            // Mode utilisé lors de la construction
let S=null;                                                // Dernier état reçu de la carte
// ---- File d'attente : une seule requête à la fois vers la carte ----
let Q=[];let wake=null;                                    // Requêtes en attente, réveil de la boucle
function ask(u,o){return new Promise(res=>{Q.push({u,o,res});if(wake)wake();});}   // Met une requête en file
async function req(u,o){                                   // Envoie une requête, abandon au bout de 2 s
 let c=new AbortController();let t=setTimeout(()=>c.abort(),2000);
 try{let r=await fetch(u,Object.assign({signal:c.signal,cache:'no-store'},o||{}));
     return {ok:r.ok,t:await r.text()};}
 catch(e){return {ok:false,t:'Carte injoignable'};}
 finally{clearTimeout(t);}
}
async function pump(){                                     // Boucle unique : commandes d'abord, puis état
 for(;;){
  while(Q.length){let q=Q.shift();q.res(await req(q.u,q.o));}   // Commandes en attente
  await refresh();                                         // Puis lecture de l'état
  await new Promise(r=>{wake=r;setTimeout(r,500);});wake=null;   // 500 ms, ou moins si une commande arrive
 }
}
function $(id){return document.getElementById(id);}       // Raccourci
function T(id,v){let e=$(id);if(e)e.textContent=v;}       // Écrit un texte si l'élément existe
function build(s){                                         // Construit les cadres selon le mode
 let h='',c='',r='';                                       // h = cadres des stores, c = calibration, r = réglages
 let two=s.m.length>1;                                     // Deux moteurs ?
 let L=s.sync?[s.m[0]]:s.m;                                // Synchronisés : un seul cadre de commande
 L.forEach((m,i)=>{let k=i+1;let g=m.ready?m.pct:50;
  let n=s.sync?`<span class="nm1">${s.m[0].name}</span> + <span class="nm2">${s.m[1].name}</span>`
              :`<span class="nm${k}">${m.name}</span>`;   // Nom affiché
  let t=s.sync?s.m[0].side+' + '+s.m[1].side+' — synchronisés':(two?m.side:'Moteur');   // Titre du cadre
  h+=`
<div class="frame">
 <div class="ftitle">${t}</div>
 <div class="card">
  <h2 style="text-align:center">${n}</h2>
  <div style="text-align:center">Position : <b id="pct${k}">--</b> %</div>
  <div style="text-align:center"><small>0 % = haut, 100 % = bas</small></div>
  <div class="bar"><div class="fill" id="fill${k}"></div></div>
  <div style="margin-top:6px">État : <b id="etat${k}">--</b></div>
  <div class="row">
   <button onclick="cmd('up',${k})" title="Monter">▲<span class="lbl"> Monter</span></button>
   <button class="stop" onclick="cmd('stop',${k})" title="Stop">■<span class="lbl"> Stop</span></button>
   <button onclick="cmd('down',${k})" title="Descendre">▼<span class="lbl"> Descendre</span></button>
  </div>
  <div style="margin-top:10px">Aller à : <b id="gv${k}">${g}</b> %</div>
  <input type="range" id="goto${k}" min="0" max="100" value="${g}" oninput="$('gv${k}').textContent=this.value">
  <div class="row"><button onclick="cmd('goto',${k},$('goto${k}').value)">Aller</button></div>
 </div>
</div>`;c+=`
<div class="card calcard">
 <details><summary>Calibration — ${n}${s.sync?' (commune)':''}</summary>
  <small>1. Démarrer, puis appui bref sur ▼ : si le store monte → Réglage Moteurs, « Inverser le sens » ;
  s'il s'arrête sur « sens incohérent » → « Inverser le comptage » (puis recliquer sur Démarrer).
  &nbsp;2. Monter tout en haut, Stop, « Valider HAUT »
  &nbsp;3. Descendre tout en bas, Stop, « Valider BAS »</small>
  <div class="row">
   <button class="cal" onclick="cmd('calstart',${k})">Démarrer</button>
   <button class="cal" onclick="cmd('caltop',${k})">Valider HAUT</button>
   <button class="cal" onclick="cmd('calbot',${k})">Valider BAS</button>
  </div>
  <div style="margin-top:8px"><small>Position : <span id="step${k}">--</span> — course : <span id="travel${k}">--</span> impulsions</small></div>
 </details>
</div>`;});
 if(s.sync){let B=Math.round(s.cpr/8),P=Math.round(s.cpr/64);c+=`
<div class="card calcard">
 <details><summary>Alignement des deux côtés</summary>
  <small>Store à l'arrêt : fait tourner un seul moteur pour remettre le store droit,
  sans changer la position affichée. ▲▲ / ▼▼ = 1/8 de tour, ▲ / ▼ = petit pas.
  À faire de préférence avant la calibration.</small>
  ${s.m.map((m,i)=>{let k=i+1;return `
  <div style="margin-top:10px">${m.side} — <span class="nm${k}">${m.name}</span></div>
  <div class="row">
   <button class="cal" onclick="jog(${k},-${B})">▲▲</button>
   <button class="cal" onclick="jog(${k},-${P})">▲</button>
   <button class="cal" onclick="jog(${k},${P})">▼</button>
   <button class="cal" onclick="jog(${k},${B})">▼▼</button>
  </div>`;}).join('')}
 </details>
</div>`;}
 s.m.forEach((m,i)=>{let k=i+1;let n=`<span class="nm${k}">${m.name}</span>`;
  let v=(s.sync&&k>1)?`
 <div style="margin-top:12px"><small>Vitesse : celle de <span class="nm1">${s.m[0].name}</span>
 (les deux moteurs tournent ensemble)</small></div>`:`
 <div style="margin-top:12px">Vitesse maximale${s.sync?' (les deux moteurs)':''} : <b id="sp${k}">--</b> %</div>
 <div><small>environ <span id="rpm${k}">--</span> tr/min</small></div>
 <input type="range" id="spd${k}" min="${s.spmin}" max="100" step="5" value="100"
  oninput="showSpd(${k})" onchange="setSpd(${k})">
 <div class="ends"><small>Lent, plus doux</small><small>Rapide</small></div>`;
  r+=`
<div class="card">
 <h2>${n}</h2>${two?`<small>${m.side}</small>`:''}
 <div style="margin-top:6px">Nom :</div>
 <input type="text" id="name${k}" maxlength="32" value="${m.name}">
 <div class="row"><button class="cal" onclick="ren(${k})">Renommer</button></div>
 <div style="margin-top:12px">Sens : <b id="inv${k}">--</b></div>
 <div class="row"><button class="cal" onclick="inv(${k})">Inverser le sens</button></div>
 <div style="margin-top:12px">Comptage de l'encodeur : <b id="enc${k}">--</b></div>
 <div class="row"><button class="cal" onclick="enc(${k})">Inverser le comptage</button></div>${v}
</div>`;});
 $('motors').innerHTML=h;                                  // Cadres des stores
 $('cals').innerHTML=c;                                    // Calibration (+ alignement en synchronisé)
 $('regs').innerHTML=r;                                    // Réglages
 $('all').style.display=(two&&!s.sync)?'block':'none';     // Carte « les deux » : indépendants seulement
 $('motors').classList.toggle('two',two&&!s.sync);         // Cadres gauche et droit côte à côte
 $('regs').classList.toggle('two',two);                    // Réglages sur 2 colonnes
 $('modebox').style.display=two?'block':'none';            // Choix du mode : seulement avec 2 moteurs
 built=true;bSync=s.sync;
}
function showSpd(k){                                       // Affiche la vitesse choisie sur le curseur
 let p=$('spd'+k).value;
 T('sp'+k,p);
 T('rpm'+k,(S.rpm*p/100).toFixed(0));                     // Estimation (dépend de la charge)
}
function setSpd(k){send('/api/set?m='+k+'&spd='+$('spd'+k).value);}   // Curseur relâché : envoi à la carte
function clr(m){return S.sync?'La calibration commune sera effacée et devra être refaite.'
                             :'Sa calibration sera effacée et devra être refaite.';}
function inv(k){                                           // Inversion du sens d'un moteur
 let m=S.m[k-1];
 if(confirm('Inverser le sens de '+m.name+' ?\n'+clr(m)))
  send('/api/set?m='+k+'&inv='+(m.inv?0:1));
}
function enc(k){                                           // Inversion du comptage de l'encodeur
 let m=S.m[k-1];
 if(confirm('Inverser le comptage de l\'encodeur de '+m.name+' ?\n'+clr(m)))
  send('/api/set?m='+k+'&enc='+(m.enc?0:1));
}
function ren(k){                                           // Nouveau nom d'un moteur
 send('/api/set?m='+k+'&name='+encodeURIComponent($('name'+k).value));
}
let popMode=0;                                             // Mode demandé dans la fenêtre
function setMode(v){                                       // Clic sur Indépendants (0) / Synchronisés (1)
 if(!S)return;
 if((v==1)===S.sync){msg('Déjà dans ce mode');return;}    // Déjà dans ce mode
 popMode=v;
 $('ptitle').textContent=v?'Passer en moteurs synchronisés':'Repasser en moteurs indépendants';
 $('ptext').textContent=v?'Les deux moteurs tourneront ensemble. La calibration sera effacée et devra être refaite une fois pour les deux. Mot de passe : celui de la mise à jour du programme.'
                         :'Chaque moteur garde la calibration actuelle. Mot de passe : celui de la mise à jour du programme.';
 $('ppw').value='';$('perr').textContent='';               // Fenêtre vierge à chaque ouverture
 $('pop').classList.add('on');                             // Affiche la fenêtre
 $('ppw').focus();                                         // Curseur dans le champ mot de passe
}
function popClose(){$('pop').classList.remove('on');$('ppw').value='';}   // Ferme sans rien changer
async function popOk(){                                    // Bouton « Valider » (ou touche Entrée)
 let pw=$('ppw').value;
 if(!pw){$('perr').textContent='Saisissez le mot de passe';$('ppw').focus();return;}
 // POST : le mot de passe n'apparaît pas dans l'adresse
 let r=await ask('/api/set',{method:'POST',body:new URLSearchParams({sync:popMode,pw:pw})});
 if(r.ok){popClose();msg(r.t);}                            // Mode changé : fenêtre fermée, confirmation en bas
 else{$('perr').textContent=r.t;$('ppw').value='';$('ppw').focus();}   // Mot de passe faux ou carte injoignable
}
function jog(k,p){cmd('jog',k,p);}                         // Alignement d'un seul côté
async function send(u){                                    // Envoie une adresse de commande à l'ESP32
 let r=await ask(u);                                       // Mise en file (l'état est relu juste après)
 msg(r.t);                                                 // Message éventuel (vide si tout va bien)
}
function cmd(a,m,p){send('/api/cmd?a='+a+'&m='+m+(p!==undefined?'&p='+p:''));}   // m = 0 : les deux Stores
function msg(t){$('msg').textContent=t;}                   // Affiche un message en bas de page
const FT=['',' — BLOCAGE détecté',' — sens incohérent (Réglage Moteurs)',' — DÉFAUT DRIVER (surintensité / surchauffe)',' — DÉSYNCHRONISÉS (voir Alignement)'];
async function refresh(){                                  // Lit l'état de la carte
 try{
  let r=await req('/api/status');                          // Récupère l'état (2 s maximum)
  if(!r.ok){msg('Carte injoignable');return;}              // Pas de réponse : message en bas de page
  let s=JSON.parse(r.t);                                   // État au format JSON
  S=s;                                                     // Mémorise l'état (utilisé par les réglages)
  if(!built||s.sync!==bSync)build(s);                      // Première réponse ou changement de mode : (re)construit
  if($('msg').textContent==='Carte injoignable')msg('');   // Carte de nouveau joignable : message effacé
  T('mode',s.sync?'moteurs synchronisés':'moteurs indépendants');   // Mode actuel
  s.m.forEach((m,i)=>{let k=i+1;                           // Réglages de chaque moteur
   document.querySelectorAll('.nm'+k).forEach(e=>e.textContent=m.name);   // Nom (après un renommage)
   T('inv'+k,m.inv?'inversé':'normal');                    // Sens du moteur
   T('enc'+k,m.enc?'inversé':'normal');                    // Sens de comptage
   let sp=$('spd'+k);                                      // Curseur de vitesse (absent pour le moteur 2 en synchronisé)
   if(sp&&document.activeElement!==sp){sp.value=m.spd;showSpd(k);}   // Pas pendant que l'utilisateur le déplace
  });
  let jg=s.sync&&(s.m[0].jog||s.m[1].jog);                 // Alignement en cours ?
  (s.sync?[s.m[0]]:s.m).forEach((m,i)=>{let k=i+1;         // Cadres de commande
   T('pct'+k,m.ready?m.pct:'--');                          // Pourcentage
   $('fill'+k).style.width=(m.ready?m.pct:0)+'%';          // Barre de progression
   T('travel'+k,m.travel);                                 // Course enregistrée
   T('step'+k,m.step);                                     // Position brute (utile en calibration)
   let f=m.fault,fs='';                                    // Erreur à afficher
   if(s.sync&&!f&&s.m[1].fault){f=s.m[1].fault;fs=' ('+s.m[1].side+')';}   // Erreur côté suiveur
   else if(s.sync&&f)fs=' ('+m.side+')';
   let e=m.dir>0?'Descente':m.dir<0?'Montée':'Arrêt';      // Sens de rotation en texte
   let bad=false;
   if(s.ota)e='Mise à jour en cours';                      // Mise à jour du programme
   else if(jg)e='Alignement en cours';                     // Un seul moteur tourne
   else if(f){e+=FT[f]+fs+(f==3&&s.drv?' en cours':'');bad=true;}   // Erreur
   else if(m.cal)e+=' — calibration en cours'+(m.top?' (haut validé)':'');   // Calibration
   else if(!m.ready)e+=' — non calibré';                   // Jamais calibré
   T('etat'+k,e);                                          // Affichage de l'état
   $('etat'+k).className=bad?'err':'';                     // Rouge en cas d'erreur
  });
 }catch(e){msg('Carte injoignable');}                      // Réponse illisible
}
pump();                                                    // Boucle unique : commandes + état toutes les 500 ms
</script>
</body></html>
)rawliteral";

// Page d'informations (lecture seule, sauf la remise à zéro des compteurs)
const char INFO_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Informations — @@TITLE@@</title>
<link rel="icon" type="image/png" href="/favicon.png">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<style>
 *{box-sizing:border-box}
 body{font-family:sans-serif;background:#1e2430;color:#eee;margin:0;padding:16px}
 h1{font-size:1.4em;text-align:center}
 h2{font-size:1.15em;margin:0 0 8px}
 .card{background:#2b3240;border-radius:12px;padding:14px;margin-bottom:14px;min-width:0}
 .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px}
 .grid .card{margin-bottom:0}
 table{width:100%;border-collapse:collapse}
 td{padding:6px 4px;border-bottom:1px solid #3a4252;vertical-align:top}
 td:first-child{color:#aaa;width:50%}
 .row{display:flex;gap:8px;margin-top:10px}
 button{flex:1;padding:12px 4px;font-size:1em;border:0;border-radius:10px;background:#c0392b;color:#fff}
 small{color:#aaa}
 a{color:#8ab4f8}
</style></head><body>
<h1>Informations — @@TITLE@@</h1>
<div class="grid">
 <div class="card"><h2>Programme</h2><table id="prog"></table></div>
 <div class="card"><h2>Réseau</h2><table id="net"></table></div>
 <div class="card"><h2>Mémoire</h2><table id="mem"></table></div>
</div>
<div class="grid" id="mot" style="margin-top:14px"></div>
<p style="text-align:center"><a href="/">← Retour aux stores</a></p>
<script>
function $(id){return document.getElementById(id);}       // Raccourci
function row(a,b){return '<tr><td>'+a+'</td><td>'+b+'</td></tr>';}   // Une ligne de tableau
function dur(s){                                           // Secondes → « 2 j 3 h 12 min 5 s »
 let j=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
 return (j?j+' j ':'')+h+' h '+m+' min '+(s%60)+' s';
}
function ko(o){return (o/1024).toFixed(1)+' ko';}          // Octets → kilo-octets
let built=false;                                           // Cartes des compteurs déjà construites ?
async function refresh(){                                  // Lit les informations de la carte
 try{
  let s=await (await fetch('/api/info',{cache:'no-store'})).json();
  $('prog').innerHTML=row('Version',s.ver)+row('Compilé le',s.build)
   +row('En marche depuis',dur(s.up))+row('Dernier démarrage',s.reset)
   +(s.m.length>1?row('Moteurs',s.sync?'synchronisés':'indépendants'):'')
   +row('Motoréducteur','1:'+s.ratio+' — '+s.cpr+' impulsions par tour du tube')
   +row('Drivers',s.drv?'<b style="color:#ff6b5b">défaut en cours (FUT bas)</b>':'OK')
   +row('Processeur',s.chip+' à '+s.mhz+' MHz');
  $('net').innerHTML=row('Connexion',s.ota?'Mise à jour en cours':
    s.ap?'Point d\'accès de secours (box recherchée toutes les 2 min)':'OK — connecté à la box')
   +row('Réseau',s.ssid)+row('Adresse IP',s.ip)+row('Nom','http://'+s.host+'.local')
   +(s.ap?'':row('Signal Wi-Fi',s.rssi+' dBm<br><small>bon au-dessus de -67, faible en dessous de -75</small>'))+row('Adresse MAC',s.mac);
  $('mem').innerHTML=row('Mémoire libre',ko(s.heap))+row('Minimum depuis le démarrage',ko(s.minheap))
   +row('Taille du programme',ko(s.sketch))+row('Place pour une mise à jour',ko(s.free));
  if(!built){                                              // Cartes des compteurs : une par moteur
   let h='';s.m.forEach((m,i)=>{let k=i+1;h+=`
<div class="card"><h2>Compteurs — ${m.name}</h2><table id="cnt${k}"></table>
 <div class="row"><button onclick="rst(${k})">Remettre à zéro</button></div></div>`;});
   $('mot').innerHTML=h;built=true;
  }
  s.m.forEach((m,i)=>{let k=i+1;                           // Valeurs des compteurs
   $('cnt'+k).innerHTML=row('Mouvements',m.moves)
    +row('Tours du tube',m.turns)
    +row('Cycles complets (montée + descente)',m.travel>0?m.cycles:'-- (non calibré)');
  });
 }catch(e){$('net').innerHTML=row('Connexion','<b>Hors ligne</b>');}   // Carte injoignable
}
async function rst(k){                                     // Remise à zéro des compteurs d'un moteur
 if(!confirm('Remettre à zéro les compteurs de ce moteur ?'))return;
 try{await fetch('/api/info?reset='+k,{cache:'no-store'});}catch(e){}
 refresh();
}
setInterval(refresh,2000);                                 // Rafraîchit toutes les 2 s
refresh();                                                 // Premier affichage immédiat
</script>
</body></html>
)rawliteral";

// Page de mise à jour par le navigateur (protégée par mot de passe)
const char UPDATE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mise à jour — @@TITLE@@</title>
<link rel="icon" type="image/png" href="/favicon.png"><link rel="apple-touch-icon" href="/apple-touch-icon.png">
<style>
 body{font-family:sans-serif;background:#1e2430;color:#eee;margin:0;padding:16px;max-width:420px;margin:auto}
 h1{font-size:1.4em;text-align:center}
 .card{background:#2b3240;border-radius:12px;padding:14px;margin-bottom:14px}
 .bar{height:22px;background:#444;border-radius:11px;overflow:hidden;margin:12px 0}
 .fill{height:100%;background:#4caf50;width:0%}
 button{width:100%;padding:16px 0;font-size:1.1em;border:0;border-radius:10px;background:#3d6fb6;color:#fff}
 #msg{color:#ffb347;min-height:1.2em;text-align:center;margin-top:10px}
 small{color:#aaa} a{color:#8ab4f8}
</style></head><body>
<h1>Mise à jour du programme</h1>
<div class="card">
 <small>Arduino IDE : Croquis &gt; Exporter les binaires compilés, puis choisir le fichier
 <b>.ino.bin</b> du croquis (pas les fichiers « merged » ni « bootloader »).
 Les moteurs sont arrêtés pendant la mise à jour.</small><br><br>
 <input type="password" id="pw" placeholder="Mot de passe OTA" style="width:100%;padding:8px;margin-bottom:10px;box-sizing:border-box"><br>
 <input type="file" id="f" accept=".bin">
 <div class="bar"><div class="fill" id="fill"></div></div>
 <button onclick="go()">Envoyer</button>
 <div id="msg"></div>
</div>
<p style="text-align:center"><a href="/">← Retour aux stores</a></p>
<script>
function msg(t){document.getElementById('msg').textContent=t;}   // Affiche un message
function go(){                                             // Envoie le fichier .bin à la carte
 let f=document.getElementById('f').files[0];              // Fichier choisi
 if(!f){msg('Choisissez un fichier .bin');return;}         // Aucun fichier
 let pw=document.getElementById('pw').value;               // Mot de passe saisi
 if(!pw){msg('Saisissez le mot de passe OTA');return;}     // Mot de passe vide
 msg('Vérification du mot de passe…');
 fetch('/update/check?pw='+encodeURIComponent(pw),{cache:'no-store'})   // Vérifie le mot de passe AVANT d'envoyer le fichier
  .then(r=>{if(r.status==200)upload(f,pw);else msg('Échec : mot de passe incorrect');})
  .catch(()=>msg('Carte injoignable'));
}
function upload(f,pw){                                     // Envoi du fichier (mot de passe déjà accepté)
 let fd=new FormData();fd.append('firmware',f);            // Prépare l'envoi
 let x=new XMLHttpRequest();x.open('POST','/update?pw='+encodeURIComponent(pw));   // Requête d'envoi avec le mot de passe
 x.upload.onprogress=e=>{if(e.lengthComputable)document.getElementById('fill').style.width=(100*e.loaded/e.total)+'%';};   // Progression
 x.onload=()=>{                                            // Réponse de la carte
  if(x.status==200){msg('Terminé, redémarrage… retour automatique dans 10 s');setTimeout(()=>location.href='/',10000);}
  else msg('Échec : '+x.responseText);                     // Erreur signalée par la carte
 };
 x.onerror=()=>msg('Erreur de transfert');                 // Coupure pendant l'envoi
 x.send(fd);msg('Envoi en cours…');                        // Lancement
}
</script>
</body></html>
)rawliteral";

// ======================================================
// MISE À JOUR (OTA)
// ======================================================
void prepareUpdate() {                                      // À appeler juste avant d'écrire un nouveau programme
  otaBusy = true;                                           // Bloque les commandes des moteurs
  for (uint8_t i = 0; i < NB_MOTORS; i++) haltOne(motors[i], FAULT_NONE);   // Arrêt freiné des moteurs
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Pour chaque moteur
    Motor& m = motors[i];                                   // Raccourci
    if (!inCal(m)) savePosition(m);                         // Hors calibration : position conservée pour le redémarrage
    saveCounters(m);                                        // Les compteurs aussi
  }
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Calibration inachevée : abandonnée (position non sauvée)
    motors[i].cal = false;
    motors[i].calTop = false;
  }
  esp_timer_stop(ctrlTimer);                                // Suspend la régulation : tout le processeur pour l'OTA
}                                                           // Fin de prepareUpdate

void cancelUpdate() {                                       // Mise à jour échouée : retour au fonctionnement normal
  otaBusy = false;                                          // Les moteurs redeviennent commandables
  esp_timer_start_periodic(ctrlTimer, CTRL_PERIOD_US);      // Relance la régulation des moteurs
}                                                           // Fin de cancelUpdate

void startArduinoOTA() {                                    // Mise à jour depuis l'Arduino IDE (port réseau)
  ArduinoOTA.setHostname(HOSTNAME);                         // Nom affiché dans Outils > Port
  ArduinoOTA.setPassword(OTA_PASS);                         // Mot de passe demandé par l'IDE
  ArduinoOTA.setPort(OTA_PORT);                             // Port réseau
  ArduinoOTA.setMdnsEnabled(false);                         // mDNS déjà démarré dans startWiFi (évite un double démarrage)
  ArduinoOTA.onStart([]() {                                 // Début de la mise à jour
    prepareUpdate();                                        // Moteurs arrêtés, positions sauvées
    Serial.println("[OTA] Début de la mise à jour");        // Débogage
  });
  ArduinoOTA.onEnd([]() {                                   // Fin de la mise à jour
    Serial.println("\n[OTA] Terminée, redémarrage");        // Débogage
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {   // Progression (appelée à chaque paquet)
    static int lastTen = -1;                                // Dernière dizaine de % affichée
    int pct = done * 100 / total;                           // Pourcentage reçu
    if (pct / 10 != lastTen) {                              // Affichage seulement tous les 10 %
      lastTen = pct / 10;                                   // Mémorise la dizaine
      Serial.printf("[OTA] %d %%\n", pct);                  // Pourcentage sur le port série
    }                                                       // Fin du test
  });
  ArduinoOTA.onError([](ota_error_t err) {                  // Erreur
    Serial.printf("\n[OTA] Erreur %u\n", err);              // Code d'erreur
    cancelUpdate();                                         // Les moteurs redeviennent commandables
  });
  ArduinoOTA.begin();                                       // Démarre l'écoute
  Serial.println("[OTA] Prêt (Arduino IDE)");               // Débogage
}                                                           // Fin de startArduinoOTA

void sendPage(const char* tpl) {                            // Envoie une page avec le titre de cette carte
  String html(tpl);                                         // Copie de la page en mémoire vive
  html.replace("@@TITLE@@", PAGE_TITLE);                    // Titre (PAGE_TITLE)
  server.send(200, "text/html", html);                      // Code 200 = OK, contenu HTML
}                                                           // Fin de sendPage

void handleUpdatePage() {                                   // GET /update : affiche la page d'envoi
  sendPage(UPDATE_HTML);                                    // Envoie la page (mot de passe vérifié à l'envoi)
}                                                           // Fin de handleUpdatePage

void handleUpdateCheck() {                                  // GET /update/check?pw=… : vérification préalable
  if (server.arg("pw") == OTA_PASS) server.send(200, "text/plain", "OK");   // Bon mot de passe
  else {
    delay(1000);                                            // Ralentit les essais de mots de passe en série
    server.send(401, "text/plain", "mot de passe incorrect");   // 401 = non autorisé
  }
}                                                           // Fin de handleUpdateCheck

void handleUpdateUpload() {                                 // Réception du fichier .bin, morceau par morceau
  HTTPUpload& up = server.upload();                         // Morceau en cours
  if (up.status == UPLOAD_FILE_START) {                     // --- Début de l'envoi ---
    updateRefused = (server.arg("pw") != OTA_PASS);         // Vérifie le mot de passe saisi sur la page
    if (updateRefused) return;                              // Refusé : on ignore tout le fichier
    Serial.printf("[OTA-WEB] Fichier : %s\n", up.filename.c_str());   // Débogage
    prepareUpdate();                                        // Moteurs arrêtés, positions sauvées
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);   // Prépare la zone flash libre
  }
  else if (up.status == UPLOAD_FILE_WRITE) {                // --- Morceau de données ---
    if (updateRefused) return;                              // Refusé
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);   // Écrit en flash
  }
  else if (up.status == UPLOAD_FILE_END) {                  // --- Fin de l'envoi ---
    if (updateRefused) return;                              // Refusé
    if (Update.end(true)) Serial.printf("[OTA-WEB] Reçu : %u octets\n", up.totalSize);   // Programme validé
    else Update.printError(Serial);                         // Fichier invalide ou incomplet
  }
  else if (up.status == UPLOAD_FILE_ABORTED) {              // --- Envoi interrompu ---
    Update.abort();                                         // Annule la mise à jour
    cancelUpdate();                                         // Les moteurs redeviennent commandables
    Serial.println("[OTA-WEB] Envoi interrompu");           // Débogage
  }
}                                                           // Fin de handleUpdateUpload

void handleUpdateDone() {                                   // POST /update terminé : réponse au navigateur
  if (updateRefused) {                                      // Mauvais mot de passe
    server.send(401, "text/plain", "mot de passe incorrect");   // 401 = non autorisé (la page ajoute « Échec : »)
    return;                                                 // Fin
  }
  bool ok = !Update.hasError();                             // La mise à jour a-t-elle réussi ?
  server.sendHeader("Connection", "close");                 // Ferme la connexion après la réponse
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "fichier invalide ou incomplet");   // Réponse
  if (ok) {                                                 // Succès
    delay(500);                                             // Laisse partir la réponse
    ESP.restart();                                          // Redémarre sur le nouveau programme
  } else if (otaBusy) {                                     // Échec après le début de l'écriture
    cancelUpdate();                                         // Les moteurs redeviennent commandables
  }
}                                                           // Fin de handleUpdateDone

// ======================================================
// GESTION DES REQUÊTES WEB
// ======================================================
void handleRoot() {                                         // Adresse « / » : envoie la page
  sendPage(PAGE_HTML);
}                                                           // Fin de handleRoot

void handleFavicon() {                                      // Icône de l'onglet et de l'écran d'accueil (favicon.h)
  server.sendHeader("Cache-Control", "max-age=604800");     // Gardée 7 jours par le navigateur
  server.send_P(200, "image/png", (const char*)favicon_png, favicon_png_len);   // Envoi du PNG depuis la flash
}                                                           // Fin de handleFavicon

void handleManifest() {                                     // « /manifest.json » : nom et icône du raccourci
  String j = "{\"name\":\"";
  j += PAGE_TITLE;
  j += "\",\"short_name\":\"";
  j += PAGE_TITLE;
  j += "\",\"start_url\":\"/\",\"display\":\"standalone\",\"background_color\":\"#1e2430\",\"theme_color\":\"#1e2430\"";
  j += ",\"icons\":[{\"src\":\"/favicon.png\",\"sizes\":\"180x180\",\"type\":\"image/png\"}]}";
  server.sendHeader("Cache-Control", "max-age=86400");      // Le navigateur le garde un jour
  server.send(200, "application/manifest+json", j);
}                                                           // Fin de handleManifest

void handleStatus() {                                       // Adresse « /api/status » : état en JSON
  char json[1400];                                          // Tampon pour le texte JSON
  int n = snprintf(json, sizeof(json),                      // Début du JSON : informations générales
    "{\"ota\":%s,\"ap\":%s,\"sync\":%s,\"drv\":%s,\"rssi\":%d,\"cpr\":%ld,\"rpm\":%.1f,\"spmin\":%d,\"m\":[",
    otaBusy  ? "true" : "false",                            // Mise à jour en cours ?
    apMode   ? "true" : "false",                            // Point d'accès de secours ?
    syncMode ? "true" : "false",                            // Moteurs synchronisés ?
    drvFault ? "true" : "false",                            // Défaut d'un driver en cours ?
    apMode ? 0 : (int)WiFi.RSSI(),                          // Force du signal de la box (0 en point d'accès)
    (long)COUNTS_PER_REV, (double)MOTOR_RPM, SPEED_PCT_MIN);
  for (uint8_t i = 0; i < NB_MOTORS && n < (int)sizeof(json); i++) {   // Un bloc par moteur
    Motor& m = motors[i];                                   // Raccourci
    n += snprintf(json + n, sizeof(json) - n,
      "%s{\"name\":\"%s\",\"side\":\"%s\",\"pct\":%d,\"step\":%ld,\"travel\":%ld,\"dir\":%d,\"fault\":%d,"
      "\"cal\":%s,\"top\":%s,\"ready\":%s,\"inv\":%s,\"enc\":%s,\"spd\":%d,\"jog\":%s}",
      i ? "," : "",                                         // Virgule entre les moteurs
      m.name, m.side, positionPercent(m), (long)m.pos, (long)m.travel, (int)m.dir, (int)m.fault,
      m.cal    ? "true" : "false",                          // Calibration en cours ?
      m.calTop ? "true" : "false",                          // Haut validé ?
      m.travel > 0 ? "true" : "false",                      // Moteur calibré ?
      m.motInvert ? "true" : "false",                       // Sens inversé ?
      m.encInvert ? "true" : "false",                       // Comptage inversé ?
      speedPct(m),                                          // Vitesse maximale (%)
      m.jogActive ? "true" : "false");                      // Alignement en cours ?
  }                                                         // Fin de la boucle
  if (n < (int)sizeof(json)) snprintf(json + n, sizeof(json) - n, "]}");   // Fin du JSON
  server.send(200, "application/json", json);               // Envoi de la réponse
}                                                           // Fin de handleStatus

// Exécute une commande sur UN moteur (en synchronisé : le moteur 1, qui
// entraîne le 2). Renvoie nullptr si tout va bien, sinon le texte de l'erreur.
const char* doCommand(Motor& m, const String& a, int pct) {
  if (a == "stop") {                                        // --- STOP ---
    haltMotor(m, FAULT_NONE);                               // Arrêt freiné immédiat
    return nullptr;                                         // OK
  }
  if (drvFault && (a == "up" || a == "down" || a == "goto"))   // Pont en H encore en défaut :
    return "défaut driver en cours (surintensité ou surchauffe), patientez";   //  pas de mouvement
  if (a == "up" || a == "down") {                           // --- MONTER / DESCENDRE ---
    if (!canMove(m)) return "non calibré : lancez d'abord la calibration";
    startMove(m, (a == "up") ? -1 : +1, -1);                // Mouvement libre jusqu'à Stop ou butée
    return nullptr;                                         // OK
  }
  if (a == "goto") {                                        // --- ALLER À X % ---
    if (m.cal || m.travel <= 0) return "non calibré ou calibration en cours";
    long target = (m.travel * pct + 50) / 100;              // Conversion en impulsions
    long pos = m.pos;                                       // Position actuelle
    if (labs(target - pos) <= 1) { haltMotor(m, FAULT_NONE); return nullptr; }   // Déjà à la bonne position
    startMove(m, (target > pos) ? +1 : -1, target);         // Sens nécessaire pour l'atteindre
    return nullptr;                                         // OK
  }
  bool both = syncMode && (&m == &motors[0]);               // Synchronisé : calibration commune
  Motor& f = motors[1];
  if (a == "calstart") {                                    // --- DÉBUT DE CALIBRATION ---
    haltMotor(m, FAULT_NONE);                               // Arrêt par sécurité
    m.cal    = true;                                        // Butées désactivées : mouvement libre
    m.calTop = false;                                       // Le haut n'est pas encore validé
    Serial.printf("[CAL] %s : DÉMARRÉE\n", m.name);         // Débogage
    return nullptr;                                         // OK
  }
  if (a == "caltop") {                                      // --- VALIDER LA POSITION HAUTE ---
    if (!m.cal)      return "cliquez d'abord sur « Démarrer »";
    if (m.dir != 0)  return "arrêtez le moteur avant de valider";
    setPos(m, 0);                                           // La position actuelle devient le haut
    if (both) setPos(f, 0);                                 // Synchronisé : le même haut pour les deux côtés
    m.calTop = true;                                        // Haut validé
    Serial.printf("[CAL] %s : HAUT validé\n", m.name);      // Débogage
    return nullptr;                                         // OK
  }
  if (a == "calbot") {                                      // --- VALIDER LA POSITION BASSE ---
    if (!m.cal || !m.calTop) return "validez d'abord la position haute";
    if (m.dir != 0)  return "arrêtez le moteur avant de valider";
    if (m.pos <= 0)  return "descendez le store avant de valider le bas";
    m.travel = m.pos;                                       // La position actuelle devient la course totale
    m.cal    = false;                                       // Fin de la calibration, butées réactivées
    m.calTop = false;                                       // Remise à zéro de l'étape
    saveTravel(m);                                          // Enregistre la course
    savePosition(m);                                        // Enregistre la position
    if (both) { f.travel = m.travel; saveTravel(f); savePosition(f); }   // Synchronisé : même course
    Serial.printf("[CAL] %s : TERMINÉE\n", m.name);         // Débogage
    return nullptr;                                         // OK
  }
  return "commande inconnue";                               // --- COMMANDE INCONNUE ---
}                                                           // Fin de doCommand

// Alignement (synchronisé) : un seul moteur tourne, store à l'arrêt.
const char* doJog(int mot, long counts) {
  if (!syncMode)            return "l'alignement ne sert qu'en mode synchronisé";
  if (mot < 1 || mot > 2)   return "choisissez un moteur";
  if (motors[0].dir != 0)   return "arrêtez le store avant d'aligner";
  if (drvFault)             return "défaut driver en cours, patientez";
  if (counts == 0)          return "déplacement nul";
  Motor& m = motors[mot - 1];                               // Moteur à aligner
  m.fault = FAULT_NONE;
  long end;
  if (m.jogActive) end = m.jogEnd + counts;                 // Ajouté à ce qui reste à faire
  else {
    m.jogBase  = m.pos;                                     // Position logique à rétablir à la fin
    m.stallRef = m.pos;
    m.stallMs  = 0;
    end = m.pos + counts;
  }
  if (end > m.jogBase + JOG_MAX) end = m.jogBase + JOG_MAX; // Limite (1 tour)
  if (end < m.jogBase - JOG_MAX) end = m.jogBase - JOG_MAX;
  long here = m.pos;
  if (end == here) return nullptr;                          // Rien à faire
  m.jogEnd = end;
  m.jogDir = (end > here) ? 1 : -1;                         // Sens à prendre depuis la position actuelle
  m.moveRaw = m.moveRaw + 1;                                // Un mouvement de plus (compteurs)
  m.jogActive = true;                                       // Le temporisateur s'en charge
  Serial.printf("[ALIGN] %s : %ld impulsions\n", m.name, end - m.jogBase);   // Débogage
  return nullptr;                                           // OK
}                                                           // Fin de doJog

void handleCmd() {                                          // Adresse « /api/cmd?a=...&m=... »
  String a = server.arg("a");                               // Nom de la commande
  int mot  = server.arg("m").toInt();                       // Moteur visé : 1, 2, ou 0 = les deux
  Serial.printf("[WEB] Commande : %s, moteur %d\n", a.c_str(), mot);   // Débogage

  if (otaBusy) { server.send(409, "text/plain", "Mise à jour en cours, patientez"); return; }
  if (mot < 0 || mot > NB_MOTORS) { server.send(409, "text/plain", "Moteur inconnu"); return; }

  if (a == "jog") {                                         // --- ALIGNEMENT (synchronisé) ---
    const char* e = doJog(mot, server.arg("p").toInt());
    server.send(e ? 409 : 200, "text/plain", e ? e : "");
    return;
  }

  int pct = constrain(server.arg("p").toInt(), 0, 100);     // Pourcentage (pour « goto »)
  if (syncMode) mot = 1;                                    // Synchronisés : tout passe par le moteur 1 (il entraîne le 2)
  if (a.startsWith("cal") && mot == 0) {                    // La calibration se fait moteur par moteur
    server.send(409, "text/plain", "Choisissez un moteur pour la calibration");
    return;
  }

  int first = (mot == 0) ? 0 : mot - 1;                     // Premier moteur concerné
  int last  = (mot == 0) ? NB_MOTORS - 1 : mot - 1;         // Dernier moteur concerné
  String errors;                                            // Messages d'erreur accumulés
  int okCount = 0;                                          // Nombre de moteurs ayant accepté la commande
  for (int i = first; i <= last; i++) {                     // Pour chaque moteur concerné
    const char* e = doCommand(motors[i], a, pct);           // Exécute la commande
    if (e == nullptr) { okCount++; continue; }              // Acceptée
    if (errors.length()) errors += " / ";                   // Séparateur entre deux erreurs
    errors += motors[i].name;                               // Nom du moteur concerné
    errors += " : ";
    errors += e;                                            // Raison du refus
  }                                                         // Fin de la boucle

  if (okCount == 0)              server.send(409, "text/plain", errors);   // Tout refusé (409 = conflit)
  else                           server.send(200, "text/plain", errors);   // Accepté (message vide, ou refus partiel)
}                                                           // Fin de handleCmd

String setSyncMode(bool s) {                                // Passage indépendants ↔ synchronisés
  if (NB_MOTORS < 2) return "Un seul moteur : pas de fonctionnement synchronisé";
  if (s == syncMode) return "";                             // Déjà dans ce mode
  haltOne(motors[0], FAULT_NONE);                           // Arrêt des deux moteurs par sécurité
  haltOne(motors[1], FAULT_NONE);
  for (uint8_t i = 0; i < 2; i++) {                         // Abandon d'une calibration en cours
    motors[i].cal = false;
    motors[i].calTop = false;
  }
  if (s) {                                                  // Vers synchronisés : les deux côtés doivent
    clearCalibration(motors[0]);                            //   repartir d'une calibration commune
    clearCalibration(motors[1]);
  } else {                                                  // Vers indépendants : chacun garde la calibration commune
    savePosition(motors[0]);
    savePosition(motors[1]);
  }
  syncMode = s;                                             // Nouveau mode (le temporisateur le voit au top suivant)
  prefs.begin(PREF_NAMESPACE, false);                       // Enregistrement du mode
  prefs.putBool(KEY_SYNC, s);
  prefs.end();
  Serial.printf("[MODE] Moteurs %s\n", s ? "synchronisés" : "indépendants");   // Débogage
  return s ? "Moteurs synchronisés : faites la calibration (une seule pour les deux)"
           : "Moteurs indépendants : chacun garde la calibration actuelle";
}                                                           // Fin de setSyncMode

void handleSet() {                                          // Adresse « /api/set?... » : réglages
  if (otaBusy) { server.send(409, "text/plain", "Mise à jour en cours, patientez"); return; }

  if (server.hasArg("sync")) {                              // --- MODE (indépendants / synchronisés) ---
    if (server.arg("pw") != OTA_PASS) {                     // Protégé par le mot de passe OTA (SECRET_OTA_PASS)
      delay(1000);                                          // Ralentit les essais de mots de passe en série
      Serial.println("[MODE] Mot de passe incorrect : mode inchangé");   // Débogage
      server.send(401, "text/plain", "Mot de passe incorrect : mode inchangé");   // 401 = non autorisé
      return;
    }
    server.send(200, "text/plain", setSyncMode(server.arg("sync").toInt() != 0));
    return;
  }

  int mot = server.arg("m").toInt();                        // Moteur visé : 1 ou 2
  if (mot < 1 || mot > NB_MOTORS) { server.send(409, "text/plain", "Choisissez un moteur"); return; }
  Motor& m = motors[mot - 1];                               // Raccourci
  String reply;                                             // Message renvoyé à la page

  if (server.hasArg("spd")) {                               // --- VITESSE MAXIMALE ---
    setSpeedPct(m, server.arg("spd").toInt());              // Appliquée tout de suite, même en mouvement
  }

  if (server.hasArg("name")) {                              // --- NOM ---
    setName(m, server.arg("name"));                         // Nom nettoyé (vide = nom par défaut)
    reply = String("Nom enregistré : ") + m.name;
  }

  bool wantInv = server.hasArg("inv") && ((server.arg("inv").toInt() != 0) != m.motInvert);   // Sens à changer ?
  bool wantEnc = server.hasArg("enc") && ((server.arg("enc").toInt() != 0) != m.encInvert);   // Comptage à changer ?
  if (wantInv || wantEnc) {
    haltMotor(m, FAULT_NONE);                               // Arrêt par sécurité (les deux en synchronisé)
    if (wantInv) {                                          // « Inverser le sens » : le store tourne dans l'autre sens
      m.motInvert = !m.motInvert;                           //  moteur inversé…
      m.encInvert = !m.encInvert;                           //  …et comptage aussi, pour rester cohérent
    }
    if (wantEnc) m.encInvert = !m.encInvert;                // « Inverser le comptage » seul
    if (syncMode) {                                         // Synchronisés : la calibration commune n'est plus valable
      clearCalibration(motors[0]);
      clearCalibration(motors[1]);
      reply = String(m.name) + (wantInv ? " : sens inversé" : " : comptage inversé")
            + ", refaites la calibration (commune aux deux moteurs)";
    } else {                                                // Indépendants : seulement ce moteur
      clearCalibration(m);
      reply = String(m.name) + (wantInv ? " : sens inversé" : " : comptage inversé")
            + ", refaites sa calibration";
    }
    m.fault = FAULT_NONE;                                   // L'ancien « sens incohérent » n'a plus lieu d'être
  }

  saveSettings(m);                                          // Enregistre nom, sens, comptage et vitesse en NVS
  server.send(200, "text/plain", reply);                    // Réponse (vide si rien à signaler)
}                                                           // Fin de handleSet

const char* resetReasonText() {                             // Cause du dernier démarrage, en clair
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "mise sous tension";
    case ESP_RST_SW:        return "redémarrage par le programme (mise à jour…)";
    case ESP_RST_PANIC:     return "plantage du programme";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "chien de garde (programme bloqué)";
    case ESP_RST_BROWNOUT:  return "baisse de tension de l'alimentation";
    case ESP_RST_DEEPSLEEP: return "sortie de veille";
    case ESP_RST_EXT:       return "broche de reset";
    default:                return "autre";
  }
}                                                           // Fin de resetReasonText

void handleInfoPage() {                                     // Adresse « /info » : page d'informations
  sendPage(INFO_HTML);
}                                                           // Fin de handleInfoPage

void handleInfo() {                                         // Adresse « /api/info » : informations en JSON
  int r = server.arg("reset").toInt();                      // Remise à zéro demandée ? (1 ou 2)
  if (r >= 1 && r <= NB_MOTORS) {                           // Oui, pour un moteur valide
    Motor& m = motors[r - 1];
    accumulateCounters(m);                                  // Prend en compte ce qui vient d'être fait…
    m.totalDist  = 0;                                       // …puis repart de zéro
    m.totalMoves = 0;
    saveCounters(m, true);                                  // Enregistre les compteurs remis à zéro
    Serial.printf("[INFO] %s : compteurs remis à zéro\n", m.name);   // Débogage
  }

  String j = "{\"ver\":\"";                                 // Construction du JSON
  j += FW_VERSION;
  j += "\",\"build\":\"" __DATE__ " " __TIME__ "\"";        // Date de compilation
  j += ",\"up\":";    j += (unsigned long)(esp_timer_get_time() / 1000000ULL);   // Secondes depuis le démarrage
  j += ",\"reset\":\""; j += resetReasonText(); j += "\"";
  j += ",\"sync\":";  j += syncMode ? "true" : "false";     // Moteurs synchronisés ?
  j += ",\"drv\":";   j += drvFault ? "true" : "false";     // Défaut d'un driver en cours ?
  j += ",\"chip\":\""; j += ESP.getChipModel(); j += "\"";
  j += ",\"mhz\":";   j += ESP.getCpuFreqMHz();
  j += ",\"ap\":";    j += apMode ? "true" : "false";
  j += ",\"ota\":";   j += otaBusy ? "true" : "false";     // Mise à jour en cours ?
  j += ",\"ssid\":\""; j += apMode ? String(AP_SSID) : WiFi.SSID(); j += "\"";
  j += ",\"ip\":\"";  j += apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); j += "\"";
  j += ",\"host\":\""; j += HOSTNAME; j += "\"";
  j += ",\"rssi\":";  j += (int)WiFi.RSSI();                // (int) : sinon ajouté comme un caractère
  j += ",\"mac\":\""; j += WiFi.macAddress(); j += "\"";
  j += ",\"heap\":";  j += ESP.getFreeHeap();               // Mémoire vive libre
  j += ",\"minheap\":"; j += ESP.getMinFreeHeap();          // Plus bas niveau atteint
  j += ",\"sketch\":"; j += ESP.getSketchSize();            // Taille du programme
  j += ",\"free\":";  j += ESP.getFreeSketchSpace();        // Place disponible pour une mise à jour
  j += ",\"cpr\":";   j += (long)COUNTS_PER_REV;            // Impulsions par tour du tube
  j += ",\"ratio\":"; j += (long)GEAR_RATIO;                // Rapport du réducteur
  j += ",\"m\":[";
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Un bloc par moteur
    Motor& m = motors[i];
    accumulateCounters(m);                                  // Compteurs à jour (même en mouvement)
    double turns = (double)m.totalDist / (double)COUNTS_PER_REV;   // Tours du tube
    unsigned long cycles = (m.travel > 0) ? (unsigned long)(m.totalDist / (2ULL * (uint64_t)m.travel)) : 0;
    if (i) j += ",";
    j += "{\"name\":\""; j += m.name; j += "\"";
    j += ",\"moves\":";  j += (unsigned long)m.totalMoves;
    j += ",\"turns\":";  j += String(turns, 1);
    j += ",\"cycles\":"; j += cycles;
    j += ",\"travel\":"; j += (long)m.travel;
    j += "}";
  }
  j += "]}";
  server.send(200, "application/json", j);                  // Envoi de la réponse
}                                                           // Fin de handleInfo

// ======================================================
// WIFI
// ======================================================
void startMdns() {                                          // Active le nom « store.local » et les annonces
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);                     // Annonce le serveur web
    MDNS.enableArduino(OTA_PORT, true);                     // Annonce l'OTA : la carte apparaît dans Outils > Port
    Serial.printf("[WIFI] Page web : http://%s.local\n", HOSTNAME);   // Débogage
  }                                                         // Fin du test mDNS
}                                                           // Fin de startMdns

void startWiFi() {                                          // Connexion à la box, sinon point d'accès
  WiFi.mode(WIFI_STA);                                      // Mode client
  WiFi.setSleep(false);                                     // Pas de veille Wi-Fi : OTA et page web plus rapides
  WiFi.setHostname(HOSTNAME);                               // Nom de la carte sur le réseau
  WiFi.begin(WIFI_SSID, WIFI_PASS);                         // Lance la connexion
  WiFi.setTxPower(WIFI_POWER_8_5dBm);                       // Puissance réduite APRÈS begin : évite la saturation du XIAO C3
  Serial.print("[WIFI] Connexion");                         // Débogage

  unsigned long t0 = millis();                              // Instant de départ
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {   // Attente 15 s maximum
    delay(500);                                             // Pause de 500 ms
    Serial.print(".");                                      // Point de progression
  }                                                         // Fin de l'attente

  if (WiFi.status() == WL_CONNECTED) {                      // Connexion réussie
    Serial.printf("\n[WIFI] CONNECTÉ, IP : %s, signal : %d dBm\n",   // Affiche l'IP et la force du signal
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {                                                  // Échec : point d'accès de secours
    Serial.println("\n[WIFI] ÉCHEC → point d'accès de secours");   // Débogage
    WiFi.mode(WIFI_AP);                                     // Mode point d'accès
    WiFi.setSleep(false);                                   // Pas de veille Wi-Fi
    WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);               // Fixe l'adresse : IP = passerelle = 192.168.5.1
    WiFi.softAP(AP_SSID, AP_PASS);                          // Crée le réseau de secours
    WiFi.setTxPower(WIFI_POWER_8_5dBm);                     // Puissance réduite (même raison qu'en mode box)
    Serial.printf("[WIFI] Réseau « %s », IP : %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());   // Infos
    apMode = true;                                          // Mode secours actif
    apLastTry = millis();                                   // Prochaine tentative vers la box dans 2 min
  }                                                         // Fin du test

  startMdns();                                              // Nom « store.local »
}                                                           // Fin de startWiFi

// En mode secours, retente la box toutes les 2 min, sans bloquer la boucle.
// La tentative n'a lieu que si personne n'est connecté au point d'accès :
// on ne coupe jamais quelqu'un qui est en train de commander les stores.
void wifiRecovery() {
  if (!apMode || otaBusy) return;                           // Uniquement en mode secours, hors mise à jour
  uint32_t now = millis();                                  // Instant présent
  if (!apTrying) {                                          // --- Pas de tentative en cours ---
    if (now - apLastTry < AP_RETRY_MS) return;              // Pas encore l'heure
    apLastTry = now;                                        // Le délai repart de maintenant
    if (WiFi.softAPgetStationNum() > 0) {                   // Quelqu'un utilise le point d'accès :
      Serial.println("[WIFI] Point d'accès utilisé : tentative vers la box reportée");
      return;                                               //  on réessaiera plus tard
    }
    Serial.println("[WIFI] Nouvelle tentative de connexion à la box");
    WiFi.mode(WIFI_AP_STA);                                 // Point d'accès + client en même temps
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);                       // Lance la connexion (non bloquant)
    WiFi.setTxPower(WIFI_POWER_8_5dBm);                     // Puissance réduite
    apTrying = true;
    apTryStart = now;
  } else if (WiFi.status() == WL_CONNECTED) {               // --- Box retrouvée ---
    WiFi.softAPdisconnect(true);                            // Coupe le point d'accès : client seul
    apMode = false;
    apTrying = false;
    Serial.printf("[WIFI] Retour sur la box, IP : %s, signal : %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    MDNS.end();                                             // Relance le mDNS sur la nouvelle interface
    startMdns();
  } else if (now - apTryStart >= AP_TRY_MS) {               // --- Échec de la tentative ---
    WiFi.disconnect();                                      // Abandonne la connexion…
    WiFi.mode(WIFI_AP);                                     // …et revient au point d'accès seul
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    apTrying = false;
    apLastTry = now;                                        // Prochaine tentative dans 2 min
    Serial.println("[WIFI] Box toujours introuvable, maintien du point d'accès");
  }
}                                                           // Fin de wifiRecovery

// ======================================================
// INITIALISATION / BOUCLE PRINCIPALE
// ======================================================
void setup() {                                              // Exécutée une fois au démarrage
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Pour chaque moteur utilisé
    Motor& m = motors[i];                                   // Raccourci
    pwmAttach(m.in1, m.ch1);                                // Broche IN1 en PWM
    pwmAttach(m.in2, m.ch2);                                // Broche IN2 en PWM
    brakeMotor(m);                                          // Moteur freiné dès le démarrage
  }                                                         // Fin de la boucle

  Serial.begin(115200);                                     // Port série (USB) à 115200 bauds
#if ARDUINO_USB_CDC_ON_BOOT                                 // Port série par l'USB intégré du C3
  Serial.setTxTimeoutMs(0);                                 // Ne bloque jamais si aucun moniteur série ne lit
#endif                                                      // Fin du test
  delay(500);                                               // Laisse le temps à l'USB de démarrer
  Serial.printf("\n=== XIAO ESP32-C3 Stores enrouleurs (web) v%s - JGB37-520 12 V + DRV8876 ===\n", FW_VERSION);   // Bannière
  Serial.printf("[INFO] Démarrage : %s\n", resetReasonText());   // Cause du démarrage

  loadSettings();                                           // Recharge mode, calibrations, positions, réglages, compteurs

  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Encodeurs de chaque moteur
    Motor& m = motors[i];                                   // Raccourci
    pinMode(m.encA, INPUT_PULLUP);                          // Voie A en entrée (rappel au +3,3 V)
    pinMode(m.encB, INPUT_PULLUP);                          // Voie B en entrée (rappel au +3,3 V)
    m.encState = (digitalRead(m.encA) << 1) | digitalRead(m.encB);   // État de départ
    attachInterruptArg(m.encA, encoderISR, &m, CHANGE);     // Interruption sur chaque front de A
    attachInterruptArg(m.encB, encoderISR, &m, CHANGE);     // Interruption sur chaque front de B
  }                                                         // Fin de la boucle

  if (NFAULT_ENABLED) pinMode(DRV_FAULT, INPUT_PULLUP);     // FUT : collecteur ouvert, haut = pas de défaut

  const esp_timer_create_args_t args = {                    // Paramètres du temporisateur
    .callback = &controlTick,                               // Fonction appelée à chaque échéance
    .arg = nullptr,                                         // Pas d'argument
    .dispatch_method = ESP_TIMER_TASK,                      // Exécutée dans la tâche dédiée aux temporisateurs
    .name = "motors",                                       // Nom (pour le débogage)
    .skip_unhandled_events = true                           // Après un retard (écriture flash), pas de rafale
  };                                                        // Fin des paramètres
  esp_timer_create(&args, &ctrlTimer);                      // Crée le temporisateur
  esp_timer_start_periodic(ctrlTimer, CTRL_PERIOD_US);      // Le lance : un appel toutes les 10 ms

  startWiFi();                                              // Connexion Wi-Fi
  startArduinoOTA();                                        // Mise à jour depuis l'Arduino IDE

  server.on("/", handleRoot);                               // Page principale
  server.on("/favicon.ico", handleFavicon);                 // Icône demandée par défaut par les navigateurs
  server.on("/favicon.png", handleFavicon);                 // Icône de l'onglet
  server.on("/apple-touch-icon.png", handleFavicon);        // Icône « Sur l'écran d'accueil » (iPhone / Android)
  server.on("/icon.png", handleFavicon);                    // Adresse de la v8.0 (raccourcis déjà créés)
  server.on("/manifest.json", handleManifest);              // Nom et icône du raccourci
  server.on("/api/status", handleStatus);                   // État au format JSON
  server.on("/api/cmd", handleCmd);                         // Commandes des moteurs
  server.on("/api/set", handleSet);                         // Réglages (mode, nom, sens, comptage, vitesse)
  server.on("/info", handleInfoPage);                       // Page d'informations
  server.on("/api/info", handleInfo);                       // Informations au format JSON
  server.on("/update", HTTP_GET, handleUpdatePage);         // Page de mise à jour
  server.on("/update/check", HTTP_GET, handleUpdateCheck);  // Vérification du mot de passe avant l'envoi
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);   // Réception du programme
  server.begin();                                           // Démarre le serveur web
  Serial.println("[WEB] Serveur démarré");                  // Débogage
}                                                           // Fin de setup

void loop() {                                               // Exécutée en boucle
  server.handleClient();                                    // Traite les requêtes des navigateurs
  ArduinoOTA.handle();                                      // Surveille une mise à jour venant de l'IDE
  wifiRecovery();                                           // Mode secours : retour automatique sur la box

  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Chaque moteur est surveillé séparément
    Motor& m = motors[i];                                   // Raccourci

    if (m.stopEvent) {                                      // Ce moteur vient de s'arrêter
      m.stopEvent = false;                                  // Acquitte l'événement
      m.stoppedAt = millis();                               // Instant de l'arrêt
      m.pendingSave = true;                                 // Sauvegarde une fois le glissement terminé
      const char* why = (m.fault == FAULT_STALL) ? " — BLOCAGE"
                      : (m.fault == FAULT_DIR)   ? " — SENS INCOHÉRENT"
                      : (m.fault == FAULT_DRV)   ? " — DÉFAUT DRIVER"
                      : (m.fault == FAULT_SYNC)  ? " — DÉSYNCHRONISÉS" : "";   // Raison éventuelle
      Serial.printf("[MOTEUR] %s : ARRÊT à %ld impulsions (%d %%)%s\n",
                    m.name, (long)m.pos, positionPercent(m), why);   // Débogage
    }                                                       // Fin du test

    bool stillMoving = (m.dir != 0) || m.jogActive;         // En mouvement (ou en alignement)
    if (m.pendingSave && !stillMoving && millis() - m.stoppedAt >= SETTLE_MS) {   // Arrêt stabilisé
      m.pendingSave = false;                                // Fait
      if (!inCal(m)) savePosition(m);                       // Position définitive (sauf en calibration)
      saveCounters(m);                                      // Compteurs (une écriture par mouvement)
    } else if (!inCal(m) && !m.jogActive && m.pos != m.lastSaved
               && millis() - m.lastSaveMs >= SAVE_EVERY_MS) {
      savePosition(m);                                      // En mouvement (ou store qui glisse) : sauvegarde périodique
    }                                                       // Fin du test
  }                                                         // Fin de la boucle
}                                                           // Fin de loop
