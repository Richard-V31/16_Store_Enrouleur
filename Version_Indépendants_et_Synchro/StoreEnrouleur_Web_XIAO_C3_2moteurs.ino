// ======================================================
// XIAO ESP32-C3 : Commande de stores enrouleurs par page web
// Version autonome, sans Blynk — DEUX MOTEURS, INDÉPENDANTS OU SYNCHRONISÉS
// v2 : sauvegarde de la position pendant le mouvement + mise à jour OTA
// v3 : deuxième moteur sur 4 GPIO séparés + alignement des deux côtés
// v3.1 : OTA réparée (puissance Wi-Fi réduite, veille Wi-Fi désactivée,
//        port série non bloquant), page /update avec le seul mot de passe OTA,
//        affichage de la force du signal Wi-Fi
// v4 : les deux moteurs deviennent indépendants : position, sens, butées,
//      calibration et sauvegarde propres à chacun. Commandes « les deux »
//      conservées pour piloter les deux moteurs d'un seul clic.
//      L'alignement disparaît (chaque moteur se règle maintenant seul).
// v4.1 : page web en pleine largeur ; avec 2 moteurs, leurs commandes
//        sont côte à côte sur 2 colonnes ; avec 1 moteur, pleine largeur.
// v4.2 : en mode 2 moteurs, boutons des moteurs réduits aux symboles ▲ ■ ▼
//        (texte conservé avec 1 moteur) ; calibration toujours en pleine largeur.
// v5 : menu « Réglage Moteurs » : inversion du sens (efface la calibration)
//      et curseur de vitesse propres à chaque moteur, enregistrés en NVS.
//      Point d'accès de secours : nouvel essai vers la box toutes les 2 min.
//      Le curseur « Aller à » démarre sur la position actuelle du store.
// v6 : - rampe d'accélération au démarrage et de décélération avant
//        l'arrivée (butée haute, butée basse ou position demandée)
//      - renommage des stores depuis la page (menu « Réglage Moteurs »)
//      - page d'informations (/info) : version, durée de fonctionnement,
//        cause du dernier démarrage, réseau, mémoire, et compteurs de
//        chaque moteur (mouvements, tours, cycles complets)
// v6.1 : page d'informations : cycles complets affichés en nombre entier
// v6.2 : les commandes de chaque store (titre jusqu'au bouton « Aller »)
//        sont entourées d'un cadre « Moteur gauche » / « Moteur droit ».
//        Calibration et menu « Réglage Moteurs » inchangés, en dessous.
// v7 : choix du fonctionnement sur la page (menu « Réglage Moteurs ») :
//      - « Indépendants » : comme en v6.2, chaque moteur a sa position,
//        ses butées, sa calibration et sa vitesse.
//      - « Synchronisés » : les deux moteurs tournent ensemble, demi-pas
//        par demi-pas (un seul store entraîné des deux côtés). Une seule
//        calibration, une seule position, la vitesse du moteur 1.
//        Chaque moteur garde son propre sens (moteurs face à face).
//        Carte « Alignement des deux côtés » : fait tourner un seul moteur,
//        store à l'arrêt, pour remettre le store droit.
//      Passage en synchronisé : calibration effacée (à refaire une fois).
//      Retour en indépendant : chaque moteur garde la calibration commune.
//      Le choix est enregistré en NVS (conservé après une coupure).
// v7.1 : le changement de mode (indépendants / synchronisés) demande le
//        mot de passe OTA (SECRET_OTA_PASS), saisi dans le menu « Réglage
//        Moteurs » et envoyé en POST (absent de l'adresse et de l'historique).
//        Les autres réglages (nom, sens, vitesse) restent libres.
// v7.2 : le mot de passe du changement de mode se saisit dans une fenêtre
//        (popup) qui s'ouvre au clic sur « Indépendants » ou « Synchronisés » ;
//        elle remplace le champ du menu et la confirmation séparée.
// v7.3 : page web plus fluide (fini les blocages d'environ 1 s) :
//        - une seule requête à la fois vers la carte : commandes et lecture
//          de l'état passent par une file d'attente (le serveur web de l'ESP32
//          ne sert qu'une connexion à la fois ; deux requêtes simultanées
//          faisaient attendre la seconde)
//        - une requête sans réponse est abandonnée au bout de 2 s
//        - une commande part tout de suite, sans attendre le rafraîchissement
//        - plus de demande automatique de /favicon.ico (pages / et /info)
// v7.3.1 : dans les cadres des moteurs, « Position » centrée, et en dessous
//          « 0 % = haut, 100 % = bas » centré (sans parenthèses)
// v7.3.2 : l'état de la connexion et la force du signal Wi-Fi quittent la page
//          principale pour la page d'informations (carte « Réseau »).
//          Si la carte ne répond plus, la page principale l'affiche en bas.
// v7.3.3 : dans les cadres des moteurs, le nom du store est centré
// v7.3.4 : plusieurs cartes = plusieurs applications sur le téléphone :
//          - PAGE_TITLE : titre de la page et nom proposé pour le raccourci
//            (menu du navigateur > « Ajouter à l'écran d'accueil »)
//          - ICON_COLOR : couleur de l'icône, dessinée par le programme
//            (un store sur fond coloré, /icon.png)
//          Avec plusieurs cartes, changer aussi HOSTNAME (store1, store2…)
//          et SECRET_AP_SSID dans arduino_secrets.h.
// ------------------------------------------------------
// Carte Arduino IDE : « XIAO_ESP32C3 » (paquet esp32 d'Espressif)
// Pour voir le moniteur série : Outils > USB CDC On Boot > Enabled
//   (obligatoire ici : D6/D7 servent au moteur 2, le moniteur passe par l'USB)
// Pour l'OTA : Outils > Partition Scheme > un schéma avec OTA (celui par défaut convient)
// Page web : http://store.local  ou l'adresse IP affichée sur le port série
// Informations : http://store.local/info
// Mise à jour par le navigateur : http://store.local/update
// Si le Wi-Fi de la box est introuvable : réseau du point d'accès de secours
// (SECRET_AP_SSID / SECRET_AP_PASS), puis http://192.168.5.1
// ------------------------------------------------------
// Câblage (inchangé) :
//   Moteur 1 (driver ULN2003 n°1) : D2→IN1, D3→IN2, D4→IN3, D5→IN4
//   Moteur 2 (driver ULN2003 n°2) : D1→IN1, D6→IN2, D7→IN3, D10→IN4
//   Alimentation 5 V externe (1 A minimum) sur les deux drivers,
//   masse (GND) commune avec le XIAO.
// Broches restées libres : D0, D8, D9 (broches de démarrage, réservées
// à des boutons avec résistance de rappel au +3,3 V).
// ------------------------------------------------------
// Commandes web : /api/cmd?a=<action>&m=<moteur>[&p=<pourcentage>]
//   m = 1 ou 2 : un seul moteur ; m = 0 : les deux moteurs
//   a = up, down, stop, goto (avec p = 0 à 100),
//       calstart, caltop, calbot (m = 1 ou 2 obligatoire)
//   En synchronisé, toutes ces commandes pilotent les deux moteurs ensemble.
//   a = jog (synchronisé seulement) : m = 1 ou 2, p = demi-pas
//       (négatif = monter) → alignement d'un seul côté
// Réglages : /api/set?m=<1 ou 2>[&inv=<0 ou 1>][&per=<µs par demi-pas>][&name=<nom>]
//            /api/set?sync=<0 ou 1>&pw=<mot de passe OTA>  (0 = indépendants, 1 = synchronisés)
//            (de préférence en POST, comme la page, pour ne pas mettre le mot de passe dans l'adresse)
// Infos    : /api/info  (avec &reset=<1 ou 2> : remise à zéro des compteurs)
// ------------------------------------------------------
// Passage depuis la v7.0 à v7.3.3 : aucune action (seule la page web change).
// Passage depuis la v6.x : aucune action. La carte démarre en mode
// SYNC_DEFAULT (indépendants) avec les calibrations existantes.
// ======================================================
#include "arduino_secrets.h"   // Fichier séparé contenant le nom du réseau (SSID) et le mot de passe WiFi

#include <WiFi.h>          // Bibliothèque Wi-Fi de l'ESP32
#include <WebServer.h>     // Serveur web intégré au paquet ESP32 (aucune bibliothèque à installer)
#include <ESPmDNS.h>       // Permet d'accéder à la carte par le nom « store.local »
#include <Preferences.h>   // Stockage en mémoire non volatile (NVS)
#include <ArduinoOTA.h>    // Mise à jour par le réseau depuis l'Arduino IDE (intégré au paquet ESP32)
#include <Update.h>        // Écriture d'un nouveau programme en flash (mise à jour par le navigateur)
#include "esp_timer.h"     // Temporisateur matériel de l'ESP32 (moteurs indépendants du réseau)
#include "esp_system.h"    // Cause du dernier démarrage (page d'informations)

#ifndef SECRET_OTA_PASS                     // Si le mot de passe OTA n'est pas dans arduino_secrets.h…
#define SECRET_OTA_PASS "store-ota"         // …mot de passe par défaut (À CHANGER dans arduino_secrets.h)
#endif                                      // Fin du test

