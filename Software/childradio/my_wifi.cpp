
#define WIFI_MANAGER
extern int DEBUG;


#ifndef WIFI_MANAGER
#include <WiFi.h>

const char* ssid = "xxxxxxxxxxxxxxxxxxxxxx";
const char* password = "xxxxxxxxxxxxxxxxxxxxxx";

//const char* ssid = "BISON";
//const char* password = "015156191136";

//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup_WiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (DEBUG>0) Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    if (DEBUG>0) Serial.print('.');
    delay(1000);
  }
  if (DEBUG>0) Serial.print(' ');
  if (DEBUG>0) Serial.println(WiFi.localIP());
  if (DEBUG>0) Serial.print("RRSI: ");
  if (DEBUG>0) Serial.println(WiFi.RSSI());
}

#else


#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

WiFiManager wifiManager;

//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//*******************
void setup_WiFi() {
  bool res=false;
  wifiManager.setHostname("kinderradio");
  //first parameter is name of access point, second is the password
  while(!res){
    res=wifiManager.autoConnect("kinderradio");
  }
   if (DEBUG>0) Serial.println("Wifi Connected");
}


#endif
