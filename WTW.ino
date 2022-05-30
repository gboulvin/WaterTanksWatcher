//Déclaration des librairies nécessaires
#include "WiFiEsp.h"      //Librairie pour l'utilisation du WiFi
#include <PubSubClient.h> //Librairie pour l'utilisation de MQTT

// Emulate Serial1 on pins 6/7 if not present
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(6, 7); // RX, TX
#endif
char ssid[] = "XXXXXX";      // your network SSID (name)
char pass[] = "YYYYYYYY";    // your network password
int status = WL_IDLE_STATUS;  // the Wifi radio's status

//Propriétés du serveur MQTT
#define mqtt_server "KKK.KKK.KKK.KKK"
#define mqtt_user "NNNNNNNN"  //MQTT's Username (s'il a été configuré sur Mosquitto)
#define mqtt_password "ZZZZZZZZ" //Password idem

#define citerneM_topic "gladys/master/device/mqtt:Jardin:Citernes/feature/mqtt:Jardin:Citernes:CiterneM/state" //Topic petite citerne
#define citerneL_topic "gladys/master/device/mqtt:Jardin:Citernes/feature/mqtt:Jardin:Citernes:CiterneL/state" //Topic grande citerne
WiFiEspClient espClient;
PubSubClient client(espClient);


//Caractéristiques petite citerne (1)
const byte trigger1=2;
const byte echo1=3;
const int profondeur_max1=170; //Profondeur de la petite citerne en cm (remplissage à 0%)
const int distance_haut1=70; //distance entre le capteur et le niveau maximum (remplissage à 100%)
//Caractéristiques grande citerne (2)
const byte trigger2=4;
const byte echo2=5;
const int profondeur_max2=215; //Profondeur de la grande citerne en cm
const int distance_haut2=40; //distance entre le capteur et le niveau maximum
const unsigned long timeout = 25000UL;
const float sound_speed=340.0/1000;
int pourcentage1old=100; //Stockage de la valeur précédente (historique)
int pourcentage2old=100; //Stockage de la valeur précédente (historique)

void setup() {
  
pinMode(trigger1,OUTPUT);
digitalWrite(trigger1,LOW);
pinMode(echo1,INPUT);
pinMode(trigger2,OUTPUT);
digitalWrite(trigger2,LOW);
pinMode(echo2,INPUT);
Serial.begin(9600);  // initialize serial for ESP module
  Serial1.begin(9600);  // initialize ESP module
  WiFi.init(&Serial1);  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Aucune carte Wifi détectée");
    // don't continue
    while (true);
  }
  // attempt to connect to WiFi network
  while ( status != WL_CONNECTED) {
 //   Serial.print("Tentative de connexion au réseau WPA SSID : ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }
  Serial.println("Connection au réseau réussie");


  // Connexion au serveur MQTT
  client.setServer(mqtt_server, 1883);    //Configuration de la connexion au serveur MQTT
//  client.setCallback(callback);  //Appelle la fonction de callback qui est executée pour la réception des messages   
  while (!client.connected()) {
  Serial.print("Connexion à Gladys en MQTT...");
    if (client.connect("Citerne", mqtt_user, mqtt_password )) {
      client.subscribe(citerneM_topic);
      client.subscribe(citerneL_topic);
      Serial.println("Connecté !!");
    } else {
      Serial.print("Erreur ! Code : ");
      Serial.print(client.state());
      delay(100);
      }
  }
}

void reconnexion_mqtt()
{
  if ( status != WL_CONNECTED) {
    while ( status != WL_CONNECTED) {
      status = WiFi.begin(ssid, pass);
    }
   }
  while (!client.connected()) 
  {
   Serial.println("Tentative de reconnexion à Gladys en MQTT...");
   String clientId = "Citernes-";
   clientId += String(random(0xffff), HEX);
   if (client.connect(clientId.c_str())) 
      {
        Serial.println("Connecté");
        client.subscribe(citerneM_topic);
        client.subscribe(citerneL_topic);
      } else 
      {
        Serial.print("Connexion à Gladys impossible, code d'erreur : ");
        Serial.println(client.state());
        Serial.println("Nouvelle tentative dans 5 secondes");
        delay(5000);
      }
    } 
 
}

void loop() {
// Mesure citerne 1
delay(1000);
digitalWrite(trigger1,HIGH);
delayMicroseconds(10);
digitalWrite(trigger1,LOW);
long mesure1=pulseIn(echo1,HIGH,timeout);
float distance1=mesure1/20.0*sound_speed; //Récupère la distance mesurée en cm
float pourcentage1=(1-(distance1-distance_haut1)/(profondeur_max1-distance_haut1))*100; //Conversion en % : (250-50)/x=100 

//Mesure citerne 2
delay(1000);
digitalWrite(trigger2,HIGH);
delayMicroseconds(10);
digitalWrite(trigger2,LOW);
long mesure2=pulseIn(echo2,HIGH,timeout);
float distance2=mesure2/20.0*sound_speed;
float pourcentage2=(1-(distance2-distance_haut2)/(profondeur_max2-distance_haut2))*100; //Conversion en % (250-50)/x=100

//Affichage des résultats
Serial.print("Distance petite citerne (1) : ");
Serial.print(distance1);
Serial.println("cm");
Serial.print("Soit : ");
Serial.print(pourcentage1);
Serial.println("%");
Serial.print("Distance grande citerne (2) : ");
Serial.print(distance2);
Serial.println("cm");
Serial.print("Soit : ");
Serial.print(pourcentage2);
Serial.println("%");

//Publication SSI modification des mesures
if ((pourcentage1!=pourcentage1old) | (pourcentage2!=pourcentage2old)) {
  Serial.println("Les valeurs ont changé, publication sur Gladys");
   if (!client.connected()) { // Vérifie la connexion avec le serveur MQTT
    reconnexion_mqtt();
    } 
   client.publish(citerneM_topic, String(pourcentage1).c_str(), true);   //Publie la température sur le topic temperature_topic
   client.publish(citerneL_topic, String(pourcentage2).c_str(), true);   //Publie la température sur le topic temperature_topic
  pourcentage1old=pourcentage1; //stockage de la dernière valeur
  pourcentage2old=pourcentage2; //stockage de la dernière valeur
  client.disconnect();
}
//Veille
Serial.println("Prochaine mesure dans une heure...");
delay(3600000);
}