const char* FW_VERSION = "7.3.4";             // Version du programme (affichée sur la page d'informations)

// ----------- WIFI -----------
char WIFI_SSID[] = SECRET_SSID;              // Nom du réseau Wi-Fi de la box
char WIFI_PASS[] = SECRET_PASS;             // Mot de passe du Wi-Fi
char AP_SSID[] = SECRET_AP_SSID;            // Nom du point d'accès de secours créé par la carte
char AP_PASS[]  = SECRET_AP_PASS;           // Mot de passe du point d'accès (8 caractères minimum)
const char* HOSTNAME  = "store";            // Nom réseau : la page sera sur http://store.local
const IPAddress AP_IP(192, 168, 5, 1);       // Adresse IP de la carte en mode point d'accès local
const IPAddress AP_MASK(255, 255, 255, 0);  // Masque de sous-réseau du point d'accès
const unsigned long STA_RETRY_MS = 120000;  // En point d'accès : nouvel essai vers la box toutes les 2 min

// 🚨----------- APPARENCE (à changer sur chaque carte) -----------
// Avec plusieurs cartes, donnez à chacune un titre et une couleur différents :
// ils servent au titre de la page, au nom proposé pour le raccourci du
// téléphone (« Ajouter à l'écran d'accueil ») et à la couleur de son icône.
// Titre : éviter les caractères < > & " et \ (ils casseraient la page).
const char* PAGE_TITLE = "Stores";          // Titre de la page et nom du raccourci 🚩
const char* ICON_COLOR = "#3d6fb6";         // Couleur de fond de l'icône, au format #RRGGBB 🚩

// ----------- OTA -----------
const char OTA_PASS[] = SECRET_OTA_PASS;    // Mot de passe OTA (Arduino IDE et page /update)
const uint16_t OTA_PORT = 3232;             // Port réseau de l'OTA Arduino (valeur standard)

// ----------- BROCHES DES MOTEURS (XIAO ESP32-C3) -----------
const uint8_t M1_PINS[4] = { D2, D3, D4, D5 };    // Moteur 1 : GPIO4, 5, 6, 7 → IN1 à IN4 du driver n°1A
const uint8_t M2_PINS[4] = { D1, D6, D7, D10 };   // Moteur 2 : GPIO3, 21, 20, 10 → IN1 à IN4 du driver n°1B
//                           IN1 IN2 IN3 IN4
// 🚨----------- RÉGLAGES DES MOTEURS -----------
// Sens, vitesse, nom et mode (indépendants / synchronisés) se règlent sur la
// page web (menu « Réglage Moteurs »). Les valeurs ci-dessous ne servent
// qu'au tout premier démarrage, tant que rien n'a été enregistré depuis la page.
const bool MOTOR2_ENABLED = true;       // false = fonctionnement avec 1 moteur par defaut true 🚩
const bool SYNC_DEFAULT   = false;      // Mode au premier démarrage : false = indépendants, true = synchronisés 🚩
const bool MOT1_INVERT    = false;      // Sens par défaut du moteur 1 (premier démarrage) 🚩
const bool MOT2_INVERT    = false;      // Sens par défaut du moteur 2 (premier démarrage) 🚩
const char* MOT1_NAME     = "Store 1"; // Nom par défaut (aussi repris si on efface le nom sur la page)
const char* MOT2_NAME     = "Store 2"; // Nom par défaut (aussi repris si on efface le nom sur la page)
const char* MOT1_SIDE     = "Store Gauche"; // Titre du cadre du moteur 1 sur la page (côté, emplacement…) 🚩
const char* MOT2_SIDE     = "Store Droit";  // Titre du cadre du moteur 2 sur la page 🚩
const uint32_t STEP_PERIOD_US = 3000;   // Vitesse par défaut : durée d'un demi-pas (3 ms ≈ 4,9 tr/min)
const long SAVE_EVERY_STEPS   = 500;    // Sauvegarde pendant le mouvement tous les 500 pas (≈ 1,5 s à 3 ms)
const uint8_t NB_MOTORS = MOTOR2_ENABLED ? 2 : 1;   // Nombre de moteurs réellement utilisés
const size_t MOTOR_NAME_LEN = 32;             // Longueur maximale d'un nom (en octets ; un accent compte pour 2)

// ----------- VITESSE -----------
// Le temporisateur tourne toutes les TICK_US ; chaque moteur fait un demi-pas
// tous les N « tops ». La vitesse se règle donc par pas de 0,5 ms.
// Plus le demi-pas est long, plus le moteur est lent… et plus il a de couple.
const uint32_t TICK_US       = 500;     // Période du temporisateur (0,5 ms)
const uint32_t SPEED_MIN_US  = 2000;    // Demi-pas le plus court autorisé (≈ 7,3 tr/min, le plus rapide)
const uint32_t SPEED_MAX_US  = 10000;   // Demi-pas le plus long autorisé (≈ 1,5 tr/min, le plus lent)
const uint32_t STEPS_PER_REV = 4096;    // Demi-pas par tour de l'axe de sortie d'un 28BYJ-48 (pour l'affichage)

// ----------- RAMPE -----------
// Le moteur démarre à RAMP_START_US par demi-pas, puis accélère d'un cran
// de 0,5 ms tous les RAMP_STEPS demi-pas jusqu'à la vitesse réglée.
// Avant l'arrivée, il ralentit de la même façon pour finir à RAMP_START_US.
// Si la vitesse réglée est déjà plus lente que RAMP_START_US, pas de rampe.
// Pas de décélération pendant la calibration (l'arrivée n'est pas connue)
// ni sur « Stop » (arrêt immédiat, par sécurité).
// En synchronisé, la rampe du moteur 1 s'applique aux deux moteurs.
const uint32_t RAMP_START_US = 6000;    // Vitesse de départ et d'arrivée (6 ms ≈ 2,4 tr/min) 🚩
const uint16_t RAMP_STEPS    = 24;      // Demi-pas sur chaque cran de 0,5 ms (plus grand = plus doux, 0 = sans rampe) 🚩
const uint8_t  RAMP_START_TICKS = RAMP_START_US / TICK_US;   // La même vitesse de départ, en tops

// ----------- ALIGNEMENT (mode synchronisé) -----------
// Un seul moteur tourne, lentement (RAMP_START_US par demi-pas), store à l'arrêt.
// La position affichée ne change pas : on corrige seulement un côté.
const long JOG_MAX = 4096;              // Déplacement maximal en attente pour un moteur (1 tour)

// ----------- DONNÉES DU MOTEUR PAS À PAS -----------
const uint8_t STEP_COUNT = 8;                     // Nombre de phases de la séquence (mode demi-pas)
const uint8_t stepSequence[STEP_COUNT][4] = {     // Table des états des 4 bobines pour chaque phase
  {1,0,0,1},   // Phase 0 : bobines A et D
  {0,0,0,1},   // Phase 1 : bobine D seule
  {0,0,1,1},   // Phase 2 : bobines C et D
  {0,0,1,0},   // Phase 3 : bobine C seule
  {0,1,1,0},   // Phase 4 : bobines B et C
  {0,1,0,0},   // Phase 5 : bobine B seule
  {1,1,0,0},   // Phase 6 : bobines A et B
  {1,0,0,0}    // Phase 7 : bobine A seule
};             // Fin de la table

// ----------- ÉTAT D'UN MOTEUR -----------
// Chaque moteur possède toutes ses variables : c'est ce qui le rend indépendant.
// En synchronisé, le moteur 1 « mène » : ses variables (sens, position, cible,
// calibration, rampe) commandent les deux moteurs, et le moteur 2 recopie
// sa position. Les variables partagées avec le temporisateur sont « volatile ».
struct Motor {
  const uint8_t* pins;            // Ses 4 broches (IN1 à IN4)
  volatile bool invert;           // Sens inversé ? (réglable depuis la page web)
  const char* defName;            // Nom par défaut (MOTx_NAME)
  const char* keyTravel;          // Clé NVS de sa course totale
  const char* keyPos;             // Clé NVS de sa dernière position
  const char* keyInv;             // Clé NVS de son sens
  const char* keyPer;             // Clé NVS de sa vitesse (durée d'un demi-pas en µs)
  const char* keyName;            // Clé NVS de son nom
  const char* keySteps;           // Clé NVS de son compteur de demi-pas
  const char* keyMoves;           // Clé NVS de son compteur de mouvements
  char name[MOTOR_NAME_LEN + 1] = "";   // Nom affiché sur la page web (réglable)
  volatile int  dir       = 0;    // Sens : +1 = descente, -1 = montée, 0 = arrêt
  volatile long pos       = 0;    // Position actuelle en pas (0 = tout en haut)
  volatile long travel    = 0;    // Course totale haut→bas en pas (0 = non calibré)
  volatile long target    = -1;   // Position cible pour « aller à X % » (-1 = pas de cible)
  volatile bool cal       = false;// Vrai pendant sa calibration (butées désactivées)
  volatile bool stopEvent = false;// Signale au programme principal qu'il vient de s'arrêter
  volatile long jog       = 0;    // Alignement : demi-pas restant à faire par ce moteur seul (+ = descente)
  volatile uint8_t periodTicks = STEP_PERIOD_US / TICK_US;   // Vitesse réglée : un demi-pas tous les N tops
  // --- Utilisées uniquement par le temporisateur ---
  uint8_t  curTicks       = RAMP_START_TICKS;   // Vitesse du moment (varie pendant la rampe)
  uint8_t  tickCount      = 0;    // Tops écoulés depuis le dernier demi-pas
  uint16_t rampCount      = 0;    // Demi-pas faits sur le cran de vitesse actuel
  int      lastDir        = 0;    // Sens au top précédent (détecte un démarrage)
  uint8_t  phase          = 0;    // Phase actuelle de la séquence
  volatile uint32_t stepCount = 0;// Demi-pas faits depuis le démarrage de la carte (compteur brut)
  volatile uint32_t moveCount = 0;// Mouvements lancés depuis le démarrage de la carte (compteur brut)
  // --- Utilisées uniquement par la boucle principale ---
  uint32_t stepSeen       = 0;    // Valeur de stepCount déjà ajoutée au total
  uint32_t moveSeen       = 0;    // Valeur de moveCount déjà ajoutée au total
  uint32_t totalSteps     = 0;    // Compteur cumulé de demi-pas (gardé en NVS)
  uint32_t totalMoves     = 0;    // Compteur cumulé de mouvements (gardé en NVS)
  uint32_t savedSteps     = 0;    // Dernier total de demi-pas écrit en NVS
  bool calTop             = false;// Vrai quand sa position haute a été validée
  long lastSaved          = 0;    // Dernière position écrite en NVS (évite les écritures inutiles)
};

