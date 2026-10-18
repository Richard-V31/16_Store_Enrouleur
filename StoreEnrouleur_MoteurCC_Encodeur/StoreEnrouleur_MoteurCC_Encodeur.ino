// ======================================================
// XIAO ESP32-C3 : Commande de stores enrouleurs par page web
// Version autonome, sans Blynk — DEUX MOTEURS INDÉPENDANTS
// v2 : sauvegarde de la position pendant le mouvement + mise à jour OTA
// v3 : deuxième moteur sur 4 GPIO séparés + alignement des deux côtés
// v3.1 : OTA réparée, page /update, force du signal Wi-Fi
// v4 : les deux moteurs deviennent indépendants
// v4.1 / v4.2 : mise en page (2 colonnes, boutons ▲ ■ ▼)
// v5 : MOTEURS À COURANT CONTINU AVEC ENCODEUR MAGNÉTIQUE
//      (motoréducteur GM12 / N20 6 V) pilotés par un DRV8833.
//      - la position est comptée par l'encodeur (quadrature x4, interruptions)
//      - vitesse réglée en PWM 20 kHz (inaudible), rampe au démarrage,
//        ralentissement à l'approche de la cible ou d'une butée
//      - freinage actif à l'arrêt (le moteur résiste à la rotation, sans consommer)
//      - détection de blocage : moteur alimenté mais encodeur immobile → arrêt
//      - détection d'incohérence de sens moteur / encodeur → arrêt
//      - inversion de sens rapprochée : courte pause freinée avant de repartir
//      ⚠ La position est maintenant en impulsions d'encodeur et non plus en pas :
//        les anciennes calibrations ne sont PAS reprises. Recalibrer chaque moteur.
// v5.2 : icône de la page web (favicon.h) : onglet et écran d'accueil du téléphone
// v5.1 : - page /update : mot de passe vérifié AVANT l'envoi du fichier
//        - message d'échec de mise à jour sans « Échec : » en double
//        - mode secours : nouvelle tentative vers la box toutes les 5 min
//          (seulement si personne n'est connecté au point d'accès)
//        - lecture de nFAULT du DRV8833 sur D0 : surintensité / surchauffe
//          → arrêt des deux moteurs et message sur la page
//        (la calibration et la position de la v5 sont conservées)
// ------------------------------------------------------
// Carte Arduino IDE : « XIAO_ESP32C3 » (paquet esp32 d'Espressif, 2.x ou 3.x)
// Pour voir le moniteur série : Outils > USB CDC On Boot > Enabled
//   (obligatoire ici : D6/D7 servent au moteur 2, le moniteur passe par l'USB)
// Pour l'OTA : Outils > Partition Scheme > un schéma avec OTA (celui par défaut convient)
// Page web : http://store.local  ou l'adresse IP affichée sur le port série
// Mise à jour par le navigateur : http://store.local/update
// Si le Wi-Fi de la box est introuvable : réseau du point d'accès de secours
// (SECRET_AP_SSID / SECRET_AP_PASS), puis http://192.168.5.1
// ------------------------------------------------------
// Câblage :
//   DRV8833 (une seule carte pour les deux moteurs)
//     VM  → +5 V externe (1 A minimum)      GND → masse commune avec le XIAO
//     nSLEEP (ou EEP/STBY selon la carte) → +3,3 V si la carte ne le relie pas déjà
//     nFAULT → D0 (garder la résistance de rappel de 10 kΩ vers +3,3 V)
//     Moteur 1 : D2 → AIN1, D3 → AIN2, sorties AOUT1/AOUT2 → fils M1/M2 du moteur 1
//     Moteur 2 : D6 → BIN1, D7 → BIN2, sorties BOUT1/BOUT2 → fils M1/M2 du moteur 2
//   Encodeurs (alimentés en 3,3 V par le XIAO, JAMAIS en 5 V : broches non tolérantes)
//     Moteur 1 : C1 (voie A) → D4, C2 (voie B) → D5, VCC → 3V3, GND → GND
//     Moteur 2 : C1 (voie A) → D1, C2 (voie B) → D10, VCC → 3V3, GND → GND
// Broches restées libres : D8, D9 (broches de démarrage : jamais d'encodeur
// dessus, un niveau bas au démarrage bloquerait la carte en mode téléchargement).
// D0 est aussi une broche de démarrage : nFAULT y convient car il est tenu à
// l'état haut par sa résistance de rappel (sauf défaut du DRV8833).
// ------------------------------------------------------
// PREMIÈRE MISE EN SERVICE (pour chaque moteur, page web > Calibration) :
//   1. « Démarrer », puis un appui bref sur ▼ Descendre :
//      - le store MONTE au lieu de descendre → mettre MOTx_INVERT à true
//      - le store descend mais s'arrête avec « sens incohérent » → mettre ENCx_INVERT à true
//   2. Monter tout en haut, Stop, « Valider HAUT »
//   3. Descendre tout en bas, Stop, « Valider BAS »
// ------------------------------------------------------
// Commandes web : /api/cmd?a=<action>&m=<moteur>[&p=<pourcentage>]
//   m = 1 ou 2 : un seul moteur ; m = 0 : les deux moteurs
//   a = up, down, stop, goto (avec p = 0 à 100),
//       calstart, caltop, calbot (m = 1 ou 2 obligatoire)
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
#include "esp_arduino_version.h"   // Version du paquet ESP32 (la fonction PWM a changé en 3.x)
#include "soc/soc.h"       // Macro REG_READ (lecture directe d'un registre)
#include "soc/gpio_reg.h"  // Registre d'entrée des GPIO (lecture ultra-rapide des encodeurs)

