// **********************************
// * LIBRAIRIES                     *
// **********************************
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
//#include <NewPing.h>
//#include <Ticker.h>

// Librairies pour l'OTA (Mise à jour à distance)
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// **********************************
// * CONFIGURATION                  *
// **********************************
// * Initiate led blinker library
//Ticker ticker;


// WiFi settings
#define WIFI_SSID "XXXXXXXX"
#define WIFI_PWD  "YYYYYYYYY"
#define DELAY_IN_WIFI 500
#define HOSTNAME "Citerne-ESP" // Nom de l'appareil sur le réseau pour l'OTA

// MQTT Server
#define mqtt_server "AAA.BBB.CCC.DDD"
#define mqtt_user "ZZZZZZZZZ"
#define mqtt_password "KKKKKKKKKKK"
const unsigned mqtt_port = 1883; //Par défaut

// Topics MQTT
#define citerneM_topic "gladys/master/device/mqtt:Jardin:Citernes/feature/mqtt:Jardin:Citernes:CiterneM/state"
#define citerneL_topic "gladys/master/device/mqtt:Jardin:Citernes/feature/mqtt:Jardin:Citernes:CiterneL/state"
#define humid_topic "gladys/master/device/mqtt:Jardin:Citernes/feature/mqtt:Jardin:Citernes:humidite/state"
#define temp_topic "gladys/master/device/mqtt:Jardin:Citernes/feature/mqtt:Jardin:Citernes:temperature/state"
#define press_topic "gladys/master/device/mqtt:Jardin:Citernes/feature/mqtt:Jardin:Citernes:pression/state"

// **********************************
// * OBJETS ET VARIABLES            *
// **********************************

WiFiClient espClient;
PubSubClient client(espClient);

// Caractéristiques petite citerne (1)
#define trigger1 16 // D0 RX
#define echo1 14    // D5 TX
const int profondeur_max1 = 170;
const int distance_haut1 = 70;

// Caractéristiques grande citerne (2)
#define trigger2 4 // D2 RX
#define echo2 5    // D1 TX
const int profondeur_max2 = 215;
const int distance_haut2 = 40;

// Capteur BME280
Adafruit_BME280 bme; // I2C
#define adresseI2CduBME280 0x76
#define BME_SDA 12 // D6
#define BME_SCL 13 // D7
const int altitude = 180; //Altitude actuelle pour la correction de la pression
const float corraltitude = altitude/8.5 ;

// Divers mesures
const unsigned long timeout = 25000UL;
const float sound_speed = 340.0 / 1000;
int pourcentage1old = 100;
int pourcentage2old = 100;
float pourcentage1 = 100;
float pourcentage2 = 100;

// Gestion du temps entre deux mesures
unsigned long previousMillis = 0;
const unsigned long interval = 1800000; // 1 heure 3600000
bool firstRun = true; // Pour forcer la mesure au démarrage

// **********************************
// * FONCTIONS                      *
// **********************************

/* // * Blink on-board Led
void tick()
{
    // * Toggle state
    int state = digitalRead(LED_BUILTIN);    // * Get the current state of GPIO1 pin
    digitalWrite(LED_BUILTIN, !state);       // * Set pin to the opposite state
}
*/
void connectWiFi() {
  WiFi.mode(WIFI_STA); // Important pour l'OTA d'être explicitement en Station
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  Serial.println(" ...");
  //ticker.attach(0.2, tick);
  
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(DELAY_IN_WIFI);
    Serial.print(".");
    if (i >= 20) { // Timeout un peu plus long
      i = 0;
      Serial.println("");
    } else {
      i++;
    }
  }
  Serial.println("connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setup_ota() {
  // Port par défaut 8266
  ArduinoOTA.setPort(8266);
  // Hostname
  ArduinoOTA.setHostname(HOSTNAME);
  // Mot de passe pour l'upload (optionnel)
  ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: si on mettait à jour le FS, on devrait démonter FS ici avec SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void reconnexion_mqtt() {
  // On vérifie d'abord le WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  
  // Tentative MQTT (une seule tentative pour ne pas bloquer l'OTA trop longtemps)
  if (!client.connected()) {
    Serial.print("Tentative de connexion MQTT...");
    //ticker.attach(0.6, tick);
    String clientId = "Citernes-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("Connecté");
      //client.subscribe(citerneM_topic);
      //client.subscribe(citerneL_topic);
    } else {
      Serial.print("Echec, rc=");
      Serial.println(client.state());
    }
  }
}