Motor motors[2] = {                                         // Les deux moteurs
  { M1_PINS, MOT1_INVERT, MOT1_NAME,                        // Moteur 1 : broches, sens et nom par défaut
    "travel1", "pos1", "inv1", "per1", "name1", "steps1", "moves1" },   // …et ses clés NVS
  { M2_PINS, MOT2_INVERT, MOT2_NAME,                        // Moteur 2 : broches, sens et nom par défaut
    "travel2", "pos2", "inv2", "per2", "name2", "steps2", "moves2" }    // …et ses clés NVS
};                                                          // Fin du tableau

volatile bool syncMode = false;         // Vrai = moteurs synchronisés (le moteur 1 entraîne le 2)
volatile bool otaBusy = false;          // Vrai pendant une mise à jour : commandes des moteurs refusées
bool updateRefused = false;             // Vrai si l'envoi par /update n'a pas le bon mot de passe
bool apMode = false;                    // Vrai quand la carte fonctionne en point d'accès de secours
unsigned long lastStaTry = 0;           // Instant du dernier essai de connexion à la box (mode secours)

// ----------- OBJETS -----------
WebServer server(80);              // Serveur web sur le port 80 (port HTTP standard)
Preferences prefs;                 // Accès à la mémoire non volatile
esp_timer_handle_t stepTimer;      // Poignée du temporisateur qui fait tourner les moteurs

const char* PREF_NAMESPACE = "roller";        // Espace de noms dans la NVS (le même qu'en v3.1)
const char* OLD_KEY_TRAVEL = "travelSteps";   // Ancienne clé v3.1 : course commune (reprise au 1er démarrage)
const char* OLD_KEY_POS    = "position";      // Ancienne clé v3.1 : position commune (reprise au 1er démarrage)
const char* KEY_SYNC       = "sync";          // Clé NVS du mode (indépendants / synchronisés)

// ======================================================
// FONCTIONS DES MOTEURS
// ======================================================
void writePhase(const uint8_t* pins, uint8_t phase) {       // Applique une phase de la table à un moteur
  for (uint8_t i = 0; i < 4; i++) {                         // Pour chacune des 4 bobines
    digitalWrite(pins[i], stepSequence[phase][i]);          // Bobine alimentée (1) ou non (0)
  }                                                         // Fin de la boucle
}                                                           // Fin de writePhase

void releaseCoils(Motor& m) {                               // Coupe les bobines d'UN moteur
  for (uint8_t i = 0; i < 4; i++) {                         // Pour chacune des 4 bobines
    digitalWrite(m.pins[i], LOW);                           // Bobine hors tension
  }                                                         // Fin de la boucle
}                                                           // Fin de releaseCoils

void stepMotor(Motor& m, int dir) {                         // Fait avancer UN moteur d'un demi-pas
  if (m.invert) dir = -dir;                                 // Inversion éventuelle du sens
  m.phase = (m.phase + dir + STEP_COUNT) % STEP_COUNT;      // Phase suivante (ou précédente), entre 0 et 7
  writePhase(m.pins, m.phase);                              // Applique la phase
}                                                           // Fin de stepMotor

void haltMotor(Motor& m) {     // Arrêt immédiat d'UN moteur (utilisable depuis le temporisateur)
  m.dir    = 0;                // Plus de mouvement
  m.target = -1;               // Annule une éventuelle cible
  m.jog    = 0;                // Annule un alignement en cours
  releaseCoils(m);             // Coupe ses bobines (le moteur ne chauffe pas à l'arrêt)
  m.stopEvent = true;          // Prévient la boucle principale (affichage + sauvegarde)
  if (syncMode && &m == &motors[0]) {   // Synchronisés : le moteur 1 entraîne le 2, on arrête les deux
    Motor& f = motors[1];
    f.dir    = 0;
    f.target = -1;
    f.jog    = 0;
    releaseCoils(f);
    f.stopEvent = true;
  }
}                              // Fin de haltMotor

bool inCal(const Motor& m) {                                // Calibration en cours pour ce moteur ?
  return syncMode ? motors[0].cal : m.cal;                  // En synchronisé, c'est celle du moteur 1
}                                                           // Fin de inCal

bool isMoving(const Motor& m) {                             // Ce moteur est-il en mouvement ?
  return syncMode ? motors[0].dir != 0 : m.dir != 0;        // En synchronisé, c'est le moteur 1 qui décide
}                                                           // Fin de isMoving

uint8_t startTicks(const Motor& m) {                        // Vitesse au démarrage d'un mouvement
  if (RAMP_STEPS == 0) return m.periodTicks;                // Sans rampe : directement la vitesse réglée
  uint8_t p = m.periodTicks;                                // Vitesse réglée
  return (p > RAMP_START_TICKS) ? p : RAMP_START_TICKS;     // Sinon la plus lente des deux
}                                                           // Fin de startTicks

void updateRamp(Motor& m, int dir) {                        // Choisit la vitesse du prochain demi-pas
  uint8_t want = m.periodTicks;                             // Vitesse visée : celle réglée sur la page
  if (RAMP_STEPS == 0) { m.curTicks = want; return; }       // Sans rampe : rien d'autre à faire

  long rem = -1;                                            // Demi-pas restant avant l'arrivée (-1 = inconnu)
  if (m.target >= 0)  rem = labs(m.target - m.pos);         // « Aller à » : distance jusqu'à la cible
  else if (!m.cal)    rem = (dir > 0) ? m.travel - m.pos : m.pos;   // Sinon : distance jusqu'à la butée
  if (rem >= 0) {                                           // Arrivée connue : on prépare la décélération
    long limit = (long)RAMP_START_TICKS - rem / RAMP_STEPS; // Plus on approche, plus on doit être lent
    if (limit > want) want = limit;                         // Ne pas aller plus vite que cette limite
  }

  if (want > m.curTicks) {                                  // Il faut ralentir (arrivée proche, ou vitesse réduite)
    m.curTicks = want;                                      // Tout de suite : ralentir est sans risque
    m.rampCount = 0;
  } else if (want < m.curTicks) {                           // Il faut accélérer
    if (++m.rampCount >= RAMP_STEPS) {                      // Assez de demi-pas faits sur ce cran
      m.curTicks--;                                         // Cran suivant : 0,5 ms de moins par demi-pas
      m.rampCount = 0;
    }
  }
}                                                           // Fin de updateRamp

// Un top du temporisateur pour UN moteur.
// f = moteur « suiveur » (synchronisé) : il fait exactement les mêmes demi-pas.
void tickMotor(Motor& m, Motor* f = nullptr) {
  int dir = m.dir;                                          // Copie locale du sens
  if (dir == 0) { m.lastDir = 0; return; }                  // Moteur à l'arrêt : rien à faire

  if (dir != m.lastDir) {                                   // Démarrage, ou changement de sens en marche
    if (m.lastDir == 0) {                                   // Un mouvement de plus (compteurs)
      m.moveCount = m.moveCount + 1;
      if (f) f->moveCount = f->moveCount + 1;
    }
    m.lastDir   = dir;                                      // Mémorise le sens
    m.curTicks  = startTicks(m);                            // La rampe repart du début
    m.tickCount = 0;
    m.rampCount = 0;
  }

  if (++m.tickCount < m.curTicks) return;                   // Pas encore l'heure du prochain demi-pas
  m.tickCount = 0;                                          // C'est l'heure : on repart pour un cycle

  if (!m.cal) {                                             // Butées actives seulement hors calibration
    if (dir < 0 && m.pos <= 0)        { haltMotor(m); return; }   // Butée HAUTE atteinte
    if (dir > 0 && m.pos >= m.travel) { haltMotor(m); return; }   // Butée BASSE atteinte
  }                                                         // Fin du test des butées

  if (m.target >= 0 && m.pos == m.target) { haltMotor(m); return; }   // Position demandée atteinte

  stepMotor(m, dir);                                        // Un demi-pas
  m.pos = m.pos + dir;                                      // Met à jour sa position
  m.stepCount = m.stepCount + 1;                            // Compteur de demi-pas
  if (f) {                                                  // Synchronisé : le suiveur fait le même demi-pas
    stepMotor(*f, dir);                                     //   (avec son propre sens, inversé ou non)
    f->pos = m.pos;                                         //   même position
    f->stepCount = f->stepCount + 1;                        //   son compteur
  }
  updateRamp(m, dir);                                       // Vitesse du demi-pas suivant
}                                                           // Fin de tickMotor