#ifndef SECRET_OTA_PASS                     // Si le mot de passe OTA n'est pas dans arduino_secrets.h…
#define SECRET_OTA_PASS "store-ota"         // …mot de passe par défaut (À CHANGER dans arduino_secrets.h)
#endif                                      // Fin du test

// ----------- WIFI -----------
char WIFI_SSID[] = SECRET_SSID;              // Nom du réseau Wi-Fi de la box
char WIFI_PASS[] = SECRET_PASS;             // Mot de passe du Wi-Fi
char AP_SSID[] = SECRET_AP_SSID;            // Nom du point d'accès de secours créé par la carte
char AP_PASS[]  = SECRET_AP_PASS;           // Mot de passe du point d'accès (8 caractères minimum)
const char* HOSTNAME  = "store";            // Nom réseau : la page sera sur http://store.local
const IPAddress AP_IP(192, 168, 5, 1);       // Adresse IP de la carte en mode point d'accès local
const IPAddress AP_MASK(255, 255, 255, 0);  // Masque de sous-réseau du point d'accès

// ----------- OTA -----------
const char OTA_PASS[] = SECRET_OTA_PASS;    // Mot de passe OTA (Arduino IDE et page /update)
const uint16_t OTA_PORT = 3232;             // Port réseau de l'OTA Arduino (valeur standard)

// ----------- BROCHES (XIAO ESP32-C3) -----------
const uint8_t M1_IN1 = D2;    // GPIO4  → AIN1 du DRV8833
const uint8_t M1_IN2 = D3;    // GPIO5  → AIN2 du DRV8833
const uint8_t M1_ENC_A = D4;  // GPIO6  ← voie A (C1) de l'encodeur 1
const uint8_t M1_ENC_B = D5;  // GPIO7  ← voie B (C2) de l'encodeur 1
const uint8_t M2_IN1 = D6;    // GPIO21 → BIN1 du DRV8833
const uint8_t M2_IN2 = D7;    // GPIO20 → BIN2 du DRV8833
const uint8_t M2_ENC_A = D1;  // GPIO3  ← voie A (C1) de l'encodeur 2
const uint8_t M2_ENC_B = D10; // GPIO10 ← voie B (C2) de l'encodeur 2
const uint8_t DRV_FAULT = D0; // GPIO2  ← nFAULT du DRV8833 (0 = défaut)

// 🚨----------- RÉGLAGES DES MOTEURS -----------
// Si un moteur ou un encodeur compte à l'envers, inutile de recâbler :
// voir « PREMIÈRE MISE EN SERVICE » en tête du programme.
const bool MOTOR2_ENABLED = true;       // false = fonctionnement avec un seul moteur 🚩
const bool MOT1_INVERT    = false;      // true = inverse le sens de rotation du moteur 1 🚩
const bool MOT2_INVERT    = false;      // true = inverse le sens de rotation du moteur 2 🚩
const bool ENC1_INVERT    = false;      // true = inverse le sens de comptage de l'encodeur 1 🚩
const bool ENC2_INVERT    = false;      // true = inverse le sens de comptage de l'encodeur 2 🚩
const char* MOT1_NAME     = "Store 1"; // Nom affiché sur la page web (ex. « Store gauche »)
const char* MOT2_NAME     = "Store 2"; // Nom affiché sur la page web (ex. « Store droit »)
const uint8_t NB_MOTORS = MOTOR2_ENABLED ? 2 : 1;   // Nombre de moteurs réellement utilisés

