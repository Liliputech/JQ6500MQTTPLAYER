//2.10.2019
//on appelle effectivement la séquence des sons avec la publication mosquitto_pub -t play -m 1 


////Sequence a performance using timecode HH,MM,SS.SSS : https://gist.github.com/kriegsman/a916be18d32ec675fea8
//le git de Kriegsman ! https://gist.github.com/kriegsman

///////////////////////////////////////////////////////////////////////////////////////////////////////

//------Bibliothèques et Settings------
#include <PubSubClient.h>//https://pubsubclient.knolleary.net/api.html
#include <ESP8266WiFi.h>//https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/station-class.html

#include "WifiConfig.h"

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <JQ6500_Serial.h>

JQ6500_Serial mp3;


//SSID et IP du Broker MQTT à indiquer dans fichier WifiConfig.h
char ssid[] = WIFI_SSID;
char password[] = WIFI_PASSWORD;
IPAddress mon_broker(BROKER_IP);

WiFiClient ESP01client;
PubSubClient client(ESP01client);

String clientID = WiFi.hostname();//nom DHCP de l'ESP, quelque chose comme : ESP_XXXXX, il est dans l'ESP de base
String topicName = clientID + "/#";//Topic individuel nominatif pour publier un message unique à un ESP unique
bool global_enabled = false; //FLAG : ne pas lancer de ledPattern tant que pas de messages reçu en MQTT
byte playSnd = 0;




//------------------------ VARIABLES POUR LA GESTION DU TEMPS DE LA PARTITION -------------------

uint32_t gTimeCodeBase = 0;
uint32_t gTimeCode = 0;
uint32_t gLastTimeCodeDoneAt = 0;
uint32_t gLastTimeCodeDoneFrom = 0;

#define TC(HOURS,MINUTES,SECONDS) \
  ((uint32_t)(((uint32_t)((HOURS)*(uint32_t)(3600000))) + \
              ((uint32_t)((MINUTES)*(uint32_t)(60000))) + \
              ((uint32_t)((SECONDS)*(uint32_t)(1000)))))


#define AT(HOURS,MINUTES,SECONDS) if( atTC(TC(HOURS,MINUTES,SECONDS)) )
#define FROM(HOURS,MINUTES,SECONDS) if( fromTC(TC(HOURS,MINUTES,SECONDS)) )

static bool atTC( uint32_t tc)
{
  bool maybe = false;
  if ( gTimeCode >= tc) {
    if ( gLastTimeCodeDoneAt < tc) {
      maybe = true;
      gLastTimeCodeDoneAt = tc;
    }
  }
  return maybe;
}


static bool fromTC( uint32_t tc)
{
  bool maybe = false;
  if( gTimeCode >= tc) {
    if( gLastTimeCodeDoneFrom <= tc) {
      maybe = true;
      gLastTimeCodeDoneFrom = tc;
    }
  }
  return maybe;
}




//------------------- FONCTIONS / SONS A DISPOSITION --------------------------
/*

*/



//--------------------------- PARTITION ------------------------
void Performance()//HH,MM,SS.sss (sss en ms)
{  
  FROM(0,0,00.100) { snd1(); }
  FROM(0,0,05.000) { snd2(); }
  FROM(0,0,10.000) { snd3(); }
  FROM(0,0,15.000) { snd4(); }
  FROM(0,0,20.000) { snd5(); } 

  playSnd = 0;
}







//------------------- WIFI SETUP : connexion, IP -----------------
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}




//-----------FONCTION MQTT/Mosquitto :connexion, souscription
void connect_mqtt()
{
  // Loop until we're reconnected
  Serial.println("tentative de connexion au broker Mosquitto...");
  while (!client.connected()) {
    if (client.connect(clientID.c_str()))
      break;
    Serial.print('.');
    delay(10);
  }

  client.subscribe("start");//mosquitto_pub -t start -m 1 = démarrer la séquence
  client.subscribe(topicName.c_str());

  client.publish("welcome", clientID.c_str()); // publie son nom sur le topic welcome lors de la connexion
  Serial.println("connexion au Broker Mosquitto OK !!");
  Serial.printf("topic welcome : %s\n", clientID.c_str());
}




//------------------------------MQTT macro----------------------------------------
//function "macro" pour traiter le message MQTT arrivant au MC et retourner la partie utile : un entier
int payloadToInt(byte* payload, int length) {
  String payloadFromMQTT = "";
  for (int i = 0; i < length; i++) { //itérer dans toute la longueur message, length est fourni dans le callback MQTT
    if (isDigit(payload[i]))// tester si le payload est bien un chiffre décimal
      payloadFromMQTT += (char)payload[i];//caster en type char et ajouter/placer dans la variable
  }
  return payloadFromMQTT.toInt();//convertir le String en entier et retourner la valeur
}