void tickJog(Motor& m) {                                    // Alignement : un seul moteur, lentement
  long j = m.jog;                                           // Demi-pas restant
  if (j == 0) return;                                       // Rien à faire
  if (++m.tickCount < RAMP_START_TICKS) return;             // Vitesse lente et sûre
  m.tickCount = 0;
  int d = (j > 0) ? 1 : -1;                                 // Sens (+1 = descente)
  stepMotor(m, d);                                          // Un demi-pas (la position affichée ne change pas)
  m.stepCount = m.stepCount + 1;                            // Compteur de demi-pas
  m.jog = j - d;                                            // Un de moins
  if (m.jog == 0) {                                         // Alignement terminé
    releaseCoils(m);                                        // Bobines coupées
    m.stopEvent = true;                                     // La boucle sauvegarde les compteurs
  }
}                                                           // Fin de tickJog

// Appelée toutes les 0,5 ms par le temporisateur matériel, même si le Wi-Fi est occupé
void stepperTick(void* arg) {
  if (syncMode) {                                           // --- Moteurs synchronisés ---
    Motor& a = motors[0];                                   // Moteur 1 : il mène
    Motor& b = motors[1];                                   // Moteur 2 : il suit
    if (a.dir != 0) {                                       // Store en mouvement
      if (a.jog || b.jog) { a.jog = 0; b.jog = 0; }         // Un mouvement annule l'alignement
      tickMotor(a, &b);                                     // Les deux moteurs ensemble
    } else {                                                // Store à l'arrêt
      a.lastDir = 0;                                        // Prochain départ : la rampe repartira du début
      tickJog(a);                                           // Alignement éventuel, côté moteur 1
      tickJog(b);                                           // Alignement éventuel, côté moteur 2
    }
    return;
  }
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // --- Moteurs indépendants ---
    tickMotor(motors[i]);                                   // Avance (ou non) selon son propre état
  }                                                         // Fin de la boucle
}                                                           // Fin de stepperTick

int positionPercent(const Motor& m) {                       // Position d'un moteur en %
  if (m.travel <= 0) return 0;                              // Non calibré : 0 %
  long pct = (100L * m.pos + m.travel / 2) / m.travel;      // Calcul du pourcentage, arrondi au plus proche
  return constrain(pct, 0, 100);                            // Limité entre 0 et 100
}                                                           // Fin de positionPercent

bool canMove(const Motor& m) {                              // Ce moteur a-t-il le droit de bouger ?
  return m.cal || m.travel > 0;                             // Oui si calibré, ou si sa calibration est en cours
}                                                           // Fin de canMove

uint32_t periodUs(const Motor& m) {                         // Vitesse réglée d'un moteur, en µs par demi-pas
  return (uint32_t)m.periodTicks * TICK_US;                 // Nombre de tops × durée d'un top
}                                                           // Fin de periodUs

void setPeriod(Motor& m, long us) {                         // Règle la vitesse d'un moteur
  us = constrain(us, (long)SPEED_MIN_US, (long)SPEED_MAX_US);   // Reste dans la plage autorisée
  m.periodTicks = (us + TICK_US / 2) / TICK_US;             // Arrondi au top le plus proche (pas de 0,5 ms)
}                                                           // Fin de setPeriod

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
  if (n > MOTOR_NAME_LEN) {                                       // Trop long : on coupe…
    n = MOTOR_NAME_LEN;
    while (n > 0 && ((uint8_t)clean[n] & 0xC0) == 0x80) n--;   // …sans couper une lettre accentuée en deux
  }
  memcpy(m.name, clean.c_str(), n);                         // Copie du nom
  m.name[n] = '\0';                                         // Fin du texte
}                                                           // Fin de setName

// ----------- COMPTEURS -----------
// Le temporisateur incrémente stepCount et moveCount ; la boucle principale
// reporte l'écart dans les totaux. Aucune variable n'est écrite des deux côtés.
void accumulateCounters(Motor& m) {                         // Ajoute aux totaux ce qui a été fait depuis la dernière fois
  uint32_t s = m.stepCount;                                 // Lecture du compteur brut de demi-pas
  m.totalSteps += s - m.stepSeen;                           // Ajoute l'écart
  m.stepSeen = s;                                           // Mémorise la valeur prise en compte
  uint32_t v = m.moveCount;                                 // Même chose pour les mouvements
  m.totalMoves += v - m.moveSeen;
  m.moveSeen = v;
}                                                           // Fin de accumulateCounters

// ======================================================
// NVS (MÉMOIRE NON VOLATILE)
// ======================================================
void loadSettings() {                                       // Relit tout ce qui est propre à chaque moteur
  prefs.begin(PREF_NAMESPACE, true);                        // Ouverture en lecture seule
  long oldTravel = prefs.getLong(OLD_KEY_TRAVEL, 0);        // Course commune de la v3.1 (0 si absente)
  long oldPos    = prefs.getLong(OLD_KEY_POS, 0);           // Position commune de la v3.1 (0 si absente)
  syncMode = (NB_MOTORS == 2) && prefs.getBool(KEY_SYNC, SYNC_DEFAULT);   // Mode enregistré (sinon SYNC_DEFAULT)
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Pour chaque moteur
    Motor& m = motors[i];                                   // Raccourci
    long travel = prefs.getLong(m.keyTravel, oldTravel);    // Sa course (sinon celle de la v3.1)
    long pos    = prefs.getLong(m.keyPos, oldPos);          // Sa position (sinon celle de la v3.1)
    if (travel < 0) travel = 0;                             // Sécurité : course jamais négative
    if (pos < 0) pos = 0;                                   // Sécurité : position jamais au-dessus du haut
    if (pos > travel) pos = travel;                         // Sécurité : position jamais au-delà du bas
    m.travel    = travel;                                   // Copie dans la variable partagée
    m.pos       = pos;                                      // Copie dans la variable partagée
    m.lastSaved = -1;                                       // Force une première sauvegarde sous les nouvelles clés
    m.invert    = prefs.getBool(m.keyInv, m.invert);        // Son sens (sinon la valeur MOTx_INVERT)
    setPeriod(m, prefs.getUInt(m.keyPer, STEP_PERIOD_US));  // Sa vitesse (sinon STEP_PERIOD_US)
    if (prefs.isKey(m.keyName)) setName(m, prefs.getString(m.keyName));   // Son nom enregistré…
    else                        setName(m, m.defName);      // …sinon le nom par défaut
    m.totalSteps = prefs.getULong(m.keySteps, 0);           // Ses compteurs
    m.totalMoves = prefs.getULong(m.keyMoves, 0);
    m.savedSteps = m.totalSteps;
    Serial.printf("[NVS] %s : course = %ld pas, position = %ld, sens %s, %lu µs/demi-pas, %lu mouvements\n",
                  m.name, travel, pos, m.invert ? "inversé" : "normal",
                  (unsigned long)periodUs(m), (unsigned long)m.totalMoves);   // Débogage
  }                                                         // Fin de la boucle
  prefs.end();                                              // Fermeture
  if (syncMode) {                                           // Synchronisés : le moteur 1 fait foi
    motors[1].travel = motors[0].travel;                    // Même course
    motors[1].pos    = motors[0].pos;                       // Même position
  }
  Serial.printf("[NVS] Mode : moteurs %s\n", syncMode ? "synchronisés" : "indépendants");   // Débogage
}                                                           // Fin de loadSettings

void savePosition(Motor& m) {                               // Enregistre la position d'un moteur
  long pos = m.pos;                                         // Copie locale (la position peut changer pendant l'écriture)
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
  Serial.printf("[NVS] %s : course enregistrée = %ld pas\n", m.name, (long)m.travel);   // Débogage
}                                                           // Fin de saveTravel

void saveSettings(Motor& m) {                               // Enregistre le sens, la vitesse et le nom d'un moteur
  prefs.begin(PREF_NAMESPACE, false);                       // Ouverture en écriture
  prefs.putBool(m.keyInv, m.invert);                        // Écrit le sens
  prefs.putUInt(m.keyPer, periodUs(m));                     // Écrit la vitesse
  prefs.putString(m.keyName, m.name);                       // Écrit le nom
  prefs.end();                                              // Fermeture
  Serial.printf("[NVS] %s : sens %s, %lu µs/demi-pas\n",
                m.name, m.invert ? "inversé" : "normal", (unsigned long)periodUs(m));   // Débogage
}                                                           // Fin de saveSettings