// 🚨----------- RÉGLAGES DE VITESSE (PWM sur 0 à 1023) -----------
const int SPEED_MAX  = 1023;   // Vitesse normale : 1023 = pleine tension. Baisser pour ralentir 🚩
const int SPEED_CAL  = 600;    // Vitesse pendant la calibration (plus lente, pour viser les butées) 🚩
const int SPEED_MIN  = 400;    // Vitesse minimale : doit suffire à faire DÉMARRER le store en montée 🚩
                               // (si « BLOCAGE » apparaît au démarrage, augmenter cette valeur)
const int RAMP_STEP  = 40;     // Accélération : augmentation de la vitesse toutes les 10 ms
const int SLOW_ZONE_PCT = 5;   // Ralentissement sur les derniers 5 % de la course avant la cible
const long SLOW_ZONE_MIN = 40; // …et au minimum sur les 40 dernières impulsions

// ----------- SÉCURITÉS -----------
const uint32_t STALL_MS       = 400;  // Moteur alimenté mais encodeur immobile pendant 400 ms → blocage
const long WRONG_DIR_COUNTS   = 60;   // Recul de 60 impulsions dans le mauvais sens → sens incohérent
const int  RESTART_WAIT_TICKS = 20;   // Pause freinée de 200 ms avant de repartir juste après un arrêt
const uint32_t RESTART_WAIT_MS = 300; // « Juste après un arrêt » = moins de 300 ms
const bool NFAULT_ENABLED     = true; // 🚨🚨false = nFAULT du DRV8833 non relié à D0 🚩 pour module rouge
const int  NFAULT_TICKS       = 3;    // nFAULT bas pendant 30 ms → défaut du DRV8833

// ----------- MODE SECOURS -----------
const uint32_t AP_RETRY_MS = 300000;  // En mode secours : nouvelle tentative vers la box toutes les 5 min
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
const uint8_t FAULT_DRV   = 3;     // Défaut signalé par le DRV8833 (surintensité / surchauffe)

// ----------- ÉTAT D'UN MOTEUR -----------
// Chaque moteur possède toutes ses variables : c'est ce qui le rend indépendant.
// Les variables partagées avec le temporisateur ou les interruptions sont « volatile ».
struct Motor {
  uint8_t in1, in2;               // Ses 2 broches de commande (DRV8833)
  uint8_t encA, encB;             // Ses 2 broches d'encodeur
  bool motInvert;                 // Sens de rotation inversé ?
  bool encInvert;                 // Sens de comptage inversé ?
  const char* name;               // Nom affiché sur la page web
  const char* keyTravel;          // Clé NVS de sa course totale
  const char* keyPos;             // Clé NVS de sa dernière position
  uint8_t ch1, ch2;               // Canaux PWM (utilisés seulement avec le paquet ESP32 2.x)
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
  bool calTop             = false;// Vrai quand sa position haute a été validée
  long lastSaved          = 0;    // Dernière position écrite en NVS (évite les écritures inutiles)
  uint32_t lastSaveMs     = 0;    // Instant de la dernière écriture
  uint32_t stoppedAt      = 0;    // Instant où la boucle a vu l'arrêt
  bool pendingSave        = false;// Sauvegarde à faire une fois le glissement terminé
};

Motor motors[2] = {                                         // Les deux moteurs
  { M1_IN1, M1_IN2, M1_ENC_A, M1_ENC_B, MOT1_INVERT, ENC1_INVERT, MOT1_NAME, "travel1", "pos1", 0, 1 },   // Moteur 1
  { M2_IN1, M2_IN2, M2_ENC_A, M2_ENC_B, MOT2_INVERT, ENC2_INVERT, MOT2_NAME, "travel2", "pos2", 2, 3 }    // Moteur 2
};                                                          // Fin du tableau

volatile bool otaBusy = false;          // Vrai pendant une mise à jour : commandes des moteurs refusées
bool updateRefused = false;             // Vrai si l'envoi par /update n'a pas le bon mot de passe
volatile bool drvFault = false;         // Vrai tant que le DRV8833 signale un défaut (nFAULT bas)
volatile int  drvFaultCount = 0;        // Nombre de périodes consécutives avec nFAULT bas

bool apMode = false;                    // Vrai en mode point d'accès de secours
bool apTrying = false;                  // Vrai pendant une tentative de retour sur la box
uint32_t apLastTry = 0;                 // Instant de la dernière tentative
uint32_t apTryStart = 0;                // Début de la tentative en cours

// ----------- OBJETS -----------
WebServer server(80);              // Serveur web sur le port 80 (port HTTP standard)
Preferences prefs;                 // Accès à la mémoire non volatile
esp_timer_handle_t ctrlTimer;      // Poignée du temporisateur qui régule les moteurs