void prendre_mesures_et_publier() {
  Serial.println("Début des mesures...");
  //ticker.attach(1, tick);
  delay(100);
  // --- Mesure citerne 1 ---
  digitalWrite(trigger1, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger1, HIGH);
  delayMicroseconds(250);
  digitalWrite(trigger1, LOW);
  long mesure1 = pulseIn(echo1, HIGH, timeout);
  float distance1 = mesure1 / 20.0 * sound_speed;
  pourcentage1 = (1 - (distance1 - distance_haut1) / (profondeur_max1 - distance_haut1)) * 100;

  // --- Mesure citerne 2 ---
  delay(100); // Petit délai de sécurité entre les deux capteurs
  digitalWrite(trigger2, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger2, HIGH);
  delayMicroseconds(250);
  digitalWrite(trigger2, LOW);
  long mesure2 = pulseIn(echo2, HIGH, timeout);
  float distance2 = mesure2 / 20.0 * sound_speed;
  pourcentage2 = (1 - (distance2 - distance_haut2) / (profondeur_max2 - distance_haut2)) * 100;
  
  // --- Mesure BME280 ---
  float temp = bme.readTemperature() - 5;
  float humid = bme.readHumidity() + 17;
  float pression = (bme.readPressure() / 100.0F) + corraltitude;

  // --- Affichage ---
  Serial.print("Dist C1: "); Serial.print(distance1); Serial.print("cm (soit : "); Serial.print(pourcentage1); Serial.println("%)");
  Serial.print("Dist C2: "); Serial.print(distance2); Serial.print("cm (soit : "); Serial.print(pourcentage2); Serial.println("%)");
  Serial.print("Temp : "); Serial.print(temp); Serial.println(" °C");
  Serial.print("Pression : "); Serial.print(pression); Serial.println(" hPa");
  Serial.print("Humidité : "); Serial.print(humid); Serial.println(" %");

  // --- Publication MQTT ---
  // On ne publie que si ça change ou si c'est le premier run et si valeurs cohérentes
  
  if (firstRun || (pourcentage1 != pourcentage1old) || (pourcentage2 != pourcentage2old)){
    if(pourcentage1<110 && pourcentage2<110) {
    Serial.println("Changement détecté (ou démarrage), envoi vers Gladys...");
    
    if (!client.connected()) {
      reconnexion_mqtt();
    }
    
    if (client.connected()) {
      Serial.println("Publication en cours");
       client.publish(citerneM_topic, String(pourcentage1).c_str(), true);
       client.publish(citerneL_topic, String(pourcentage2).c_str(), true);
       client.publish(temp_topic, String(temp).c_str(), true);
       client.publish(humid_topic, String(humid).c_str(), true);
       client.publish(press_topic, String(pression).c_str(), true);
       
       pourcentage1old = pourcentage1;
       pourcentage2old = pourcentage2;
       
       // Déconnection MQTT
       client.disconnect();
    }
  }
  
  firstRun = false;
  Serial.println("Fin des mesures. En attente...");
}
}

// **********************************
// * SETUP                          *
// **********************************
void setup() {
  Serial.begin(9600);
  // * Set led pin as output
   // pinMode(LED_BUILTIN, OUTPUT);
   // ticker.attach(0.6, tick);
    
  // Init BME280
  Wire.begin(BME_SDA, BME_SCL);
  if (!bme.begin(adresseI2CduBME280)) {
    Serial.println("Impossible de trouver le BME280 !");
    // On ne bloque pas tout le programme pour ça, sinon plus d'OTA possible en cas de panne capteur
  }

  // Init Pins
  pinMode(trigger1, OUTPUT); //digitalWrite(trigger1, LOW);
  pinMode(echo1, INPUT);
  pinMode(trigger2, OUTPUT); //digitalWrite(trigger2, LOW);
  pinMode(echo2, INPUT);


  // Connexions Réseau
  connectWiFi();
  
  // Init MQTT Server param
  client.setServer(mqtt_server, mqtt_port);

  // Init OTA (Important : après le WiFi)
  setup_ota();
}

// **********************************
// * LOOP PRINCIPALE                *
// **********************************
void loop() {
  // 1. Gérer les mises à jour OTA
  ArduinoOTA.handle();

  // 2. Gestion du Timer pour les mesures
  unsigned long currentMillis = millis();

  if (firstRun || (currentMillis - previousMillis >= interval) || pourcentage1>110 || pourcentage2>110) {
    // Sauvegarde du temps actuel
    previousMillis = currentMillis;
    
    // Exécution des mesures
    prendre_mesures_et_publier();
  }
  /* // * Keep LED on
    ticker.detach();
    digitalWrite(LED_BUILTIN, LOW);
    */
 }