void saveCounters(Motor& m, bool force = false) {           // Enregistre les compteurs d'un moteur
  accumulateCounters(m);                                    // Totaux à jour
  if (!force && m.totalSteps == m.savedSteps) return;       // Rien de nouveau : on épargne la flash
  prefs.begin(PREF_NAMESPACE, false);                       // Ouverture en écriture
  prefs.putULong(m.keySteps, m.totalSteps);                 // Écrit le total de demi-pas
  prefs.putULong(m.keyMoves, m.totalMoves);                 // Écrit le total de mouvements
  prefs.end();                                              // Fermeture
  m.savedSteps = m.totalSteps;                              // Mémorise ce qui vient d'être écrit
}                                                           // Fin de saveCounters

void saveAllAtBoot() {                                      // Écrit les clés propres à chaque moteur dès le démarrage
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Pour chaque moteur
    saveTravel(motors[i]);                                  // Course sous sa propre clé
    savePosition(motors[i]);                                // Position sous sa propre clé
  }                                                         // Fin de la boucle
}                                                           // Fin de saveAllAtBoot

void clearCalibration(Motor& m) {                           // Efface la calibration d'un moteur (« non calibré »)
  m.cal    = false;                                         // Abandon d'une calibration en cours
  m.calTop = false;
  m.travel = 0;                                             // Plus de butées valables
  m.pos    = 0;
  saveTravel(m);                                            // Enregistre la course effacée
  savePosition(m);                                          // Enregistre la position remise à zéro
}                                                           // Fin de clearCalibration

void mirrorFollower(bool save) {                            // Synchronisés : le moteur 2 recopie le moteur 1
  Motor& a = motors[0];
  Motor& b = motors[1];
  b.travel = a.travel;                                      // Même course
  b.pos    = a.pos;                                         // Même position
  if (save) { saveTravel(b); savePosition(b); }             // Enregistrement (fin de calibration)
}                                                           // Fin de mirrorFollower

// ----------- ICÔNE (dessinée par le programme, couleur ICON_COLOR) -----------
// Image PNG de 192 × 192 points, 4 couleurs, fabriquée une seule fois au premier appel.
const int ICON_SIZE = 192;                  // Taille de l'icône en points
uint8_t* iconPng = nullptr;                 // Image PNG en mémoire (nullptr = pas encore fabriquée)
size_t iconLen = 0;                         // Taille de l'image PNG en octets

uint32_t crc32Png(const uint8_t* d, size_t n) {             // Somme de contrôle exigée par le format PNG
  uint32_t c = 0xFFFFFFFFUL;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320UL : (c >> 1);
  }
  return c ^ 0xFFFFFFFFUL;
}                                                           // Fin de crc32Png

uint8_t iconPixel(int x, int y) {                           // Couleur d'un point : 0 = fond, 1 = blanc, 2 = fond foncé, 3 = gris nuit
  if (y >= 34 && y < 52 && x >= 28 && x < 164) return 1;    // Tube d'enroulement
  if (y >= 52 && y < 130 && x >= 42 && x < 150) {          // Toile du store
    return ((y - 52) % 16 >= 13) ? 2 : 1;                   //   avec des lignes horizontales
  }
  if (y >= 130 && y < 140 && x >= 34 && x < 158) return 3;  // Barre de charge en bas de la toile
  if (y >= 140 && y < 162 && x >= 139 && x < 143) return 1; // Cordon
  int dx = x - 141, dy = y - 167;
  if (dx * dx + dy * dy <= 36) return 1;                    // Poignée du cordon
  return 0;                                                 // Fond coloré
}                                                           // Fin de iconPixel

uint8_t hexByte(const char* s) {                            // « 3d » → 61
  char b[3] = { s[0], s[1], 0 };
  return (uint8_t)strtol(b, nullptr, 16);
}                                                           // Fin de hexByte

void buildIcon() {                                          // Fabrique l'image PNG de l'icône
  const size_t rowBytes = 1 + ICON_SIZE / 4;                // Un octet de filtre + 4 points par octet
  const size_t raw = rowBytes * ICON_SIZE;                  // Données de l'image (9408 octets)
  const size_t cap = 8 + 25 + 24 + 12 + 7 + raw + 4 + 12;   // Taille totale du fichier PNG
  iconPng = (uint8_t*)malloc(cap);
  if (!iconPng) return;                                     // Plus de mémoire : pas d'icône
  size_t p = 0, start = 0;
  auto put8  = [&](uint8_t b)  { iconPng[p++] = b; };
  auto put32 = [&](uint32_t v) { put8(v >> 24); put8(v >> 16); put8(v >> 8); put8(v); };
  auto chunk = [&](uint32_t len, const char* t) { put32(len); start = p; for (int i = 0; i < 4; i++) put8(t[i]); };
  auto endChunk = [&]() { put32(crc32Png(iconPng + start, p - start)); };

  const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };   // Signature PNG
  for (int i = 0; i < 8; i++) put8(sig[i]);

  chunk(13, "IHDR");                                        // En-tête : taille, 2 bits par point, palette
  put32(ICON_SIZE); put32(ICON_SIZE); put8(2); put8(3); put8(0); put8(0); put8(0);
  endChunk();

  const char* c = (ICON_COLOR[0] == '#') ? ICON_COLOR + 1 : ICON_COLOR;   // Couleur « #RRGGBB »
  uint8_t r = 0x3d, g = 0x6f, b = 0xb6;                     // Bleu par défaut si la couleur est mal écrite
  if (strlen(c) >= 6) { r = hexByte(c); g = hexByte(c + 2); b = hexByte(c + 4); }
  chunk(12, "PLTE");                                        // Les 4 couleurs
  put8(r); put8(g); put8(b);                                // 0 : couleur choisie
  put8(255); put8(255); put8(255);                          // 1 : blanc
  put8(r * 6 / 10); put8(g * 6 / 10); put8(b * 6 / 10);     // 2 : couleur choisie, plus foncée
  put8(0x1e); put8(0x24); put8(0x30);                       // 3 : gris nuit (fond de la page)
  endChunk();

  chunk(2 + 5 + raw + 4, "IDAT");                           // Image, sans compression (format zlib « stocké »)
  put8(0x78); put8(0x01);                                   // En-tête zlib
  put8(0x01);                                               // Un seul bloc, non compressé
  put8(raw & 0xFF); put8(raw >> 8);                         // Longueur du bloc…
  put8(~raw & 0xFF); put8((~raw >> 8) & 0xFF);              // …et son complément
  uint32_t s1 = 1, s2 = 0;                                  // Somme de contrôle Adler-32
  for (int y = 0; y < ICON_SIZE; y++) {
    for (size_t i = 0; i < rowBytes; i++) {
      uint8_t v = 0;                                        // Octet 0 de la ligne : filtre « aucun »
      if (i > 0) {
        int x = (i - 1) * 4;
        v = (iconPixel(x, y) << 6) | (iconPixel(x + 1, y) << 4)
          | (iconPixel(x + 2, y) << 2) | iconPixel(x + 3, y);
      }
      put8(v);
      s1 = (s1 + v) % 65521;
      s2 = (s2 + s1) % 65521;
    }
  }
  put32((s2 << 16) | s1);
  endChunk();

  chunk(0, "IEND");                                         // Fin du fichier
  endChunk();
  iconLen = p;
}                                                           // Fin de buildIcon