const char* PREF_NAMESPACE = "rollerdc";   // NOUVEL espace de noms : positions en impulsions (≠ pas de la v4)

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
  m->pos += m->encInvert ? -d : d;                          // Met à jour la position
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
  // Mode « décroissance lente » du DRV8833 : une entrée à 1, l'autre en PWM.
  // La vitesse est ainsi presque proportionnelle au rapport cyclique, même à basse vitesse.
  if (speed > 0)      writeOutputs(m, PWM_MAX, PWM_MAX - speed);   // Descente
  else if (speed < 0) writeOutputs(m, PWM_MAX + speed, PWM_MAX);   // Montée
  else                brakeMotor(m);                               // Arrêt freiné
}                                                           // Fin de driveMotor

void haltMotor(Motor& m, uint8_t fault) {   // Arrêt immédiat d'UN moteur (aussi depuis le temporisateur)
  m.dir    = 0;                             // Plus de mouvement
  m.target = -1;                            // Annule une éventuelle cible
  m.duty   = 0;                             // Vitesse nulle
  m.wait   = 0;                             // Annule une pause en cours
  brakeMotor(m);                            // Freinage actif
  m.fault  = fault;                         // Mémorise l'éventuelle erreur
  m.lastHaltMs = millis();                  // Instant de l'arrêt
  m.stopEvent = true;                       // Prévient la boucle principale (affichage + sauvegarde)
}                                           // Fin de haltMotor

void startMove(Motor& m, int dir, long target) {            // Lance un moteur dans un sens (avec ou sans cible)
  int old = m.dir;                                          // Sens actuel
  m.dir = 0;                                                // Fige le moteur le temps de tout préparer
  m.fault = FAULT_NONE;                                     // Efface l'erreur précédente
  bool justStopped = (old == 0) && (millis() - m.lastHaltMs < RESTART_WAIT_MS);   // Il glisse peut-être encore
  if ((old != 0 && old != dir) || justStopped) {            // Inversion de sens ou arrêt tout récent :
    brakeMotor(m);                                          //  on freine d'abord…
    m.duty = 0;                                             //  …on repartira de la vitesse minimale…
    m.wait = RESTART_WAIT_TICKS;                            //  …après une courte pause
  } else if (old == 0) {                                    // Départ normal depuis l'arrêt
    m.duty = 0;                                             //  vitesse minimale puis rampe
    m.wait = 0;                                             //  sans pause
  }                                                         // Même sens qu'avant : la vitesse est conservée
  m.startPos = m.pos;                                       // Point de départ (contrôle du sens)
  m.stallRef = m.pos;                                       // Point de départ (contrôle du blocage)
  m.stallMs  = 0;                                           // Remise à zéro du chronomètre de blocage
  m.target   = target;                                      // Cible mémorisée AVANT de lancer le moteur
  m.dir      = dir;                                         // Lancement
}                                                           // Fin de startMove

