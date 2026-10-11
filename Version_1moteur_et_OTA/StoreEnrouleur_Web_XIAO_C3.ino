// ======================================================
// XIAO ESP32-C3 : Commande de store enrouleur par page web
// Version autonome, sans Blynk
// ------------------------------------------------------
// Carte Arduino IDE : « XIAO_ESP32C3 » (paquet esp32 d'Espressif)
// Pour voir le moniteur série : Outils > USB CDC On Boot > Enabled
// Page web : http://store.local  ou l'adresse IP affichée sur le port série
// Si le Wi-Fi de la box est introuvable : réseau « Store-ESP32 »,
// mot de passe « store1234 », puis http://192.168.5.1
// ======================================================
#include "arduino_secrets.h"   // Fichier séparé contenant le nom du réseau (SSID) et le mot de passe WiFi

#include <WiFi.h>          // Bibliothèque Wi-Fi de l'ESP32
#include <WebServer.h>     // Serveur web intégré au paquet ESP32 (aucune bibliothèque à installer)
#include <ESPmDNS.h>       // Permet d'accéder à la carte par le nom « store.local »
#include <Preferences.h>   // Stockage en mémoire non volatile (NVS)
#include "esp_timer.h"     // Temporisateur matériel de l'ESP32 (moteur indépendant du réseau)

// ----------- WIFI -----------
char WIFI_SSID[] = SECRET_SSID;              // Nom du réseau Wi-Fi de la box (à remplacer)
char WIFI_PASS[] = SECRET_PASS;             // Mot de passe du Wi-Fi (à remplacer)
char AP_SSID[] = SECRET_AP_SSID;            // Nom du point d'accès de secours créé par la carte
char AP_PASS[]  = SECRET_AP_PASS;           // Mot de passe du point d'accès (8 caractères minimum)
const char* HOSTNAME  = "store";            // Nom réseau : la page sera sur http://store.local
const IPAddress AP_IP(192, 168, 5, 1);       // Adresse IP de la carte en mode point d'accès local
const IPAddress AP_MASK(255, 255, 255, 0);  // Masque de sous-réseau du point d'accès

// ----------- BROCHES DU MOTEUR (XIAO ESP32-C3) -----------
#define IN1 D2   // D2 = GPIO4 → entrée IN1 du driver ULN2003
#define IN2 D3   // D3 = GPIO5 → entrée IN2 du driver
#define IN3 D4   // D4 = GPIO6 → entrée IN3 du driver
#define IN4 D5   // D5 = GPIO7 → entrée IN4 du driver

// ----------- RÉGLAGES DU MOTEUR -----------
const uint32_t STEP_PERIOD_US = 3000;   // Durée d'un demi-pas en microsecondes (3 ms, comme l'original)

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

// Variables partagées entre le temporisateur et le programme principal → « volatile »
volatile int  direction       = 0;      // Sens : +1 = descente, -1 = montée, 0 = arrêt
volatile long currentStep     = 0;      // Position actuelle en pas (0 = tout en haut)
volatile long fullTravelSteps = 0;      // Course totale haut→bas en pas (0 = non calibré)
volatile long targetStep      = -1;     // Position cible pour « aller à X % » (-1 = pas de cible)
volatile bool calibrating     = false;  // Vrai pendant une calibration (butées désactivées)
volatile bool stopEvent       = false;  // Signale au programme principal qu'un arrêt a eu lieu
bool calTopDone = false;                // Vrai quand la position haute a été validée pendant la calibration
int  stepIndex  = 0;                    // Indice de la phase actuelle (utilisé uniquement par le temporisateur)

// ----------- OBJETS -----------
WebServer server(80);              // Serveur web sur le port 80 (port HTTP standard)
Preferences prefs;                 // Accès à la mémoire non volatile
esp_timer_handle_t stepTimer;      // Poignée du temporisateur qui fait tourner le moteur

const char* PREF_NAMESPACE = "roller";        // Espace de noms dans la NVS
const char* KEY_TRAVEL     = "travelSteps";   // Clé : course totale
const char* KEY_POS        = "position";      // Clé : dernière position connue (conservée après coupure)