// ======================================================
// PAGE WEB (HTML + CSS + JavaScript stockés en mémoire flash)
// @@TITLE@@ et @@COLOR@@ sont remplacés à l'envoi par PAGE_TITLE et ICON_COLOR.
// Les cartes des moteurs sont construites par le JavaScript
// à partir de la réponse de /api/status (une carte par moteur,
// ou une seule carte pour les deux en mode synchronisé).
// ======================================================
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>@@TITLE@@</title>
<link rel="icon" type="image/png" href="/icon.png">
<link rel="apple-touch-icon" href="/icon.png">
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="@@COLOR@@">
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
 small{color:#aaa}
 a{color:#8ab4f8}
 .ends{display:flex;justify-content:space-between}
 #pop{display:none;position:fixed;inset:0;background:rgba(0,0,0,.65);z-index:10;align-items:center;justify-content:center;padding:16px}
 #pop.on{display:flex}
 #pop .box{background:#2b3240;border-radius:14px;padding:18px;width:100%;max-width:380px;box-shadow:0 8px 30px rgba(0,0,0,.5)}
 #pop h2{margin-bottom:10px}
 #pop p{margin:0 0 12px;color:#ccc;font-size:.95em;line-height:1.4}
 #pop input{width:100%;padding:10px;font-size:1.05em;border-radius:8px;border:0}
 #pop .err{color:#ff7b6b;min-height:1.2em;margin-top:8px;font-size:.9em}
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
   Synchronisés : un seul store entraîné des deux côtés, les deux moteurs font
   exactement les mêmes pas (une seule calibration, vitesse du premier moteur).</small>
  </div>
  <small>Le changement de sens efface la calibration du moteur concerné
  (en synchronisé : la calibration commune) : il faudra la refaire.
  Plus le moteur est lent, plus il a de force.</small>
  <div id="regs"></div>
 </details>
</div>

<div id="pop">
 <div class="box">
  <h2 id="ptitle">--</h2>
  <p id="ptext">--</p>
  <input type="password" id="ppw" placeholder="Mot de passe" autocomplete="off"
   onkeydown="if(event.key==='Enter')popOk();if(event.key==='Escape')popClose();">
  <div class="err" id="perr"></div>
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
let built=false;                                           // Cartes des moteurs déjà construites ?
let bSync=null;                                            // Mode utilisé lors de la construction
let S=null;                                                // Dernier état reçu de la carte
// ---- File d'attente : une seule requête à la fois vers la carte ----
// Le serveur web de l'ESP32 ne sert qu'une connexion à la fois : deux requêtes
// simultanées font attendre la seconde. Ici, tout passe par une boucle unique.
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
function build(s){                                         // Construit les cartes selon le mode
 let h='',c='',r='';                                       // h = cadres des stores, c = calibration, r = réglages
 let smax=(s.pmax-s.pmin)/s.ptick;                         // Nombre de crans du curseur de vitesse
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
  <small>1. Démarrer &nbsp;2. Monter tout en haut, Stop, « Valider HAUT »
  &nbsp;3. Descendre tout en bas, Stop, « Valider BAS »</small>
  <div class="row">
   <button class="cal" onclick="cmd('calstart',${k})">Démarrer</button>
   <button class="cal" onclick="cmd('caltop',${k})">Valider HAUT</button>
   <button class="cal" onclick="cmd('calbot',${k})">Valider BAS</button>
  </div>
  <div style="margin-top:8px"><small>Course : <span id="travel${k}">--</span> pas</small></div>
 </details>
</div>`;});
 if(s.sync){c+=`
<div class="card calcard">
 <details><summary>Alignement des deux côtés</summary>
  <small>Store à l'arrêt : fait tourner un seul moteur pour remettre le store droit,
  sans changer la position affichée. ▲▲ / ▼▼ = 1/8 de tour, ▲ / ▼ = petit pas.
  À faire de préférence avant la calibration.</small>
  ${s.m.map((m,i)=>{let k=i+1;return `
  <div style="margin-top:10px">${m.side} — <span class="nm${k}">${m.name}</span></div>
  <div class="row">
   <button class="cal" onclick="jog(${k},-512)">▲▲</button>
   <button class="cal" onclick="jog(${k},-64)">▲</button>
   <button class="cal" onclick="jog(${k},64)">▼</button>
   <button class="cal" onclick="jog(${k},512)">▼▼</button>
  </div>`;}).join('')}
 </details>
</div>`;}
 s.m.forEach((m,i)=>{let k=i+1;let n=`<span class="nm${k}">${m.name}</span>`;
  let v=(s.sync&&k>1)?`
 <div style="margin-top:12px"><small>Vitesse : celle de <span class="nm1">${s.m[0].name}</span>
 (les deux moteurs tournent ensemble)</small></div>`:`
 <div style="margin-top:12px">Vitesse${s.sync?' (les deux moteurs)':''} : <b id="rpm${k}">--</b> tr/min</div>
 <div><small><span id="ms${k}">--</span> ms par demi-pas</small></div>
 <input type="range" id="spd${k}" min="0" max="${smax}" step="1" value="0"
  oninput="showSpd(${k})" onchange="setSpd(${k})">
 <div class="ends"><small>Lent, plus de force</small><small>Rapide</small></div>`;
  r+=`
<div class="card">
 <h2>${n}</h2>${two?`<small>${m.side}</small>`:''}
 <div style="margin-top:6px">Nom :</div>
 <input type="text" id="name${k}" maxlength="32" value="${m.name}">
 <div class="row"><button class="cal" onclick="ren(${k})">Renommer</button></div>
 <div style="margin-top:12px">Sens : <b id="inv${k}">--</b></div>
 <div class="row"><button class="cal" onclick="inv(${k})">Inverser le sens</button></div>${v}
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
function per(k){return S.pmax-$('spd'+k).value*S.ptick;}  // Position du curseur → µs par demi-pas
function showSpd(k){                                       // Affiche la vitesse choisie sur le curseur
 let u=per(k);
 T('rpm'+k,(60e6/(u*S.spr)).toFixed(1));                  // Tours par minute de l'axe de sortie
 T('ms'+k,(u/1000).toFixed(1));                            // Durée d'un demi-pas en ms
}
function setSpd(k){send('/api/set?m='+k+'&per='+per(k));} // Curseur relâché : envoi à la carte
function inv(k){                                           // Inversion du sens d'un moteur
 let m=S.m[k-1];
 let t=S.sync?'La calibration commune sera effacée et devra être refaite.'
             :'Sa calibration sera effacée et devra être refaite.';
 if(confirm('Inverser le sens de '+m.name+' ?\n'+t))
  send('/api/set?m='+k+'&inv='+(m.inv?0:1));
}
function ren(k){                                           // Nouveau nom d'un moteur
 send('/api/set?m='+k+'&name='+encodeURIComponent($('name'+k).value));
}
let popMode=0;                                              // Mode demandé dans la fenêtre
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
 else{$('perr').textContent=r.t;$('ppw').value='';$('ppw').focus();}   // Mot de passe faux ou carte injoignable : on reste
}
function jog(k,p){cmd('jog',k,p);}                         // Alignement d'un seul côté
async function send(u){                                    // Envoie une adresse de commande à l'ESP32
 let r=await ask(u);                                       // Mise en file (l'état est relu juste après)
 msg(r.t);                                                 // Message éventuel (vide si tout va bien)
}
function cmd(a,m,p){send('/api/cmd?a='+a+'&m='+m+(p!==undefined?'&p='+p:''));}   // m = 0 : les deux Stores
function msg(t){$('msg').textContent=t;}                   // Affiche un message en bas de page
async function refresh(){                                  // Lit l'état de la carte
 try{
  let r=await req('/api/status');                          // Récupère l'état (2 s maximum)
  if(!r.ok){msg('Carte injoignable');return;}             // Pas de réponse : message en bas de page
  let s=JSON.parse(r.t);                                   // État au format JSON
  S=s;                                                     // Mémorise l'état (utilisé par les réglages)
  if(!built||s.sync!==bSync)build(s);                      // Première réponse ou changement de mode : (re)construit
  if($('msg').textContent==='Carte injoignable')msg('');   // Carte de nouveau joignable : message effacé
  T('mode',s.sync?'moteurs synchronisés':'moteurs indépendants');   // Mode actuel
  s.m.forEach((m,i)=>{let k=i+1;                           // Réglages de chaque moteur
   document.querySelectorAll('.nm'+k).forEach(e=>e.textContent=m.name);   // Nom (après un renommage)
   T('inv'+k,m.inv?'inversé':'normal');                    // Sens du moteur
   let sp=$('spd'+k);                                      // Curseur de vitesse (absent pour le moteur 2 en synchronisé)
   if(sp&&document.activeElement!==sp){                    // Pas pendant que l'utilisateur le déplace
    sp.value=(s.pmax-m.per)/s.ptick;                       // Vitesse enregistrée dans la carte
    showSpd(k);
   }
  });
  let jg=s.sync&&(s.m[0].jog||s.m[1].jog);                 // Alignement en cours ?
  (s.sync?[s.m[0]]:s.m).forEach((m,i)=>{let k=i+1;         // Cadres de commande
   T('pct'+k,m.ready?m.pct:'--');                          // Pourcentage
   $('fill'+k).style.width=(m.ready?m.pct:0)+'%';          // Barre de progression
   T('travel'+k,m.travel);                                 // Course enregistrée
   let e=m.dir>0?'Descente':m.dir<0?'Montée':'Arrêt';      // Sens de rotation en texte
   if(s.ota)e='Mise à jour en cours';                      // Mise à jour du programme
   else if(jg)e='Alignement en cours';                     // Un seul moteur tourne
   else if(m.cal)e+=' — calibration en cours'+(m.top?' (haut validé)':'');   // Calibration
   else if(!m.ready)e+=' — non calibré';                   // Jamais calibré
   T('etat'+k,e);                                          // Affichage de l'état
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
<link rel="icon" type="image/png" href="/icon.png">
<link rel="apple-touch-icon" href="/icon.png">
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
  let s=await (await fetch('/api/info')).json();
  $('prog').innerHTML=row('Version',s.ver)+row('Compilé le',s.build)
   +row('En marche depuis',dur(s.up))+row('Dernier démarrage',s.reset)
   +(s.m.length>1?row('Moteurs',s.sync?'synchronisés':'indépendants'):'')
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
    +row('Tours du moteur',(m.steps/s.spr).toFixed(1))
    +row('Cycles complets (montée + descente)',m.travel>0?Math.floor(m.steps/(2*m.travel)):'-- (non calibré)');
  });
 }catch(e){$('net').innerHTML=row('Connexion','<b>Hors ligne</b>');}   // Carte injoignable
}
async function rst(k){                                     // Remise à zéro des compteurs d'un moteur
 if(!confirm('Remettre à zéro les compteurs de ce moteur ?'))return;
 try{await fetch('/api/info?reset='+k);}catch(e){}
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
<title>Mise à jour</title>
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
    haltMotor(m);                                           // Arrêt du moteur
    if (!inCal(m)) savePosition(m);                         // Hors calibration : position conservée pour le redémarrage
    saveCounters(m);                                        // Les compteurs aussi
  }                                                         // Fin de la boucle
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Calibration inachevée : abandonnée (position non sauvée)
    motors[i].cal = false;
    motors[i].calTop = false;
  }
  esp_timer_stop(stepTimer);                                // Suspend le temporisateur : tout le processeur pour l'OTA
}                                                           // Fin de prepareUpdate

