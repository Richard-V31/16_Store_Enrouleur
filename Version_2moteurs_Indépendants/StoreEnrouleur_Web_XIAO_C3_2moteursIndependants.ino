// ======================================================
// XIAO ESP32-C3 : Commande de stores enrouleurs par page web
// Version autonome, sans Blynk — DEUX MOTEURS INDÉPENDANTS
// v2 : sauvegarde de la position pendant le mouvement + mise à jour OTA
// v3 : deuxième moteur sur 4 GPIO séparés + alignement des deux côtés
// v3.1 : OTA réparée (puissance Wi-Fi réduite, veille Wi-Fi désactivée,
//        port série non bloquant), page /update avec le seul mot de passe OTA,
//        affichage de la force du signal Wi-Fi
// v4 : les deux moteurs deviennent indépendants : position, sens, butées,
//      calibration et sauvegarde propres à chacun. Commandes « les deux »
//      conservées pour piloter les deux moteurs d'un seul clic.
//      L'alignement disparaît (chaque moteur se règle maintenant seul).
// ------------------------------------------------------
// Carte Arduino IDE : « XIAO_ESP32C3 » (paquet esp32 d'Espressif)
// Pour voir le moniteur série : Outils > USB CDC On Boot > Enabled
//   (obligatoire ici : D6/D7 servent au moteur 2, le moniteur passe par l'USB)
// Pour l'OTA : Outils > Partition Scheme > un schéma avec OTA (celui par défaut convient)
// Page web : http://store.local  ou l'adresse IP affichée sur le port série
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
// ------------------------------------------------------
// Passage depuis la v3.1 : au premier démarrage, la calibration et la
// position communes de la v3.1 sont reprises pour chacun des deux moteurs.
// Aucune recalibration n'est donc nécessaire.
// ======================================================
#include "arduino_secrets.h"   // Fichier séparé contenant le nom du réseau (SSID) et le mot de passe WiFi

#include <WiFi.h>          // Bibliothèque Wi-Fi de l'ESP32
#include <WebServer.h>     // Serveur web intégré au paquet ESP32 (aucune bibliothèque à installer)
#include <ESPmDNS.h>       // Permet d'accéder à la carte par le nom « store.local »
#include <Preferences.h>   // Stockage en mémoire non volatile (NVS)
#include <ArduinoOTA.h>    // Mise à jour par le réseau depuis l'Arduino IDE (intégré au paquet ESP32)
#include <Update.h>        // Écriture d'un nouveau programme en flash (mise à jour par le navigateur)
#include "esp_timer.h"     // Temporisateur matériel de l'ESP32 (moteurs indépendants du réseau)

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

// ----------- BROCHES DES MOTEURS (XIAO ESP32-C3) -----------
const uint8_t M1_PINS[4] = { D2, D3, D4, D5 };    // Moteur 1 : GPIO4, 5, 6, 7 → IN1 à IN4 du driver n°1
const uint8_t M2_PINS[4] = { D1, D6, D7, D10 };   // Moteur 2 : GPIO3, 21, 20, 10 → IN1 à IN4 du driver n°2
//                           IN1 IN2 IN3 IN4
// ----------- RÉGLAGES DES MOTEURS -----------
// Si un moteur tourne dans le mauvais sens, inutile de recâbler :
// mettre MOT1_INVERT ou MOT2_INVERT à true dans le programme.
const bool MOTOR2_ENABLED = true;       // false = fonctionnement avec un seul moteur
const bool MOT1_INVERT    = false;      // true = inverse le sens du moteur 1
const bool MOT2_INVERT    = false;      // true = inverse le sens du moteur 2
const char* MOT1_NAME     = "Store 1"; // Nom affiché sur la page web (ex. « Store gauche »)
const char* MOT2_NAME     = "Store 2"; // Nom affiché sur la page web (ex. « Store droit »)
const uint32_t STEP_PERIOD_US = 3000;   // Durée d'un demi-pas en microsecondes (3 ms, comme l'original)
const long SAVE_EVERY_STEPS   = 500;    // Sauvegarde pendant le mouvement tous les 500 pas (≈ 1,5 s)
const uint8_t NB_MOTORS = MOTOR2_ENABLED ? 2 : 1;   // Nombre de moteurs réellement utilisés

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
// Les variables partagées avec le temporisateur sont « volatile ».
struct Motor {
  const uint8_t* pins;            // Ses 4 broches (IN1 à IN4)
  bool invert;                    // Sens inversé ?
  const char* name;               // Nom affiché sur la page web
  const char* keyTravel;          // Clé NVS de sa course totale
  const char* keyPos;             // Clé NVS de sa dernière position
  volatile int  dir       = 0;    // Sens : +1 = descente, -1 = montée, 0 = arrêt
  volatile long pos       = 0;    // Position actuelle en pas (0 = tout en haut)
  volatile long travel    = 0;    // Course totale haut→bas en pas (0 = non calibré)
  volatile long target    = -1;   // Position cible pour « aller à X % » (-1 = pas de cible)
  volatile bool cal       = false;// Vrai pendant sa calibration (butées désactivées)
  volatile bool stopEvent = false;// Signale au programme principal qu'il vient de s'arrêter
  bool calTop             = false;// Vrai quand sa position haute a été validée
  uint8_t phase           = 0;    // Phase actuelle (utilisée uniquement par le temporisateur)
  long lastSaved          = 0;    // Dernière position écrite en NVS (évite les écritures inutiles)
};