void ctrlMotor(Motor& m) {                                  // Régulation d'UN moteur (toutes les 10 ms)
  int dir = m.dir;                                          // Copie locale du sens
  if (dir == 0) return;                                     // À l'arrêt : le frein est déjà appliqué

  if (m.wait > 0) {                                         // Pause freinée en cours
    int w = m.wait - 1;                                     // Une période de pause de moins
    m.wait = w;                                             // Mémorise le décompte
    if (w == 0) {                                           // Fin de la pause :
      m.startPos = m.pos;                                   //  nouveau point de départ (le glissement est fini)
      m.stallRef = m.pos;
      m.stallMs  = 0;
    }
    return;                                                 // Le moteur reste freiné pendant la pause
  }

  long pos = m.pos;                                         // Copie locale de la position

  if ((pos - m.startPos) * dir < -WRONG_DIR_COUNTS) {       // L'encodeur compte dans le sens opposé
    haltMotor(m, FAULT_DIR);                                // → sens moteur / encodeur incohérent
    return;
  }

  bool hasLimit = true;                                     // Y a-t-il un point d'arrêt à viser ?
  long limit = 0;                                           // Position à laquelle s'arrêter
  if (m.target >= 0)  limit = m.target;                     // « Aller à X % »
  else if (!m.cal)    limit = (dir > 0) ? m.travel : 0;     // Butée basse ou haute
  else                hasLimit = false;                     // Calibration : mouvement libre

  int want = m.cal ? SPEED_CAL : SPEED_MAX;                 // Vitesse souhaitée
  if (hasLimit) {
    long dist = (limit - pos) * dir;                        // Distance restante (positive si pas encore arrivé)
    if (dist <= 0) { haltMotor(m, FAULT_NONE); return; }    // Arrivé (ou dépassé) : arrêt freiné
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

  if (pos != m.stallRef) {                                  // L'encodeur a bougé :
    m.stallRef = pos;                                       //  tout va bien
    m.stallMs  = 0;
  } else {                                                  // L'encodeur n'a pas bougé :
    uint32_t still = m.stallMs + CTRL_MS;                   //  durée d'immobilité mise à jour
    m.stallMs = still;                                      //  mémorisée
    if (still >= STALL_MS) {                                // Immobile depuis trop longtemps :
      haltMotor(m, FAULT_STALL);                            //  blocage → arrêt
      return;
    }
  }

  driveMotor(m, dir * d);                                   // Applique la vitesse dans le bon sens
}                                                           // Fin de ctrlMotor

// Appelée toutes les 10 ms par le temporisateur, même si le Wi-Fi est occupé
void controlTick(void* arg) {
  if (NFAULT_ENABLED) {                                     // Surveillance du DRV8833
    if (digitalRead(DRV_FAULT) == LOW) {                    // nFAULT bas : surintensité ou surchauffe
      if (drvFaultCount < NFAULT_TICKS) drvFaultCount = drvFaultCount + 1;   // Filtre les parasites brefs
      if (drvFaultCount >= NFAULT_TICKS && !drvFault) {     // Défaut confirmé (30 ms) :
        drvFault = true;                                    //  mémorisé tant qu'il dure
        for (uint8_t i = 0; i < NB_MOTORS; i++) {           //  les DEUX moteurs sont arrêtés
          if (motors[i].dir != 0) haltMotor(motors[i], FAULT_DRV);   // (on ne sait pas lequel est en cause)
          else motors[i].fault = FAULT_DRV;                 //  moteur déjà arrêté : défaut affiché quand même
        }
      }
    } else {                                                // nFAULT haut : tout va bien
      drvFaultCount = 0;
      drvFault = false;                                     // Le défaut a disparu (le message reste jusqu'à la prochaine commande)
    }
  }
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Chaque moteur est traité séparément
    ctrlMotor(motors[i]);                                   // Régule selon son propre état
  }                                                         // Fin de la boucle
}                                                           // Fin de controlTick

int positionPercent(const Motor& m) {                       // Position d'un moteur en %
  if (m.travel <= 0) return 0;                              // Non calibré : 0 %
  long pct = (100L * m.pos + m.travel / 2) / m.travel;      // Calcul arrondi au plus proche
  return constrain(pct, 0, 100);                            // Limité entre 0 et 100 (léger dépassement possible)
}                                                           // Fin de positionPercent

bool canMove(const Motor& m) {                              // Ce moteur a-t-il le droit de bouger ?
  return m.cal || m.travel > 0;                             // Oui si calibré, ou si sa calibration est en cours
}                                                           // Fin de canMove

// ======================================================
// NVS (MÉMOIRE NON VOLATILE)
// ======================================================
void loadSettings() {                                       // Relit la calibration et la position de chaque moteur
  prefs.begin(PREF_NAMESPACE, true);                        // Ouverture en lecture seule
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Pour chaque moteur
    Motor& m = motors[i];                                   // Raccourci
    long travel = prefs.getLong(m.keyTravel, 0);            // Sa course (0 = jamais calibré)
    long pos    = prefs.getLong(m.keyPos, 0);               // Sa position
    if (travel < 0) travel = 0;                             // Sécurité : course jamais négative
    if (pos < 0) pos = 0;                                   // Sécurité : position jamais au-dessus du haut
    if (pos > travel) pos = travel;                         // Sécurité : position jamais au-delà du bas
    m.travel    = travel;                                   // Copie dans la variable partagée
    m.pos       = pos;                                      // Copie dans la variable partagée
    m.lastSaved = pos;                                      // Rien à réécrire tant que la position ne change pas
    Serial.printf("[NVS] %s : course = %ld impulsions, position = %ld\n", m.name, travel, pos);   // Débogage
  }                                                         // Fin de la boucle
  prefs.end();                                              // Fermeture
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