void cancelUpdate() {                                       // Mise à jour échouée : retour au fonctionnement normal
  otaBusy = false;                                          // Les moteurs redeviennent commandables
  esp_timer_start_periodic(stepTimer, TICK_US);             // Relance le temporisateur des moteurs
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
    server.send(401, "text/plain", "Mot de passe incorrect");   // 401 = non autorisé
    return;                                                 // Fin
  }
  bool ok = !Update.hasError();                             // La mise à jour a-t-elle réussi ?
  server.sendHeader("Connection", "close");                 // Ferme la connexion après la réponse
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "Échec : fichier invalide");   // Réponse
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
void sendPage(const char* tpl) {                            // Envoie une page avec le titre et la couleur de cette carte
  String html(tpl);                                         // Copie de la page en mémoire vive
  html.replace("@@TITLE@@", PAGE_TITLE);                    // Titre (PAGE_TITLE)
  html.replace("@@COLOR@@", ICON_COLOR);                    // Couleur (ICON_COLOR)
  server.send(200, "text/html", html);                      // Code 200 = OK, contenu HTML
}                                                           // Fin de sendPage

void handleRoot() {                                         // Adresse « / » : envoie la page
  sendPage(PAGE_HTML);
}                                                           // Fin de handleRoot

void handleIcon() {                                         // Adresse « /icon.png » : icône de la page et du raccourci
  if (!iconPng) buildIcon();                                // Fabriquée au premier appel seulement
  if (!iconPng) { server.send(500, "text/plain", "Mémoire insuffisante"); return; }
  server.sendHeader("Cache-Control", "max-age=86400");      // Le navigateur la garde un jour : pas de nouvelle demande
  server.send_P(200, "image/png", (PGM_P)iconPng, iconLen); // Envoi de l'image
}                                                           // Fin de handleIcon

void handleManifest() {                                     // Adresse « /manifest.json » : nom et icône du raccourci
  String j = "{\"name\":\"";
  j += PAGE_TITLE;
  j += "\",\"short_name\":\"";
  j += PAGE_TITLE;
  j += "\",\"start_url\":\"/\",\"display\":\"standalone\",\"background_color\":\"#1e2430\",\"theme_color\":\"";
  j += ICON_COLOR;
  j += "\",\"icons\":[{\"src\":\"/icon.png\",\"sizes\":\"192x192\",\"type\":\"image/png\"}]}";
  server.sendHeader("Cache-Control", "max-age=86400");      // Le navigateur le garde un jour
  server.send(200, "application/manifest+json", j);
}                                                           // Fin de handleManifest

void handleStatus() {                                       // Adresse « /api/status » : état en JSON
  char json[1100];                                          // Tampon pour le texte JSON
  int n = snprintf(json, sizeof(json),                      // Début du JSON : informations générales
    "{\"ota\":%s,\"ap\":%s,\"sync\":%s,\"rssi\":%d,\"pmin\":%lu,\"pmax\":%lu,\"ptick\":%lu,\"spr\":%lu,\"m\":[",
    otaBusy  ? "true" : "false",                            // Mise à jour en cours ?
    apMode   ? "true" : "false",                            // Point d'accès de secours ?
    syncMode ? "true" : "false",                            // Moteurs synchronisés ?
    (int)WiFi.RSSI(),                                       // Force du signal de la box (0 en point d'accès)
    (unsigned long)SPEED_MIN_US, (unsigned long)SPEED_MAX_US,   // Plage de vitesse (µs par demi-pas)
    (unsigned long)TICK_US, (unsigned long)STEPS_PER_REV);  // Pas du curseur, demi-pas par tour
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Un bloc par moteur
    Motor& m = motors[i];                                   // Raccourci
    n += snprintf(json + n, sizeof(json) - n,
      "%s{\"name\":\"%s\",\"side\":\"%s\",\"pct\":%d,\"step\":%ld,\"travel\":%ld,\"dir\":%d,\"cal\":%s,\"top\":%s,\"ready\":%s,\"inv\":%s,\"per\":%lu,\"jog\":%ld}",
      i ? "," : "",                                         // Virgule entre les moteurs
      m.name, (i == 0) ? MOT1_SIDE : MOT2_SIDE,             // Nom, titre du cadre
      positionPercent(m), (long)m.pos, (long)m.travel, (int)m.dir,
      m.cal    ? "true" : "false",                          // Calibration en cours ?
      m.calTop ? "true" : "false",                          // Haut validé ?
      m.travel > 0 ? "true" : "false",                      // Moteur calibré ?
      m.invert ? "true" : "false",                          // Sens inversé ?
      (unsigned long)periodUs(m),                           // Vitesse (µs par demi-pas)
      (long)m.jog);                                         // Alignement restant (demi-pas)
  }                                                         // Fin de la boucle
  snprintf(json + n, sizeof(json) - n, "]}");               // Fin du JSON
  server.send(200, "application/json", json);               // Envoi de la réponse
}                                                           // Fin de handleStatus

// Exécute une commande sur UN moteur (en synchronisé : le moteur 1, qui
// entraîne le 2). Renvoie nullptr si tout va bien, sinon le texte de l'erreur.
const char* doCommand(Motor& m, const String& a, int pct) {
  if (a == "stop") {                                        // --- STOP ---
    haltMotor(m);                                           // Arrêt immédiat
    return nullptr;                                         // OK
  }
  if (a == "up" || a == "down") {                           // --- MONTER / DESCENDRE ---
    if (!canMove(m)) return "non calibré : lancez d'abord la calibration";
    m.target = -1;                                          // Mouvement libre jusqu'à Stop ou butée
    m.dir    = (a == "up") ? -1 : +1;                       // Choix du sens
    return nullptr;                                         // OK
  }
  if (a == "goto") {                                        // --- ALLER À X % ---
    if (m.cal || m.travel <= 0) return "non calibré ou calibration en cours";
    m.dir = 0;                                              // Fige le moteur le temps du calcul (position stable)
    long target = (m.travel * pct + 50) / 100;              // Conversion en pas
    if (target == m.pos) { haltMotor(m); return nullptr; }  // Déjà à la bonne position
    m.target = target;                                      // Mémorise la cible AVANT de lancer le moteur
    m.dir    = (target > m.pos) ? +1 : -1;                  // Sens nécessaire pour l'atteindre
    return nullptr;                                         // OK
  }
  if (a == "calstart") {                                    // --- DÉBUT DE CALIBRATION ---
    haltMotor(m);                                           // Arrêt par sécurité
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

// Alignement (synchronisé) : un seul moteur tourne, store à l'arrêt.
const char* doJog(int mot, long steps) {
  if (!syncMode)            return "l'alignement ne sert qu'en mode synchronisé";
  if (mot < 1 || mot > 2)   return "choisissez un moteur";
  if (motors[0].dir != 0)   return "arrêtez le store avant d'aligner";
  if (steps == 0)           return "déplacement nul";
  Motor& m = motors[mot - 1];                               // Moteur à aligner
  long j = m.jog + steps;                                   // Ajouté à ce qui reste éventuellement à faire
  if (j >  JOG_MAX) j =  JOG_MAX;                           // Limite (1 tour)
  if (j < -JOG_MAX) j = -JOG_MAX;
  m.jog = j;                                                // Le temporisateur s'en charge
  Serial.printf("[ALIGN] %s : %ld demi-pas\n", m.name, j);  // Débogage
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

  if (syncMode) mirrorFollower(a == "calbot");              // Synchronisés : le moteur 2 suit (course enregistrée en fin de calibration)

  if (okCount == 0)              server.send(409, "text/plain", errors);   // Tout refusé (409 = conflit)
  else                           server.send(200, "text/plain", errors);   // Accepté (message vide, ou refus partiel)
}                                                           // Fin de handleCmd

String setSyncMode(bool s) {                                // Passage indépendants ↔ synchronisés
  if (NB_MOTORS < 2) return "Un seul moteur : pas de fonctionnement synchronisé";
  if (s == syncMode) return "";                             // Déjà dans ce mode
  haltMotor(motors[0]);                                     // Arrêt des deux moteurs par sécurité
  haltMotor(motors[1]);
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

  if (server.hasArg("per")) {                               // --- VITESSE ---
    setPeriod(m, server.arg("per").toInt());                // Appliquée tout de suite, même en mouvement (la rampe suit)
  }

  if (server.hasArg("name")) {                              // --- NOM ---
    setName(m, server.arg("name"));                         // Nom nettoyé (vide = nom par défaut)
    reply = String("Nom enregistré : ") + m.name;
  }

  if (server.hasArg("inv")) {                               // --- SENS ---
    bool inv = server.arg("inv").toInt() != 0;              // Sens demandé
    if (inv != m.invert) {                                  // Changement réel
      haltMotor(syncMode ? motors[0] : m);                  // Arrêt par sécurité (les deux en synchronisé)
      m.invert = inv;                                       // Nouveau sens
      if (syncMode) {                                       // Synchronisés : la calibration commune n'est plus valable
        clearCalibration(motors[0]);
        clearCalibration(motors[1]);
        reply = String(m.name) + " : sens inversé, refaites la calibration (commune aux deux moteurs)";
      } else {                                              // Indépendants : seulement ce moteur
        clearCalibration(m);
        reply = String(m.name) + " : sens inversé, refaites sa calibration";
      }
    }
  }

  saveSettings(m);                                          // Enregistre sens, vitesse et nom en NVS
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
    m.totalSteps = 0;                                       // …puis repart de zéro
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
  j += ",\"spr\":";   j += STEPS_PER_REV;
  j += ",\"m\":[";
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Un bloc par moteur
    Motor& m = motors[i];
    accumulateCounters(m);                                  // Compteurs à jour (même en mouvement)
    if (i) j += ",";
    j += "{\"name\":\""; j += m.name; j += "\"";
    j += ",\"moves\":";  j += m.totalMoves;
    j += ",\"steps\":";  j += m.totalSteps;
    j += ",\"travel\":"; j += (long)m.travel;
    j += "}";
  }
  j += "]}";
  server.send(200, "application/json", j);                  // Envoi de la réponse
}                                                           // Fin de handleInfo

// ======================================================
// WIFI
// ======================================================
void startMDNS() {                                          // Nom « store.local » et annonce des services
  if (MDNS.begin(HOSTNAME)) {                               // Active le nom « store.local »
    MDNS.addService("http", "tcp", 80);                     // Annonce le serveur web
    MDNS.enableArduino(OTA_PORT, true);                     // Annonce l'OTA : la carte apparaît dans Outils > Port
    Serial.printf("[WIFI] Page web : http://%s.local\n", HOSTNAME);   // Débogage
  }                                                         // Fin du test mDNS
}                                                           // Fin de startMDNS

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
    WiFi.setAutoReconnect(false);                           // Pas de recherche permanente (perturberait le point d'accès)
    WiFi.disconnect(false);                                 // Arrête l'essai en cours, Wi-Fi laissé allumé
    WiFi.mode(WIFI_AP_STA);                                 // Point d'accès + client (pour réessayer la box plus tard)
    WiFi.setSleep(false);                                   // Pas de veille Wi-Fi
    WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);               // Fixe l'adresse : IP = passerelle = 192.168.5.1
    WiFi.softAP(AP_SSID, AP_PASS);                          // Crée le réseau de secours
    WiFi.setTxPower(WIFI_POWER_8_5dBm);                     // Puissance réduite (même raison qu'en mode box)
    apMode = true;                                          // Mémorise le mode secours
    lastStaTry = millis();                                  // Prochain essai vers la box dans 2 min
    Serial.printf("[WIFI] Réseau « %s », IP : %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());   // Infos
  }                                                         // Fin du test

  startMDNS();                                              // Nom « store.local »
}                                                           // Fin de startWiFi

