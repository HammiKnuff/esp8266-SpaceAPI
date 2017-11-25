/*
  Get space state and show open/closed status on RGB led

  Additional libarys, these can be added via Arduino IDE, menu include libery, manage libery :
    - WifiClientSecure - for secure HTTPS connection, included in ESP libary
    - ArduinoJson - Reading the api file on easy way, see https://bblanchon.github.io/ArduinoJson/ I used version 5.8.4

  For additional documentation see :
    This project on TkkrLab API : https://tkkrlab.nl/wiki/SpaceState_ESP
    More for about space api : http://spaceapi.net/ or its fork at https://spacedirectory.org/
    
  Version : 1.0
  Date Created : 2017-4-5
  Date last update : 2017-4-5
  Created by Dave Borghuis / dave@daveborghuis.nl
  Licence : Creative Commens BY-SA 4.0 https://creativecommons.org/licenses/by-sa/4.0/
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//**** Change setting for WIFI ******
const char* ssid     = "Your Wifi Name";
const char* password = "Your Wifi Password";

//**** Change WEB setting space API ******
//for directory of space see https://spaceapi.fixme.ch/

const char* server = "hamburg.ccc.de";//your hackerspace adress
const char* resource = "/dooris/status.json";//and the path behind the adress
const int JSON_BUFFERSIZE = 817; //Size of buffer, make it big enoug to fit whole api/json file.
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char* fingerprint = "";

//****** LED PINS ********
#define green_pin D6 // GPO pin 12 //D6
#define red_pin D7 //GPO pin 13 D7
#define blue_pin D5 //GP pin 14 D5

#ifdef true //change to false for catcode LEDS
  //anode LED mode
  #define ON  LOW
  #define OFF HIGH
#else
  //catcode LED mode
  #define ON  HIGH
  #define OFF LOW
#endif

//*** internal parameters dont change ***
int oldspacestate = -1;
int spacestate = 0; 
unsigned int waittime;

const size_t MAX_CONTENT_SIZE = 512; // max size of the HTTP response
const unsigned long HTTP_TIMEOUT = 10000;  // max respone time from server

WiFiClientSecure client;

void setup() {
Serial.print("Hallo i bims 1 Ampel vong Lampen her!");
  Serial.begin(9600);
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(ssid);

  pinMode(blue_pin, OUTPUT);
  pinMode(green_pin, OUTPUT);
  pinMode(red_pin, OUTPUT);
  
  setLED(OFF, OFF, ON);

  //pinMode(LED_BUILTIN, OUTPUT); //onboard led (NOT on ESP)   //Deaktiviert weil nervt!
  //digitalWrite(LED_BUILTIN, HIGH); //das was oben steht.
  
  //Setup Wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  digitalWrite(LED_BUILTIN, LOW);

  setLED(OFF, OFF, OFF); 

  //OTA

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Space State Siemens Ampel");

  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"wuff");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
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
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void loop() {
ArduinoOTA.handle();
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("============================");

  if (getStatus()) {
    setRGBfromStatus();
  } else {
    Serial.println("ERROR no state found!");
    setLED(ON, ON, ON); //blue
  }
  digitalWrite(LED_BUILTIN, LOW);

  yield();
  delay(waittime);  
}


bool getStatus() {
  // Use WiFiClientSecure class to create TLS connection
  Serial.print("connecting to ");
  Serial.println(server);

  if (!client.connect(server, httpsPort)) {
    yield();
    Serial.println("connection failed");
    return false;
  }

  if (client.verify(fingerprint, server)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }

  if (sendRequest(server, resource) && skipResponseHeaders()) {
    if (readReponseContent()) {
      setRGBfromStatus();
      client.stop();
      return true;
    }
  }

  //something went wrong
  return false;
}

bool readReponseContent() {

  char pBuf[JSON_BUFFERSIZE];
  String Buffer;  
 
  //Fill buffer 
  int i;
  for ( i = 0; i < (sizeof(pBuf) - 1) && client.available(); i++)
  {
    char c = client.read();
    pBuf[i] = c;
  }
  // Add zero terminator
  pBuf[i] = '\0';

  // Allocate a temporary memory pool
  DynamicJsonBuffer jsonBuffer(JSON_BUFFERSIZE);

  JsonObject& root = jsonBuffer.parseObject(pBuf);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return false;
  }

  if ( root["api"].as<float>() >= (float) 0.13) {
    spacestate = root["state"]["open"].as<bool>(); //api v 0.13 and up
  } else {
    spacestate = root["open"].as<bool>(); //v 0.12
  }

  //process schedule from space api
  String schedule = root["cache"]["schedule"].as<String>();
  String s = schedule.substring(0,1);
  int num = schedule.substring(2).toInt();
  if (num != 0) {
    if (s == "d") { //day
      waittime = num * 24*60*60*1000;
    };
    if (s == "h") { //hour
      waittime = num * 60*60*1000;    
    };
    if (s == "m") { //minute
      waittime = num * 60*1000;
    };
    if (s == "s") { //second (only for testing, not part of 
      waittime = num*1000;
    };
  } else {
    waittime = 10*1000; //minium 10 minute if no schedule found
  };

  Serial.print("Space state = ");
  Serial.print(spacestate);
  Serial.print(" Waittime = ");
  Serial.println(waittime);

  return true;
}

// Send the HTTP GET request to the server
bool sendRequest(const char* host, const char* resource) {
  Serial.print("GET ");
  Serial.print(host);
  
  Serial.println(resource);

  client.print("GET ");
  client.print(resource);
  client.println(" HTTP/1.0");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println();

  return true;
}

// Skip HTTP headers so that we are at the beginning of the response's body
bool skipResponseHeaders() {
  // HTTP headers end with an empty line
  char endOfHeaders[] = "\r\n\r\n";

  client.setTimeout(HTTP_TIMEOUT);
  bool ok = client.find(endOfHeaders);

  if (!ok) {
    Serial.println("No response or invalid response!");
  } else {
    Serial.println("Headers skipped.");
  }

  return ok;
}

void setRGBfromStatus() {
  //first time, set current spacestate
  if (oldspacestate == -1) {
    if (spacestate) {
      setLED(OFF, ON, OFF); //Green
    } else {
      setLED(ON, OFF, OFF); //Red
    }
    oldspacestate = spacestate;
  }

  if (spacestate != oldspacestate) {
    if (spacestate == 1 and oldspacestate == 0 ) {
      Serial.println("Switch from Closed to Open");
      setLED(OFF, ON, OFF); //Green
    } else if (spacestate == 0 and oldspacestate == 1 ) {
      //Space is nu gesloten
      Serial.println("Switch from Open to Closed");
      setLED(ON, OFF, OFF); //Red
    } 
    oldspacestate = spacestate;
  }
}

void setLED(int R, int G, int B) {
  digitalWrite(blue_pin, B);
  digitalWrite(green_pin, G);
  digitalWrite(red_pin, R);

  Serial.print("Set LED to R :");
  Serial.print(R);

  Serial.print(" G :");
  Serial.print(G);

  Serial.print(" B : ");
  Serial.println(B);

  
}