// ======================================================
// PAGE WEB (HTML + CSS + JavaScript stockés en mémoire flash)
// Les cartes des moteurs sont construites par le JavaScript
// à partir de la réponse de /api/status (une carte par moteur).
// ======================================================
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stores</title>
<link rel="icon" type="image/png" href="/favicon.png"><link rel="apple-touch-icon" href="/apple-touch-icon.png">
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
 #motors .card{margin-bottom:0}
 #motors.two{grid-template-columns:1fr 1fr}
 #motors.two button{font-size:1.3em;padding:14px 2px}
 #motors.two .lbl{display:none}
 .calcard summary{margin-top:0}
 button:active{filter:brightness(1.3)}
 .stop{background:#c0392b}.cal{background:#7a5bb5;font-size:.95em}.both{background:#2e8b7a}
 input[type=range]{width:100%}
 summary{cursor:pointer;margin-top:10px;color:#ccc}
 #msg{color:#ffb347;min-height:1.2em;text-align:center}
 .err{color:#ff6b5b}
 small{color:#aaa}
 a{color:#8ab4f8}
</style></head><body>
<h1>Stores enrouleurs</h1>

<div class="card">
 <div>Connexion : <b id="conn">--</b></div>
 <div style="margin-top:6px"><small>Signal Wi-Fi : <span id="rssi">--</span> dBm
 (bon au-dessus de -67, faible en dessous de -75)</small></div>
</div>

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

<div id="msg"></div>
<p style="text-align:center"><small><a href="/update">Mise à jour du programme</a></small></p>

<script>
let built=false;                                           // Cartes des moteurs déjà construites ?
function $(id){return document.getElementById(id);}       // Raccourci
function build(s){                                         // Construit les cartes de chaque moteur
 let h='',c='';                                            // h = déplacement (colonnes), c = calibration (pleine largeur)
 s.m.forEach((m,i)=>{let k=i+1;h+=`
<div class="card">
 <h2>${m.name}</h2>
 <div>Position : <b id="pct${k}">--</b> % <small>(0 % = haut, 100 % = bas)</small></div>
 <div class="bar"><div class="fill" id="fill${k}"></div></div>
 <div style="margin-top:6px">État : <b id="etat${k}">--</b></div>
 <div class="row">
  <button onclick="cmd('up',${k})" title="Monter">▲<span class="lbl"> Monter</span></button>
  <button class="stop" onclick="cmd('stop',${k})" title="Stop">■<span class="lbl"> Stop</span></button>
  <button onclick="cmd('down',${k})" title="Descendre">▼<span class="lbl"> Descendre</span></button>
 </div>
 <div style="margin-top:10px">Aller à : <b id="gv${k}">50</b> %</div>
 <input type="range" id="goto${k}" min="0" max="100" value="50" oninput="$('gv${k}').textContent=this.value">
 <div class="row"><button onclick="cmd('goto',${k},$('goto${k}').value)">Aller</button></div>
</div>`;c+=`
<div class="card calcard">
 <details><summary>Calibration — ${m.name}</summary>
  <small>1. Démarrer, puis appui bref sur ▼ : si le store monte, mettre MOT${k}_INVERT à true ;
  s'il descend mais s'arrête sur « sens incohérent », mettre ENC${k}_INVERT à true.
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
 $('motors').innerHTML=h;                                  // Cartes de déplacement (1 ou 2 colonnes)
 $('cals').innerHTML=c;                                    // Cartes de calibration (toujours pleine largeur)
 if(s.m.length>1){                                         // Deux moteurs :
  $('all').style.display='block';                          //  carte « les deux » en pleine largeur
  $('motors').classList.add('two');                        //  cartes des moteurs sur 2 colonnes
 }                                                         // Un seul moteur : une carte en pleine largeur
 built=true;
}
async function send(u){                                    // Envoie une adresse de commande à l'ESP32
 try{
  let r=await fetch(u);                                    // Envoi de la requête
  msg(await r.text());                                     // Message éventuel (vide si tout va bien)
 }catch(e){msg('Carte injoignable');}                      // Pas de réponse
 refresh();                                                // Met à jour l'affichage tout de suite
}
function cmd(a,m,p){send('/api/cmd?a='+a+'&m='+m+(p!==undefined?'&p='+p:''));}   // m = 0 : les deux Stores
function msg(t){$('msg').textContent=t;}                   // Affiche un message en bas de page
async function refresh(){                                  // Lit l'état de la carte
 try{
  let s=await (await fetch('/api/status')).json();         // Récupère l'état au format JSON
  if(!built)build(s);                                      // Première réponse : construit les cartes
  $('conn').textContent=s.ota?'Mise à jour en cours':(s.ap?'OK — point d\'accès de secours':'OK');   // État de la connexion
  $('rssi').textContent=s.rssi?s.rssi:'--';                // Force du signal Wi-Fi
  s.m.forEach((m,i)=>{let k=i+1;                           // Pour chaque moteur
   $('pct'+k).textContent=m.ready?m.pct:'--';              // Pourcentage
   $('fill'+k).style.width=(m.ready?m.pct:0)+'%';          // Barre de progression
   $('travel'+k).textContent=m.travel;                     // Course enregistrée
   $('step'+k).textContent=m.step;                         // Position brute (utile en calibration)
   let e=m.dir>0?'Descente':m.dir<0?'Montée':'Arrêt';      // Sens de rotation en texte
   let bad=false;                                          // Erreur à afficher en rouge ?
   if(s.ota)e='Mise à jour en cours';                      // Mise à jour du programme
   else if(m.fault==1){e+=' — BLOCAGE détecté';bad=true;}  // Moteur bloqué
   else if(m.fault==2){e+=' — sens incohérent (voir Calibration)';bad=true;}   // Inversion à régler
   else if(m.fault==3){e+=' — DÉFAUT DRV8833 (surintensité / surchauffe)'+(s.drv?' en cours':'');bad=true;}   // Pont en H en défaut
   else if(m.cal)e+=' — calibration en cours'+(m.top?' (haut validé)':'');   // Calibration
   else if(!m.ready)e+=' — non calibré';                   // Jamais calibré
   $('etat'+k).textContent=e;                              // Affichage de l'état
   $('etat'+k).className=bad?'err':'';                     // Rouge en cas d'erreur
  });
 }catch(e){$('conn').textContent='Hors ligne';}            // Carte injoignable
}
setInterval(refresh,500);                                  // Rafraîchit toutes les 500 ms
refresh();                                                 // Premier affichage immédiat
</script>
</body></html>
)rawliteral";

// Page de mise à jour par le navigateur (protégée par mot de passe)
const char UPDATE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mise à jour</title>
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
 fetch('/update/check?pw='+encodeURIComponent(pw))         // Vérifie le mot de passe AVANT d'envoyer le fichier
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
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Pour chaque moteur
    Motor& m = motors[i];                                   // Raccourci
    haltMotor(m, FAULT_NONE);                               // Arrêt freiné du moteur
    if (m.cal) { m.cal = false; m.calTop = false; }         // Calibration inachevée : abandonnée (position non sauvée)
    else savePosition(m);                                   // Sinon, la position est conservée pour le redémarrage
  }                                                         // Fin de la boucle
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

void handleUpdatePage() {                                   // GET /update : affiche la page d'envoi
  server.send_P(200, "text/html", UPDATE_HTML);             // Envoie la page (mot de passe vérifié à l'envoi)
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
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "fichier invalide ou incomplet");   // Réponse (la page ajoute « Échec : »)
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
  server.send_P(200, "text/html", PAGE_HTML);               // Code 200 = OK, contenu HTML
}                                                           // Fin de handleRoot

void handleFavicon() {                                      // Icône de l'onglet et de l'écran d'accueil
  server.sendHeader("Cache-Control", "max-age=604800");     // Gardée 7 jours par le navigateur
  server.send_P(200, "image/png", (const char*)favicon_png, favicon_png_len);   // Envoi du PNG depuis la flash
}                                                           // Fin de handleFavicon

void handleStatus() {                                       // Adresse « /api/status » : état en JSON
  char json[700];                                           // Tampon pour le texte JSON
  int n = snprintf(json, sizeof(json),                      // Début du JSON : informations générales
    "{\"ota\":%s,\"ap\":%s,\"drv\":%s,\"rssi\":%d,\"m\":[",
    otaBusy ? "true" : "false",                             // Mise à jour en cours ?
    apMode  ? "true" : "false",                             // Mode point d'accès de secours ?
    drvFault ? "true" : "false",                            // Défaut du DRV8833 en cours ?
    apMode ? 0 : (int)WiFi.RSSI());                         // Force du signal de la box (0 en point d'accès)
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Un bloc par moteur
    Motor& m = motors[i];                                   // Raccourci
    n += snprintf(json + n, sizeof(json) - n,
      "%s{\"name\":\"%s\",\"pct\":%d,\"step\":%ld,\"travel\":%ld,\"dir\":%d,\"fault\":%d,\"cal\":%s,\"top\":%s,\"ready\":%s}",
      i ? "," : "",                                         // Virgule entre les moteurs
      m.name, positionPercent(m), (long)m.pos, (long)m.travel, (int)m.dir, (int)m.fault,
      m.cal    ? "true" : "false",                          // Calibration en cours ?
      m.calTop ? "true" : "false",                          // Haut validé ?
      m.travel > 0 ? "true" : "false");                     // Moteur calibré ?
  }                                                         // Fin de la boucle
  snprintf(json + n, sizeof(json) - n, "]}");               // Fin du JSON
  server.send(200, "application/json", json);               // Envoi de la réponse
}                                                           // Fin de handleStatus

// Exécute une commande sur UN moteur. Renvoie nullptr si tout va bien,
// sinon le texte de l'erreur.
const char* doCommand(Motor& m, const String& a, int pct) {
  if (a == "stop") {                                        // --- STOP ---
    haltMotor(m, FAULT_NONE);                               // Arrêt freiné immédiat
    return nullptr;                                         // OK
  }
  if (drvFault && (a == "up" || a == "down" || a == "goto"))   // Pont en H encore en défaut :
    return "défaut DRV8833 en cours (surintensité ou surchauffe), patientez";   //  pas de mouvement
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
    m.pos    = 0;                                           // La position actuelle devient le haut
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
    Serial.printf("[CAL] %s : TERMINÉE\n", m.name);         // Débogage
    return nullptr;                                         // OK
  }
  return "commande inconnue";                               // --- COMMANDE INCONNUE ---
}                                                           // Fin de doCommand

void handleCmd() {                                          // Adresse « /api/cmd?a=...&m=... »
  String a = server.arg("a");                               // Nom de la commande
  int mot  = server.arg("m").toInt();                       // Moteur visé : 1, 2, ou 0 = les deux
  int pct  = constrain(server.arg("p").toInt(), 0, 100);    // Pourcentage (pour « goto »)
  Serial.printf("[WEB] Commande : %s, moteur %d\n", a.c_str(), mot);   // Débogage

  if (otaBusy) { server.send(409, "text/plain", "Mise à jour en cours, patientez"); return; }
  if (mot < 0 || mot > NB_MOTORS) { server.send(409, "text/plain", "Moteur inconnu"); return; }
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

// ======================================================
// WIFI
// ======================================================
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
    apLastTry = millis();                                   // Prochaine tentative vers la box dans 5 min
  }                                                         // Fin du test

  startMdns();                                              // Nom « store.local »
}                                                           // Fin de startWiFi

void startMdns() {                                          // Active le nom « store.local » et les annonces
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);                     // Annonce le serveur web
    MDNS.enableArduino(OTA_PORT, true);                     // Annonce l'OTA : la carte apparaît dans Outils > Port
    Serial.printf("[WIFI] Page web : http://%s.local\n", HOSTNAME);   // Débogage
  }                                                         // Fin du test mDNS
}                                                           // Fin de startMdns

// En mode secours, retente la box toutes les 5 min, sans bloquer la boucle.
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
    apLastTry = now;                                        // Prochaine tentative dans 5 min
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
  Serial.println("\n=== XIAO ESP32-C3 Stores enrouleurs (web) v5.2 - moteurs CC + encodeurs ===");   // Bannière

  loadSettings();                                           // Recharge calibrations et positions (avant les encodeurs)

  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Encodeurs de chaque moteur
    Motor& m = motors[i];                                   // Raccourci
    pinMode(m.encA, INPUT_PULLUP);                          // Voie A en entrée (rappel au +3,3 V)
    pinMode(m.encB, INPUT_PULLUP);                          // Voie B en entrée (rappel au +3,3 V)
    m.encState = (digitalRead(m.encA) << 1) | digitalRead(m.encB);   // État de départ
    attachInterruptArg(m.encA, encoderISR, &m, CHANGE);     // Interruption sur chaque front de A
    attachInterruptArg(m.encB, encoderISR, &m, CHANGE);     // Interruption sur chaque front de B
  }                                                         // Fin de la boucle

  if (NFAULT_ENABLED) pinMode(DRV_FAULT, INPUT_PULLUP);     // nFAULT : collecteur ouvert, haut = pas de défaut

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
  server.on("/api/status", handleStatus);                   // État au format JSON
  server.on("/api/cmd", handleCmd);                         // Commandes des moteurs
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
                      : (m.fault == FAULT_DRV)   ? " — DÉFAUT DRV8833" : "";   // Raison éventuelle
      Serial.printf("[MOTEUR] %s : ARRÊT à %ld impulsions (%d %%)%s\n",
                    m.name, (long)m.pos, positionPercent(m), why);   // Débogage
    }                                                       // Fin du test

    if (m.cal) { m.pendingSave = false; continue; }         // En calibration : aucune sauvegarde de position

    if (m.pendingSave && m.dir == 0 && millis() - m.stoppedAt >= SETTLE_MS) {   // Arrêt stabilisé
      m.pendingSave = false;                                // Fait
      savePosition(m);                                      // Position définitive enregistrée
    } else if (m.pos != m.lastSaved && millis() - m.lastSaveMs >= SAVE_EVERY_MS) {
      savePosition(m);                                      // En mouvement (ou store qui glisse) : sauvegarde périodique
    }                                                       // Fin du test
  }                                                         // Fin de la boucle
}                                                           // Fin de loop