// ======================================================
// FONCTIONS DU MOTEUR
// ======================================================
void setCoils(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {   // Applique un état aux 4 bobines
  digitalWrite(IN1, a);   // Bobine A
  digitalWrite(IN2, b);   // Bobine B
  digitalWrite(IN3, c);   // Bobine C
  digitalWrite(IN4, d);   // Bobine D
}                         // Fin de setCoils

void haltMotor() {        // Arrêt immédiat (utilisable depuis le temporisateur)
  direction  = 0;         // Plus de mouvement
  targetStep = -1;        // Annule une éventuelle cible
  setCoils(0,0,0,0);      // Coupe les bobines (le moteur ne chauffe pas à l'arrêt)
  stopEvent  = true;      // Prévient la boucle principale (affichage + sauvegarde)
}                         // Fin de haltMotor

// Appelée toutes les 3 ms par le temporisateur matériel, même si le Wi-Fi est occupé
void stepperTick(void* arg) {
  int dir = direction;                                      // Copie locale du sens (évite qu'il change en cours de route)
  if (dir == 0) return;                                     // Moteur à l'arrêt : rien à faire

  if (!calibrating) {                                       // Butées actives seulement hors calibration
    if (dir < 0 && currentStep <= 0)               { haltMotor(); return; }   // Butée HAUTE atteinte
    if (dir > 0 && currentStep >= fullTravelSteps) { haltMotor(); return; }   // Butée BASSE atteinte
  }                                                         // Fin du test des butées

  if (targetStep >= 0 && currentStep == targetStep) { haltMotor(); return; }  // Position demandée atteinte

  stepIndex = (stepIndex + dir + STEP_COUNT) % STEP_COUNT;  // Phase suivante (ou précédente), entre 0 et 7
  setCoils(stepSequence[stepIndex][0],                      // Applique la phase : bobine A
           stepSequence[stepIndex][1],                      //                     bobine B
           stepSequence[stepIndex][2],                      //                     bobine C
           stepSequence[stepIndex][3]);                     //                     bobine D
  currentStep += dir;                                       // Met à jour la position
}                                                           // Fin de stepperTick

int positionPercent() {                                     // Position actuelle en %
  if (fullTravelSteps <= 0) return 0;                       // Non calibré : 0 %
  long pct = (100L * currentStep) / fullTravelSteps;        // Calcul du pourcentage
  return constrain(pct, 0, 100);                            // Limité entre 0 et 100
}                                                           // Fin de positionPercent

bool canMove() {                                            // Le moteur a-t-il le droit de bouger ?
  return calibrating || fullTravelSteps > 0;                // Oui si calibré, ou si une calibration est en cours
}                                                           // Fin de canMove

// ======================================================
// NVS (MÉMOIRE NON VOLATILE)
// ======================================================
void loadSettings() {                                       // Relit la calibration et la position
  prefs.begin(PREF_NAMESPACE, true);                        // Ouverture en lecture seule
  long travel = prefs.getLong(KEY_TRAVEL, 0);               // Course totale (0 si absente), variable locale non volatile
  long pos    = prefs.getLong(KEY_POS, 0);                  // Dernière position (0 si absente)
  prefs.end();                                              // Fermeture

  if (travel < 0) travel = 0;                               // Sécurité : course jamais négative
  if (pos < 0) pos = 0;                                     // Sécurité : position jamais au-dessus du haut
  if (pos > travel) pos = travel;                           // Sécurité : position jamais au-delà du bas
  fullTravelSteps = travel;                                 // Copie dans la variable partagée
  currentStep     = pos;                                    // Copie dans la variable partagée
  Serial.printf("[NVS] Course = %ld pas, position = %ld\n", fullTravelSteps, currentStep);   // Débogage
}                                                           // Fin de loadSettings

void savePosition() {                                       // Enregistre la position (à chaque arrêt)
  prefs.begin(PREF_NAMESPACE, false);                       // Ouverture en écriture
  prefs.putLong(KEY_POS, currentStep);                      // Écrit la position
  prefs.end();                                              // Fermeture
}                                                           // Fin de savePosition

void saveTravel() {                                         // Enregistre la course totale
  prefs.begin(PREF_NAMESPACE, false);                       // Ouverture en écriture
  prefs.putLong(KEY_TRAVEL, fullTravelSteps);               // Écrit la course
  prefs.end();                                              // Fermeture
  Serial.printf("[NVS] Course enregistrée = %ld pas\n", fullTravelSteps);   // Débogage
}                                                           // Fin de saveTravel

// ======================================================
// PAGE WEB (HTML + CSS + JavaScript stockés en mémoire flash)
// ======================================================
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Store</title>
<style>
 body{font-family:sans-serif;background:#1e2430;color:#eee;margin:0;padding:16px;max-width:420px;margin:auto}
 h1{font-size:1.4em;text-align:center}
 .card{background:#2b3240;border-radius:12px;padding:14px;margin-bottom:14px}
 .bar{height:22px;background:#444;border-radius:11px;overflow:hidden}
 .fill{height:100%;background:#4caf50;width:0%;transition:width .3s}
 .row{display:flex;gap:8px;margin-top:10px}
 button{flex:1;padding:16px 0;font-size:1.1em;border:0;border-radius:10px;background:#3d6fb6;color:#fff}
 button:active{filter:brightness(1.3)}
 .stop{background:#c0392b}.cal{background:#7a5bb5;font-size:.95em}
 input[type=range]{width:100%}
 #msg{color:#ffb347;min-height:1.2em;text-align:center}
 small{color:#aaa}
</style></head><body>
<h1>Store enrouleur</h1>

<div class="card">
 <div>Position : <b id="pct">--</b> % <small>(0 % = haut, 100 % = bas)</small></div>
 <div class="bar"><div class="fill" id="fill"></div></div>
 <div style="margin-top:6px">État : <b id="etat">--</b></div>
</div>

<div class="card">
 <div class="row">
  <button onclick="cmd('up')">▲ Monter</button>
  <button class="stop" onclick="cmd('stop')">■ Stop</button>
  <button onclick="cmd('down')">▼ Descendre</button>
 </div>
</div>

<div class="card">
 <div>Aller à : <b id="gv">50</b> %</div>
 <input type="range" id="goto" min="0" max="100" value="50" oninput="gv.textContent=this.value">
 <div class="row"><button onclick="cmd('goto',document.getElementById('goto').value)">Aller</button></div>
</div>

<div class="card">
 <b>Calibration</b><br>
 <small>1. Démarrer &nbsp;2. Monter le store tout en haut, Stop, « Valider HAUT »
 &nbsp;3. Descendre tout en bas, Stop, « Valider BAS »</small>
 <div class="row">
  <button class="cal" onclick="cmd('calstart')">Démarrer</button>
  <button class="cal" onclick="cmd('caltop')">Valider HAUT</button>
  <button class="cal" onclick="cmd('calbot')">Valider BAS</button>
 </div>
 <div style="margin-top:8px"><small>Course : <span id="travel">--</span> pas</small></div>
</div>

<div id="msg"></div>

<script>
async function cmd(a,p){                                   // Envoie une commande à l'ESP32
 let u='/api/cmd?a='+a+(p!==undefined?'&p='+p:'');        // Construit l'adresse de la commande
 try{
  let r=await fetch(u);                                    // Envoi de la requête
  msg(r.ok?'':await r.text());                             // Affiche le message d'erreur éventuel
 }catch(e){msg('Carte injoignable');}                      // Pas de réponse
 refresh();                                                // Met à jour l'affichage tout de suite
}
function msg(t){document.getElementById('msg').textContent=t;}   // Affiche un message en bas de page
async function refresh(){                                  // Lit l'état de la carte
 try{
  let s=await (await fetch('/api/status')).json();         // Récupère l'état au format JSON
  document.getElementById('pct').textContent=s.ready?s.pct:'--';   // Pourcentage
  document.getElementById('fill').style.width=(s.ready?s.pct:0)+'%';   // Barre de progression
  document.getElementById('travel').textContent=s.travel;  // Course enregistrée
  let e=s.dir>0?'Descente':s.dir<0?'Montée':'Arrêt';       // Sens de rotation en texte
  if(s.cal)e+=' — calibration en cours'+(s.top?' (haut validé)':'');   // Indication de calibration
  else if(!s.ready)e+=' — non calibré';                    // Store jamais calibré
  document.getElementById('etat').textContent=e;           // Affichage de l'état
 }catch(e){document.getElementById('etat').textContent='Hors ligne';}   // Carte injoignable
}
setInterval(refresh,500);                                  // Rafraîchit toutes les 500 ms
refresh();                                                 // Premier affichage immédiat
</script>
</body></html>
)rawliteral";

// ======================================================
// GESTION DES REQUÊTES WEB
// ======================================================
void handleRoot() {                                         // Adresse « / » : envoie la page
  server.send_P(200, "text/html", PAGE_HTML);               // Code 200 = OK, contenu HTML
}                                                           // Fin de handleRoot

void handleStatus() {                                       // Adresse « /api/status » : état en JSON
  char json[160];                                           // Tampon pour le texte JSON
  snprintf(json, sizeof(json),                              // Construit le JSON
    "{\"pct\":%d,\"step\":%ld,\"travel\":%ld,\"dir\":%d,\"cal\":%s,\"top\":%s,\"ready\":%s}",
    positionPercent(), (long)currentStep, (long)fullTravelSteps, (int)direction,
    calibrating ? "true" : "false",                         // Calibration en cours ?
    calTopDone  ? "true" : "false",                         // Haut validé ?
    fullTravelSteps > 0 ? "true" : "false");                // Store calibré ?
  server.send(200, "application/json", json);               // Envoi de la réponse
}                                                           // Fin de handleStatus

void reply(bool ok, const char* errText) {                  // Réponse courte à une commande
  if (ok) server.send(200, "text/plain", "OK");             // Commande acceptée
  else    server.send(409, "text/plain", errText);          // Commande refusée (409 = conflit) avec explication
}                                                           // Fin de reply

void handleCmd() {                                          // Adresse « /api/cmd?a=... »
  String a = server.arg("a");                               // Nom de la commande
  Serial.printf("[WEB] Commande : %s\n", a.c_str());        // Débogage

  if (a == "stop") {                                        // --- STOP ---
    haltMotor();                                            // Arrêt immédiat
    reply(true, "");                                        // OK
  }
  else if (a == "up" || a == "down") {                      // --- MONTER / DESCENDRE ---
    if (!canMove()) { reply(false, "Store non calibré : lancez d'abord la calibration"); return; }
    targetStep = -1;                                        // Mouvement libre jusqu'à Stop ou butée
    direction  = (a == "up") ? -1 : +1;                     // Choix du sens
    reply(true, "");                                        // OK
  }
  else if (a == "goto") {                                   // --- ALLER À X % ---
    if (calibrating || fullTravelSteps <= 0) { reply(false, "Impossible : store non calibré ou calibration en cours"); return; }
    int pct = constrain(server.arg("p").toInt(), 0, 100);   // Pourcentage demandé (0 à 100)
    long target = (fullTravelSteps * pct) / 100;            // Conversion en pas
    if (target == currentStep) { reply(true, ""); return; } // Déjà à la bonne position
    targetStep = target;                                    // Mémorise la cible AVANT de lancer le moteur
    direction  = (target > currentStep) ? +1 : -1;          // Sens nécessaire pour l'atteindre
    reply(true, "");                                        // OK
  }
  else if (a == "calstart") {                               // --- DÉBUT DE CALIBRATION ---
    haltMotor();                                            // Arrêt par sécurité
    calibrating = true;                                     // Butées désactivées : mouvement libre
    calTopDone  = false;                                    // Le haut n'est pas encore validé
    Serial.println("[CAL] DÉMARRÉE");                       // Débogage
    reply(true, "");                                        // OK
  }
  else if (a == "caltop") {                                 // --- VALIDER LA POSITION HAUTE ---
    if (!calibrating)   { reply(false, "Cliquez d'abord sur « Démarrer »"); return; }
    if (direction != 0) { reply(false, "Arrêtez le moteur avant de valider"); return; }
    currentStep = 0;                                        // La position actuelle devient le haut
    calTopDone  = true;                                     // Haut validé
    Serial.println("[CAL] HAUT validé");                    // Débogage
    reply(true, "");                                        // OK
  }
  else if (a == "calbot") {                                 // --- VALIDER LA POSITION BASSE ---
    if (!calibrating || !calTopDone) { reply(false, "Validez d'abord la position haute"); return; }
    if (direction != 0)  { reply(false, "Arrêtez le moteur avant de valider"); return; }
    if (currentStep <= 0) { reply(false, "Descendez le store avant de valider le bas"); return; }
    fullTravelSteps = currentStep;                          // La position actuelle devient la course totale
    calibrating = false;                                    // Fin de la calibration, butées réactivées
    calTopDone  = false;                                    // Remise à zéro de l'étape
    saveTravel();                                           // Enregistre la course
    savePosition();                                         // Enregistre la position
    Serial.println("[CAL] TERMINÉE");                       // Débogage
    reply(true, "");                                        // OK
  }
  else {                                                    // --- COMMANDE INCONNUE ---
    reply(false, "Commande inconnue");                      // Erreur
  }
}                                                           // Fin de handleCmd

// ======================================================
// WIFI
// ======================================================
void startWiFi() {                                          // Connexion à la box, sinon point d'accès
  WiFi.mode(WIFI_STA);                                      // Mode client
  WiFi.setHostname(HOSTNAME);                               // Nom de la carte sur le réseau
  // WiFi.setTxPower(WIFI_POWER_8_5dBm);                    // À décommenter si la connexion échoue (aide sur certains C3)
  WiFi.begin(WIFI_SSID, WIFI_PASS);                         // Lance la connexion
  Serial.print("[WIFI] Connexion");                         // Débogage

  unsigned long t0 = millis();                              // Instant de départ
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {   // Attente 15 s maximum
    delay(500);                                             // Pause de 500 ms
    Serial.print(".");                                      // Point de progression
  }                                                         // Fin de l'attente

  if (WiFi.status() == WL_CONNECTED) {                      // Connexion réussie
    Serial.printf("\n[WIFI] CONNECTÉ, IP : %s\n", WiFi.localIP().toString().c_str());   // Affiche l'IP
  } else {                                                  // Échec : point d'accès de secours
    Serial.println("\n[WIFI] ÉCHEC → point d'accès de secours");   // Débogage
    WiFi.mode(WIFI_AP);                                     // Mode point d'accès
    WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);               // Fixe l'adresse : IP = passerelle = 192.168.5.1
    WiFi.softAP(AP_SSID, AP_PASS);                          // Crée le réseau « Store-ESP32 »
    Serial.printf("[WIFI] Réseau « %s », IP : %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());   // Infos
  }                                                         // Fin du test

  if (MDNS.begin(HOSTNAME)) {                               // Active le nom « store.local »
    MDNS.addService("http", "tcp", 80);                     // Annonce le serveur web
    Serial.printf("[WIFI] Page web : http://%s.local\n", HOSTNAME);   // Débogage
  }                                                         // Fin du test mDNS
}                                                           // Fin de startWiFi

// ======================================================
// INITIALISATION / BOUCLE PRINCIPALE
// ======================================================
void setup() {                                              // Exécutée une fois au démarrage
  Serial.begin(115200);                                     // Port série à 115200 bauds
  delay(500);                                               // Laisse le temps à l'USB de démarrer
  Serial.println("\n=== XIAO ESP32-C3 Store enrouleur (web) ===");   // Bannière

  pinMode(IN1, OUTPUT);                                     // IN1 en sortie
  pinMode(IN2, OUTPUT);                                     // IN2 en sortie
  pinMode(IN3, OUTPUT);                                     // IN3 en sortie
  pinMode(IN4, OUTPUT);                                     // IN4 en sortie
  setCoils(0,0,0,0);                                        // Moteur hors tension au démarrage

  loadSettings();                                           // Recharge calibration et position

  const esp_timer_create_args_t args = {                    // Paramètres du temporisateur
    .callback = &stepperTick,                               // Fonction appelée à chaque échéance
    .arg = nullptr,                                         // Pas d'argument
    .dispatch_method = ESP_TIMER_TASK,                      // Exécutée dans la tâche dédiée aux temporisateurs
    .name = "stepper"                                       // Nom (pour le débogage)
  };                                                        // Fin des paramètres
  esp_timer_create(&args, &stepTimer);                      // Crée le temporisateur
  esp_timer_start_periodic(stepTimer, STEP_PERIOD_US);      // Le lance : un appel toutes les 3 ms

  startWiFi();                                              // Connexion Wi-Fi

  server.on("/", handleRoot);                               // Page principale
  server.on("/api/status", handleStatus);                   // État au format JSON
  server.on("/api/cmd", handleCmd);                         // Commandes du moteur
  server.begin();                                           // Démarre le serveur web
  Serial.println("[WEB] Serveur démarré");                  // Débogage
}                                                           // Fin de setup

void loop() {                                               // Exécutée en boucle
  server.handleClient();                                    // Traite les requêtes des navigateurs

  if (stopEvent) {                                          // Un arrêt vient d'avoir lieu
    stopEvent = false;                                      // Acquitte l'événement
    Serial.printf("[MOTEUR] ARRÊT à %ld pas (%d %%)\n", (long)currentStep, positionPercent());   // Débogage
    if (!calibrating) savePosition();                       // Mémorise la position (sauf en calibration)
  }                                                         // Fin du test
}                                                           // Fin de loop