Motor motors[2] = {                                         // Les deux moteurs
  { M1_PINS, MOT1_INVERT, MOT1_NAME, "travel1", "pos1" },   // Moteur 1
  { M2_PINS, MOT2_INVERT, MOT2_NAME, "travel2", "pos2" }    // Moteur 2
};                                                          // Fin du tableau

volatile bool otaBusy = false;          // Vrai pendant une mise à jour : commandes des moteurs refusées
bool updateRefused = false;             // Vrai si l'envoi par /update n'a pas le bon mot de passe

// ----------- OBJETS -----------
WebServer server(80);              // Serveur web sur le port 80 (port HTTP standard)
Preferences prefs;                 // Accès à la mémoire non volatile
esp_timer_handle_t stepTimer;      // Poignée du temporisateur qui fait tourner les moteurs

const char* PREF_NAMESPACE = "roller";        // Espace de noms dans la NVS (le même qu'en v3.1)
const char* OLD_KEY_TRAVEL = "travelSteps";   // Ancienne clé v3.1 : course commune (reprise au 1er démarrage)
const char* OLD_KEY_POS    = "position";      // Ancienne clé v3.1 : position commune (reprise au 1er démarrage)

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
  releaseCoils(m);             // Coupe ses bobines (le moteur ne chauffe pas à l'arrêt)
  m.stopEvent = true;          // Prévient la boucle principale (affichage + sauvegarde)
}                              // Fin de haltMotor

void tickMotor(Motor& m) {                                  // Un pas de temps pour UN moteur
  int dir = m.dir;                                          // Copie locale du sens
  if (dir == 0) return;                                     // Moteur à l'arrêt : rien à faire

  if (!m.cal) {                                             // Butées actives seulement hors calibration
    if (dir < 0 && m.pos <= 0)        { haltMotor(m); return; }   // Butée HAUTE atteinte
    if (dir > 0 && m.pos >= m.travel) { haltMotor(m); return; }   // Butée BASSE atteinte
  }                                                         // Fin du test des butées

  if (m.target >= 0 && m.pos == m.target) { haltMotor(m); return; }   // Position demandée atteinte

  stepMotor(m, dir);                                        // Un demi-pas
  m.pos = m.pos + dir;                                      // Met à jour sa position
}                                                           // Fin de tickMotor

// Appelée toutes les 3 ms par le temporisateur matériel, même si le Wi-Fi est occupé
void stepperTick(void* arg) {
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Chaque moteur est traité séparément
    tickMotor(motors[i]);                                   // Avance (ou non) selon son propre état
  }                                                         // Fin de la boucle
}                                                           // Fin de stepperTick

int positionPercent(const Motor& m) {                       // Position d'un moteur en %
  if (m.travel <= 0) return 0;                              // Non calibré : 0 %
  long pct = (100L * m.pos) / m.travel;                     // Calcul du pourcentage
  return constrain(pct, 0, 100);                            // Limité entre 0 et 100
}                                                           // Fin de positionPercent

bool canMove(const Motor& m) {                              // Ce moteur a-t-il le droit de bouger ?
  return m.cal || m.travel > 0;                             // Oui si calibré, ou si sa calibration est en cours
}                                                           // Fin de canMove

// ======================================================
// NVS (MÉMOIRE NON VOLATILE)
// ======================================================
void loadSettings() {                                       // Relit la calibration et la position de chaque moteur
  prefs.begin(PREF_NAMESPACE, true);                        // Ouverture en lecture seule
  long oldTravel = prefs.getLong(OLD_KEY_TRAVEL, 0);        // Course commune de la v3.1 (0 si absente)
  long oldPos    = prefs.getLong(OLD_KEY_POS, 0);           // Position commune de la v3.1 (0 si absente)
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
    Serial.printf("[NVS] %s : course = %ld pas, position = %ld\n", m.name, travel, pos);   // Débogage
  }                                                         // Fin de la boucle
  prefs.end();                                              // Fermeture
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