// En point d'accès de secours : réessaie régulièrement la box, et bascule dessus dès qu'elle répond.
// Typiquement après une coupure de courant : la carte redémarre avant que la box soit prête.
void checkWiFi() {
  if (!apMode) return;                                      // Connecté à la box : rien à faire (reconnexion automatique)

  if (WiFi.status() == WL_CONNECTED) {                      // La box a répondu
    Serial.printf("[WIFI] Box retrouvée, IP : %s → arrêt du point d'accès\n",
                  WiFi.localIP().toString().c_str());       // Débogage
    WiFi.softAPdisconnect(true);                            // Coupe le point d'accès de secours
    WiFi.mode(WIFI_STA);                                    // Mode client seul
    WiFi.setAutoReconnect(true);                            // Reconnexion automatique à nouveau active
    WiFi.setTxPower(WIFI_POWER_8_5dBm);                     // Puissance réduite (conservée)
    apMode = false;                                         // Fin du mode secours
    MDNS.end();                                             // Relance mDNS sur le réseau de la box
    startMDNS();
    return;
  }

  if (millis() - lastStaTry < STA_RETRY_MS) return;         // Pas encore l'heure du prochain essai
  lastStaTry = millis();                                    // Mémorise l'instant de l'essai
  if (WiFi.softAPgetStationNum() > 0) return;               // Un téléphone utilise le point d'accès : on ne le dérange pas
  Serial.println("[WIFI] Nouvel essai de connexion à la box");   // Débogage
  WiFi.begin(WIFI_SSID, WIFI_PASS);                         // Essai (le résultat est vu au prochain passage)
}                                                           // Fin de checkWiFi

// ======================================================
// INITIALISATION / BOUCLE PRINCIPALE
// ======================================================
void setup() {                                              // Exécutée une fois au démarrage
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Pour chaque moteur utilisé
    for (uint8_t b = 0; b < 4; b++) pinMode(motors[i].pins[b], OUTPUT);   // Ses 4 broches en sortie
    releaseCoils(motors[i]);                                // Moteur hors tension dès le démarrage
  }                                                         // Fin de la boucle

  Serial.begin(115200);                                     // Port série (USB) à 115200 bauds
#if ARDUINO_USB_CDC_ON_BOOT                                 // Port série par l'USB intégré du C3
  Serial.setTxTimeoutMs(0);                                 // Ne bloque jamais si aucun moniteur série ne lit
#endif                                                      // Fin du test
  delay(500);                                               // Laisse le temps à l'USB de démarrer
  Serial.printf("\n=== XIAO ESP32-C3 Stores enrouleurs (web) v%s ===\n", FW_VERSION);   // Bannière
  Serial.printf("[INFO] Démarrage : %s\n", resetReasonText());   // Cause du démarrage

  loadSettings();                                           // Recharge mode, calibrations, positions, réglages, compteurs
  saveAllAtBoot();                                          // Écrit les clés propres à chaque moteur (reprise v3.1)

  const esp_timer_create_args_t args = {                    // Paramètres du temporisateur
    .callback = &stepperTick,                               // Fonction appelée à chaque échéance
    .arg = nullptr,                                         // Pas d'argument
    .dispatch_method = ESP_TIMER_TASK,                      // Exécutée dans la tâche dédiée aux temporisateurs
    .name = "stepper",                                      // Nom (pour le débogage)
    .skip_unhandled_events = true                           // Après un retard (écriture flash), pas de rafale de pas
  };                                                        // Fin des paramètres
  esp_timer_create(&args, &stepTimer);                      // Crée le temporisateur
  esp_timer_start_periodic(stepTimer, TICK_US);             // Le lance : un top toutes les 0,5 ms

  startWiFi();                                              // Connexion Wi-Fi
  startArduinoOTA();                                        // Mise à jour depuis l'Arduino IDE

  server.on("/", handleRoot);                               // Page principale
  server.on("/api/status", handleStatus);                   // État au format JSON
  server.on("/api/cmd", handleCmd);                         // Commandes des moteurs
  server.on("/api/set", handleSet);                         // Réglages (mode, sens, vitesse, nom)
  server.on("/info", handleInfoPage);                       // Page d'informations
  server.on("/icon.png", handleIcon);                       // Icône (onglet et raccourci du téléphone)
  server.on("/manifest.json", handleManifest);              // Nom et icône du raccourci
  server.on("/api/info", handleInfo);                       // Informations au format JSON
  server.on("/update", HTTP_GET, handleUpdatePage);         // Page de mise à jour
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);   // Réception du programme
  server.begin();                                           // Démarre le serveur web
  Serial.println("[WEB] Serveur démarré");                  // Débogage
}                                                           // Fin de setup

void loop() {                                               // Exécutée en boucle
  server.handleClient();                                    // Traite les requêtes des navigateurs
  ArduinoOTA.handle();                                      // Surveille une mise à jour venant de l'IDE
  checkWiFi();                                              // Mode secours : retour vers la box dès que possible

  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Chaque moteur est surveillé séparément
    Motor& m = motors[i];                                   // Raccourci

    if (m.stopEvent) {                                      // Ce moteur vient de s'arrêter
      m.stopEvent = false;                                  // Acquitte l'événement
      Serial.printf("[MOTEUR] %s : ARRÊT à %ld pas (%d %%)\n", m.name, (long)m.pos, positionPercent(m));   // Débogage
      if (!inCal(m)) savePosition(m);                       // Mémorise sa position (sauf en calibration)
      saveCounters(m);                                      // Mémorise ses compteurs (une écriture par mouvement)
    }                                                       // Fin du test

    if (isMoving(m) && !inCal(m)) {                         // Moteur en mouvement (hors calibration)
      long pos = m.pos;                                     // Copie locale de la position
      if (labs(pos - m.lastSaved) >= SAVE_EVERY_STEPS) {    // Assez de chemin parcouru depuis la dernière sauvegarde
        savePosition(m);                                    // Sauvegarde intermédiaire (protège contre une coupure)
      }                                                     // Fin du test
    }                                                       // Fin du test
  }                                                         // Fin de la boucle
}                                                           // Fin de loop
