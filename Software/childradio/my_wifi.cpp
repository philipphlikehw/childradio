
#define WIFI_MANAGER
extern int DEBUG;

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include "my_common.h"
#ifndef WIFI_MANAGER
#include <WiFi.h>


static void wifi_set_pmf_optional()
{
  wifi_config_t cfg{};
  esp_wifi_get_config(WIFI_IF_STA, &cfg);

  cfg.sta.pmf_cfg.capable = true;
  cfg.sta.pmf_cfg.required = false;

  esp_wifi_set_config(WIFI_IF_STA, &cfg);
}

//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup_WiFi() {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setHostname(HOSTNAME_default);
  WiFi.begin(ssid, password);
  wifi_set_pmf_optional();
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
#include "my_eeprom.h"
//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//*******************
void setup_WiFi() {
  bool res=false;
  char hostname[HOSTNAME_LENGTH];
  read_hostname(hostname);
  //If the stored Hostname is anvalid, use the default hostname
  if ( !isValidHostname(hostname))  {
    if (DEBUG>0) Serial.println("Wifi:  Hostname unvalid. Use Default Hostname");
    memcpy(hostname,HOSTNAME_default,sizeof(HOSTNAME_default));
  }
  if (DEBUG>0) Serial.print("Wifi:  Hostname: ");
  if (DEBUG>0) Serial.println(hostname);
  
  wifiManager.setHostname(hostname);
  wifiManager.setCountry("DE");
  //first parameter is name of access point, second is the password
  while(!res){
    res=wifiManager.autoConnect(hostname);
  }
   if (DEBUG>0) Serial.println("Wifi: Connected");
   esp_wifi_set_ps(WIFI_PS_NONE);

   if(!MDNS.begin(hostname)) {
     Serial.println("WiFi:  Error starting mDNS");
     return;
  }
  // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
}


#endif