void saveAllAtBoot() {                                      // Écrit les clés propres à chaque moteur dès le démarrage
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Pour chaque moteur
    saveTravel(motors[i]);                                  // Course sous sa propre clé
    savePosition(motors[i]);                                // Position sous sa propre clé
  }                                                         // Fin de la boucle
}                                                           // Fin de saveAllAtBoot

// ======================================================
// PAGE WEB (HTML + CSS + JavaScript stockés en mémoire flash)
// Les cartes des moteurs sont construites par le JavaScript
// à partir de la réponse de /api/status (une carte par moteur).
// ======================================================
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stores</title>
<style>
 body{font-family:sans-serif;background:#1e2430;color:#eee;margin:0;padding:16px;max-width:420px;margin:auto}
 h1{font-size:1.4em;text-align:center}
 h2{font-size:1.15em;margin:0 0 8px}
 .card{background:#2b3240;border-radius:12px;padding:14px;margin-bottom:14px}
 .bar{height:22px;background:#444;border-radius:11px;overflow:hidden;margin-top:4px}
 .fill{height:100%;background:#4caf50;width:0%;transition:width .3s}
 .row{display:flex;gap:8px;margin-top:10px}
 button{flex:1;padding:16px 0;font-size:1.1em;border:0;border-radius:10px;background:#3d6fb6;color:#fff}
 button:active{filter:brightness(1.3)}
 .stop{background:#c0392b}.cal{background:#7a5bb5;font-size:.95em}.both{background:#2e8b7a}
 input[type=range]{width:100%}
 summary{cursor:pointer;margin-top:10px;color:#ccc}
 #msg{color:#ffb347;min-height:1.2em;text-align:center}
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

<div id="msg"></div>
<p style="text-align:center"><small><a href="/update">Mise à jour du programme</a></small></p>

<script>
let built=false;                                           // Cartes des moteurs déjà construites ?
function $(id){return document.getElementById(id);}       // Raccourci
function build(s){                                         // Construit une carte par moteur
 let h='';
 s.m.forEach((m,i)=>{let k=i+1;h+=`
<div class="card">
 <h2>${m.name}</h2>
 <div>Position : <b id="pct${k}">--</b> % <small>(0 % = haut, 100 % = bas)</small></div>
 <div class="bar"><div class="fill" id="fill${k}"></div></div>
 <div style="margin-top:6px">État : <b id="etat${k}">--</b></div>
 <div class="row">
  <button onclick="cmd('up',${k})">▲ Monter</button>
  <button class="stop" onclick="cmd('stop',${k})">■ Stop</button>
  <button onclick="cmd('down',${k})">▼ Descendre</button>
 </div>
 <div style="margin-top:10px">Aller à : <b id="gv${k}">50</b> %</div>
 <input type="range" id="goto${k}" min="0" max="100" value="50" oninput="$('gv${k}').textContent=this.value">
 <div class="row"><button onclick="cmd('goto',${k},$('goto${k}').value)">Aller</button></div>
 <details><summary>Calibration</summary>
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
 $('motors').innerHTML=h;                                  // Insère les cartes dans la page
 if(s.m.length>1)$('all').style.display='block';           // Carte « les deux » seulement s'il y a 2 moteurs
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
  $('conn').textContent=s.ota?'Mise à jour en cours':'OK'; // État de la connexion
  $('rssi').textContent=s.rssi?s.rssi:'--';                // Force du signal Wi-Fi
  s.m.forEach((m,i)=>{let k=i+1;                           // Pour chaque moteur
   $('pct'+k).textContent=m.ready?m.pct:'--';              // Pourcentage
   $('fill'+k).style.width=(m.ready?m.pct:0)+'%';          // Barre de progression
   $('travel'+k).textContent=m.travel;                     // Course enregistrée
   let e=m.dir>0?'Descente':m.dir<0?'Montée':'Arrêt';      // Sens de rotation en texte
   if(s.ota)e='Mise à jour en cours';                      // Mise à jour du programme
   else if(m.cal)e+=' — calibration en cours'+(m.top?' (haut validé)':'');   // Calibration
   else if(!m.ready)e+=' — non calibré';                   // Jamais calibré
   $('etat'+k).textContent=e;                              // Affichage de l'état
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
    if (m.cal) { m.cal = false; m.calTop = false; }         // Calibration inachevée : abandonnée (position non sauvée)
    else savePosition(m);                                   // Sinon, la position est conservée pour le redémarrage
  }                                                         // Fin de la boucle
  esp_timer_stop(stepTimer);                                // Suspend le temporisateur : tout le processeur pour l'OTA
}                                                           // Fin de prepareUpdate

void cancelUpdate() {                                       // Mise à jour échouée : retour au fonctionnement normal
  otaBusy = false;                                          // Les moteurs redeviennent commandables
  esp_timer_start_periodic(stepTimer, STEP_PERIOD_US);      // Relance le temporisateur des moteurs
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
void handleRoot() {                                         // Adresse « / » : envoie la page
  server.send_P(200, "text/html", PAGE_HTML);               // Code 200 = OK, contenu HTML
}                                                           // Fin de handleRoot

void handleStatus() {                                       // Adresse « /api/status » : état en JSON
  char json[600];                                           // Tampon pour le texte JSON
  int n = snprintf(json, sizeof(json),                      // Début du JSON : informations générales
    "{\"ota\":%s,\"rssi\":%d,\"m\":[",
    otaBusy ? "true" : "false",                             // Mise à jour en cours ?
    (int)WiFi.RSSI());                                      // Force du signal de la box (0 en point d'accès)
  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Un bloc par moteur
    Motor& m = motors[i];                                   // Raccourci
    n += snprintf(json + n, sizeof(json) - n,
      "%s{\"name\":\"%s\",\"pct\":%d,\"step\":%ld,\"travel\":%ld,\"dir\":%d,\"cal\":%s,\"top\":%s,\"ready\":%s}",
      i ? "," : "",                                         // Virgule entre les moteurs
      m.name, positionPercent(m), (long)m.pos, (long)m.travel, (int)m.dir,
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
    long target = (m.travel * pct) / 100;                   // Conversion en pas
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
  }                                                         // Fin du test

  if (MDNS.begin(HOSTNAME)) {                               // Active le nom « store.local »
    MDNS.addService("http", "tcp", 80);                     // Annonce le serveur web
    MDNS.enableArduino(OTA_PORT, true);                     // Annonce l'OTA : la carte apparaît dans Outils > Port
    Serial.printf("[WIFI] Page web : http://%s.local\n", HOSTNAME);   // Débogage
  }                                                         // Fin du test mDNS
}                                                           // Fin de startWiFi

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
  Serial.println("\n=== XIAO ESP32-C3 Stores enrouleurs (web) v4 - 2 moteurs indépendants ===");   // Bannière

  loadSettings();                                           // Recharge calibrations et positions
  saveAllAtBoot();                                          // Écrit les clés propres à chaque moteur (reprise v3.1)

  const esp_timer_create_args_t args = {                    // Paramètres du temporisateur
    .callback = &stepperTick,                               // Fonction appelée à chaque échéance
    .arg = nullptr,                                         // Pas d'argument
    .dispatch_method = ESP_TIMER_TASK,                      // Exécutée dans la tâche dédiée aux temporisateurs
    .name = "stepper",                                      // Nom (pour le débogage)
    .skip_unhandled_events = true                           // Après un retard (écriture flash), pas de rafale de pas
  };                                                        // Fin des paramètres
  esp_timer_create(&args, &stepTimer);                      // Crée le temporisateur
  esp_timer_start_periodic(stepTimer, STEP_PERIOD_US);      // Le lance : un appel toutes les 3 ms

  startWiFi();                                              // Connexion Wi-Fi
  startArduinoOTA();                                        // Mise à jour depuis l'Arduino IDE

  server.on("/", handleRoot);                               // Page principale
  server.on("/api/status", handleStatus);                   // État au format JSON
  server.on("/api/cmd", handleCmd);                         // Commandes des moteurs
  server.on("/update", HTTP_GET, handleUpdatePage);         // Page de mise à jour
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);   // Réception du programme
  server.begin();                                           // Démarre le serveur web
  Serial.println("[WEB] Serveur démarré");                  // Débogage
}                                                           // Fin de setup

void loop() {                                               // Exécutée en boucle
  server.handleClient();                                    // Traite les requêtes des navigateurs
  ArduinoOTA.handle();                                      // Surveille une mise à jour venant de l'IDE

  for (uint8_t i = 0; i < NB_MOTORS; i++) {                 // Chaque moteur est surveillé séparément
    Motor& m = motors[i];                                   // Raccourci

    if (m.stopEvent) {                                      // Ce moteur vient de s'arrêter
      m.stopEvent = false;                                  // Acquitte l'événement
      Serial.printf("[MOTEUR] %s : ARRÊT à %ld pas (%d %%)\n", m.name, (long)m.pos, positionPercent(m));   // Débogage
      if (!m.cal) savePosition(m);                          // Mémorise sa position (sauf en calibration)
    }                                                       // Fin du test

    if (m.dir != 0 && !m.cal) {                             // Moteur en mouvement (hors calibration)
      long pos = m.pos;                                     // Copie locale de la position
      if (labs(pos - m.lastSaved) >= SAVE_EVERY_STEPS) {    // Assez de chemin parcouru depuis la dernière sauvegarde
        savePosition(m);                                    // Sauvegarde intermédiaire (protège contre une coupure)
      }                                                     // Fin du test
    }                                                       // Fin du test
  }                                                         // Fin de la boucle
}                                                           // Fin de loop