//-----------------------------MQTT CALLBACK-------------------------------
void callback(char* topic, byte* payload, unsigned int length)
{
  String debugMessage = "";

  global_enabled = true; // open flag : PLAY !

  //Affichage dans le moniteur (topic welcome) du -t topic et du - m message
  debugMessage = clientID + " - Topic entrant : [" + topic + "] ";


  //------ Si le topic est start -----
  //------appel de la séquence pour tous les clients ou ce client -----
  if ( (strcmp(topic, (clientID + "/play").c_str()) == 0) //mosquitto_pub -t ESP_304B27/start -m 1
       || (strcmp(topic, "play") == 0)) { // pour tous les clients: mosquitto_pub -t start -m 1
    playSnd = payloadToInt(payload, length);
    debugMessage += playSnd;

  }

  //---------- Pour visualiser le traffic arrivant aux ESP ----------------
  client.publish("welcome", debugMessage.c_str());
}







//----------------------- SETUP ----------------------------------
void setup() {
  delay(3000); // 3 second delay for recovery

  Serial.begin(74880);
  delay(10);

/*
    //////////// ESP01 settings //////////
    //********************* CHANGE ESP01 PIN FUNCTION **************************************
    pinMode(3, FUNCTION_0); //réinitialisation des pins comme par défaut au cas ou des esp ont été tripotés
    //**************************************************************************************
*/

  setup_wifi();

  //IP et port du Broker https://pubsubclient.knolleary.net/api.html#setserver
  client.setServer(mon_broker, 1883);

  //fonction qui permet de traiter -t -m
  //https://pubsubclient.knolleary.net/api.html#callback
  client.setCallback(callback);

  mp3.begin(9600, 0, 2);
  mp3.reset();
  mp3.setVolume(30);
  mp3.setLoopMode(MP3_LOOP_ONE_STOP); // Default, plays track and stops
  mp3.pause(); 

  RestartPerformance();
}


//------------------------------ LOOP --------------------------------
void loop()
{
   //Set the current timecode, based on when the performance started
    gTimeCode = millis() - gTimeCodeBase;
    Performance();    //on démarre la partition

    
  // Call the current pattern function once, updating the 'leds' array
  // gPatterns[gCurrentPatternNumber]();


  if (WiFi.status() != WL_CONNECTED)
    setup_wifi();

  //if not connected to MQTT tries to connect
  if (!client.connected())
    connect_mqtt();

  //MQTT Refresh message queue
  client.loop();

  if (!global_enabled) //Global "display enable" flag
    return;

  
//on relance ssi on l'a demandé (= Si une publication mosquitto_pub -t start -m 1 arrive)
if (playSnd >= 1)
  {
  RestartPerformance();
  }
}

/*
  if (!global_enabled) //Global "display enable" flag
    return;

  //----------------- JQ6500 ------------------//
  if (mp3.getStatus() != MP3_STATUS_PLAYING)
  {
    mp3.playFileByIndexNumber(gCurrentPatternNumber);
    mp3.play();
  }

*/


/*
  void nextPattern()
  {
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
  }
*/


//on reset les compteurs pour (re)démarrer la séquence (partition) 
void RestartPerformance() 
  {
    gLastTimeCodeDoneAt = 0;
    gLastTimeCodeDoneFrom = 0;
    gTimeCodeBase = millis();
  }

 

void snd1()
{
  if (mp3.getStatus() != MP3_STATUS_PLAYING)
      {
  mp3.playFileByIndexNumber(1);
  //mp3.play(); 
      }
}

void snd2()
{
  if (mp3.getStatus() != MP3_STATUS_PLAYING)
      {
  mp3.playFileByIndexNumber(2);
  //mp3.play(); 
      }
}

void snd3()
{
  if (mp3.getStatus() != MP3_STATUS_PLAYING)
      {
  mp3.playFileByIndexNumber(3);
  //mp3.play(); 
      }
}

void snd4()
{
  if (mp3.getStatus() != MP3_STATUS_PLAYING)
      {
  mp3.playFileByIndexNumber(4);
  //mp3.play(); 
      }
}

void snd5()
{
  if (mp3.getStatus() != MP3_STATUS_PLAYING)
      {
  mp3.playFileByIndexNumber(5);
  //mp3.play(); 
      }
}
/*
void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}

void snd2()
{
  mp3.playFileByIndexNumber(2);
  mp3.play(); 
}
*/

//-------------------------------------------------------
