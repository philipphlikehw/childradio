#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>


#include <Update.h>
#include <SPIFFS.h>
#include <FS.h>
// #include <esp_int_wdt.h>
// #include <esp_private/esp_int_wdt.h>
#include <esp_task_wdt.h>

#include "my_common.h"
#include "my_time.h"
#include "my_encoder.h"

#include "page.h"

#include <SpiffsFilePrint.h>  // https://github.com/PRosenb/SPIFFS_FilePrint
extern SpiffsFilePrint filePrint;
#define Serial filePrint


#define WEBRADIO_INFO
#ifdef WEBRADIO_INFO
  #include "my_webradio2.h"
#endif

#define EEPROM_INFO
#ifdef EEPROM_INFO
  #include "my_eeprom.h"
#endif

#define BAT_ACTIVE
#ifdef BAT_ACTIVE
  #include "my_bat.h"
#endif

#define RGB_LEDs
#ifdef RGB_LEDs
  #include "my_ledstripe.h"
#endif


#ifdef IMAGECOVER
const unsigned char default_cover_jpg[] PROGMEM = {
  0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01,
  0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43,
  0x00, 0x08, 0x06, 0x06, 0x06, 0x06, 0x06, 0x08, 0x06, 0x06, 0x06, 0x08,
  0x08, 0x08, 0x0A, 0x0C, 0x14, 0x0D, 0x0C, 0x0A, 0x0A, 0x0C, 0x13, 0x0E,
  0x0E, 0x0C, 0x14, 0x1B, 0x16, 0x14, 0x17, 0x15, 0x14, 0x16, 0x1D, 0x1A,
  /* ... Rest des JPEG ... */
  /* Kürzen für Übersicht: Array hat 629 Bytes total */
};
const unsigned int default_cover_jpg_len = 629;

static File cover_file_factory()
{
    // du kannst hier jederzeit den Pfad ändern, z.B. "/cover.jpg" oder "/image/current.jpg"
    File f = SPIFFS.open("/cover.jpg", FILE_WRITE);
    if (!f) {
        Serial.println("[webserver] cannot open /cover.jpg for write");
    }
    return f;  // File wird per Wert zurückgegeben, ist okay, Arduino-File ist ein Handle-Wrapper
}


void handle_image(void);
#endif

extern int DEBUG;

// Function headers
void setup_webserver();
void loop_webserver();
void handle_OnConnect();
void handle_OnConnect2();
void handle_NotFound();
void handle_Log();
void handle_xml_refresh();
void handle_xml_init();
void handle_alarm();
void handle_channel();
void handle_hostname();

WebServer server(80);

//******************************************************************************************
//                                  H E L P E R                                            *
//******************************************************************************************
esp_task_wdt_config_t twdt_config
{
  .timeout_ms     = 1000,      // 1 s reicht zum sicheren Reset-Trigger
  .idle_core_mask = 0b11,
  .trigger_panic  = true
};


void forcedReset(){
// use the watchdog timer to do a hard restart
// It sets the wdt to 1 second, adds the current process and then starts an
// infinite loop.
  esp_task_wdt_init(&twdt_config);
  esp_task_wdt_add(NULL);
  while(true);  // wait for watchdog timer to be triggered
}

void xml_replace(String *str) {
  str->replace("&", "&amp;");
  str->replace("<", "&lt;");
  str->replace(">", "&gt;");
  str->replace("\"", "&quot;");
  str->replace("\'", "&apos;");
}

void safeCopy(char *dst, size_t dstLen, const String& src) {
  memset(dst, 0, dstLen);
  size_t n = std::min(src.length(), dstLen - 1);
  memcpy(dst, src.c_str(), n);
  dst[n] = '\0';
}



static volatile bool g_shutdownRequested = false;

void WEBSERVER_shutdown(void){
  g_shutdownRequested=true;
}


// --- Globals (put near the top of your sketch) ---
static bool otaUploadOk     = false;
static bool otaUploadFailed = false;

static void otaMarkFail(const char* msg)
{
  otaUploadFailed = true;
  otaUploadOk = false;
  if (DEBUG > 0 && msg) Serial.println(msg);
}

// Optional: adjust this timeout to your needs
static const uint32_t WEBRADIO_STOP_TIMEOUT_MS = 8000;

//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************

void handle_webserver(void *parameter){
  //.on("/", HTTP_GET, handle_OnConnect);
  //server.on("/log", handle_Log);
  #ifdef IMAGECOVER
  webradio_register_cover_file_factory(cover_file_factory);
  #endif
  server.on("/", HTTP_GET, [](){
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send(200, "text/html", PAGE_NEW);
  });
  server.on("/heap"         , HTTP_GET,[](){server.send(200, "text/plain", String(ESP.getFreeHeap()));});
  server.on("/vol"          , HTTP_GET,[](){ if (server.hasArg("value")) { set_volumen(server.arg("value").toInt()%(VOL_MAX+1)); } } );
  server.on("/balance"      , HTTP_GET,[](){ if (server.hasArg("value")) { int8_t tmp=server.arg("value").toInt(); if((tmp>=-16) and (tmp<=16))set_balance(tmp); } } );
  server.on("/tone_low"     , HTTP_GET,[](){ if (server.hasArg("value")) { int8_t tmp=server.arg("value").toInt(); if((tmp>=-40) and (tmp<=6)) set_tone_low(tmp); } } );
  server.on("/tone_band"    , HTTP_GET,[](){ if (server.hasArg("value")) { int8_t tmp=server.arg("value").toInt(); if((tmp>=-40) and (tmp<=6)) set_tone_band(tmp); } } );
  server.on("/tone_high"    , HTTP_GET,[](){ if (server.hasArg("value")) { int8_t tmp=server.arg("value").toInt(); if((tmp>=-40) and (tmp<=6)) set_tone_high(tmp); } } );
  server.on("/alarm"        , HTTP_GET, handle_alarm);
  server.on("/ajax_refresh" , HTTP_GET, handle_xml_refresh);
  server.on("/ajax_init"    , HTTP_GET, handle_xml_init); 
  server.on("/rgb_pattern"  , HTTP_GET,[](){ if (server.hasArg("value")) { uint8_t tmp=server.arg("value").toInt(); if((tmp>=0) and (tmp<=4)) rgb_set_palette(tmp); } } );
  server.on("/rgb_time"     , HTTP_GET,[](){ if (server.hasArg("value")) { uint8_t tmp=server.arg("value").toInt(); if((tmp>=1) and (tmp<=255)) rgb_set_time(tmp); } } );
  server.on("/rgb_brightness", HTTP_GET,[](){ if (server.hasArg("value")) { uint8_t tmp=server.arg("value").toInt(); if((tmp>=1) and (tmp<=255)) rgb_set_brightness(tmp); } } );
  server.on("/channel"      , HTTP_GET,handle_channel );
  server.on("/log"          , HTTP_GET, handle_Log);
  server.on("/hostname"     , HTTP_GET, handle_hostname );
  #ifdef IMAGECOVER
  server.on("/image", HTTP_GET, handle_image);
  #endif
  server.onNotFound(handle_NotFound);
  // --- Route handler ---
server.on("/update", HTTP_POST,

  // ====== Finalize / Response handler (called AFTER upload finished) ======
  []() {
    server.sendHeader("Connection", "close");

    const bool ok = (!Update.hasError()) && otaUploadOk && !otaUploadFailed;
    server.send(200, "text/plain", ok ? "OK" : "NOK");

    // On success only: persist + reset
    if (ok) {
      eeprom_write_time(time_getEpochTime()); // ToDo
      eeprom_write_bat(bat_get_level());

      // Give the client time to receive the response
      vTaskDelay(pdMS_TO_TICKS(1500));

      // For battery based systems, a restart has to be triggered by watchdog
      forcedReset();
    } else {
      // No reset on NOK
      if (DEBUG > 0) Serial.println("OTA failed -> no reset.");
    }
  },

  // ====== Upload handler ======
  []() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
      otaUploadOk = false;
      otaUploadFailed = false;

      stop_webradio();

      // Wait for webradio to stop, but with a timeout (avoid endless blocking)
      if (DEBUG > 0) Serial.print("Wait until Webradio is closed ");
      const uint32_t t0 = millis();
      while (webradio_is_active()) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (DEBUG > 0) Serial.print(". ");

        if (millis() - t0 > WEBRADIO_STOP_TIMEOUT_MS) {
          if (DEBUG > 0) Serial.println(" timeout!");
          otaMarkFail("ERROR: Webradio did not stop in time. Abort update.");
          return; // do not call Update.begin()
        }
      }
      if (DEBUG > 0) Serial.println(" done.");

      Serial.printf("Update: %s\n", upload.filename.c_str());

      // ESP32 WebServer + Update: UPDATE_SIZE_UNKNOWN is correct (OTA partition)
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
        otaMarkFail("ERROR: Update.begin() failed.");
        return;
      }

    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (otaUploadFailed) return; // failed earlier -> ignore further data

      const size_t written = Update.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        Update.printError(Serial);
        otaMarkFail("ERROR: Update.write() size mismatch.");
        Update.abort();
        return;
      }

    } else if (upload.status == UPLOAD_FILE_END) {
      if (otaUploadFailed) {
        Update.abort();
        return;
      }

      if (Update.end(true)) { // true: set the size to the current progress
        otaUploadOk = true;
        Serial.printf("Update Success: %u\n", upload.totalSize);
      } else {
        Update.printError(Serial);
        otaMarkFail("ERROR: Update.end() failed.");
      }

    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      otaMarkFail("ERROR: Upload aborted by client.");
      Update.abort();
    }

    // Keep system responsive
    vTaskDelay(pdMS_TO_TICKS(1));
  }
);
  server.begin();
  if (DEBUG>0) Serial.print("HTTP: server started");  
  if (DEBUG>0) Serial.print(" on CPU ");
  if (DEBUG>0) Serial.println(xPortGetCoreID());
  if (DEBUG>0) Serial.flush();
  while(1)
  {
    server.handleClient();
    if (g_shutdownRequested) {
      printf("[TaskWebserver] Shutdown requested, cleaning up...\n");
      vTaskDelete(NULL);   // <--- Task beendet sich selbst
    }
    vTaskDelay(10);
  }
}

void handle_NotFound(){
  if (DEBUG>0) Serial.println(" HTTP: Page not Found");
  server.send(404, "text/plain", "Not found");
}

void xml_add(const String title, String value, String *ptr) {
  xml_replace(&value);
  *ptr +="<"; *ptr +=title; *ptr +=">";
  *ptr +=value;
  *ptr +="</"; *ptr +=title; *ptr +=">\n";
}

void handle_xml_refresh() { 
  String tmp_str;
  STATION_INFO *station_info;
  station_info = audio_get_station();
  String ptr = "<?xml version = \"1.0\" ?>\n";
  ptr +="<inputs>\n";
  xml_add("station_title", String(station_info->station), &ptr);
  xml_add("streaminfo", String(station_info->streaminfo), &ptr);     //cordump if modify!!!!
  xml_add("streamtitle",String(station_info->streamtitle),&ptr);
  xml_add("bitrate",String(station_info->bitrate),&ptr);
  xml_add("icyurl",String(station_info->icyurl),&ptr);
  xml_add("lasthost",String(station_info->lasthost),&ptr);
  xml_add("wr_wl",String(webradio_workload()),&ptr);
  tmp_str=get_volumen();
  xml_add("vol",tmp_str,&ptr);
  xml_add("time2",String(time_get_operation_Time()),&ptr);
  xml_add("bat_level",String(bat_get_level()),&ptr);
  xml_add("bat_voltage",String(bat_get_voltage()),&ptr);
  xml_add("heap",  String(ESP.getFreeHeap()) , &ptr);
  char buffer[17];
  time_getlocal_htmlDateTime(buffer,TIME_ALARM0);
  xml_add("alarm_time",  String(buffer) , &ptr);
  xml_add("alarm_byte",  String(eeprom_read_alarm_status()) , &ptr);
  xml_add("alarmVol",  String(eeprom_read_alarm_volumen()) , &ptr);
  xml_add("rgb_time",       String(rgb_get_time())        , &ptr);
  xml_add("rgb_brightness", String(rgb_get_brightness())  , &ptr);
  xml_add("rgb_pattern",    String(rgb_get_active_palette())  , &ptr);
    char hostname_read[HOSTNAME_LENGTH];
  read_hostname(hostname_read);
  xml_add("hostname",    String(hostname_read), &ptr);
  ptr +="</inputs>\n";

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "text/xml", ptr);
}



void handle_hostname() {
  char hostname_read[HOSTNAME_LENGTH];
  String tmp_str;
  if (server.hasArg("hostname")) {
  String s = server.arg("hostname");
  safeCopy(hostname_read, HOSTNAME_LENGTH, s);
    //Check if the hostname is valid
    if ( isValidHostname(hostname_read))  {
      if (DEBUG>0) Serial.print("Write Hostname into EEPROM:  ");
      if (DEBUG>0) Serial.println(hostname_read);
      write_hostname(hostname_read);
    }else{
      if (DEBUG>0) Serial.print("Skip unvalid Hostname: ");
      if (DEBUG>0) Serial.println(hostname_read);
    }
  }
}

void handle_channel() {
  int8_t  active_channel=0;
  char url_read[URL_LENGTH];
  String tmp_str;

  if (server.hasArg("channel")) {
    uint8_t tmp=server.arg("channel").toInt();
    if((tmp>=0) and (tmp<=7)) active_channel=tmp;
  }
  if (server.hasArg("setornotset")) {
    if(server.arg("setornotset").toInt()==1){
      if (server.hasArg("url")) {
        //Set URL
        String s = server.arg("url");
        safeCopy(url_read, URL_LENGTH, s);
        tmp_str=server.arg("channel");
        if (DEBUG>0) Serial.print("Write into EEPROM Channel ");
        if (DEBUG>0) Serial.print(active_channel);
        if (DEBUG>0) Serial.print("   Data: ");
        if (DEBUG>0) Serial.println(url_read);
        //Write URL into EEPROM:
        if(write_url(active_channel,url_read)!=0) Serial.println("Error Writing EEPROM");
        //Activate Channel by setting bit in ACTIVAT_CHANNEL in EEPROM:
        if(eeprom_write_station_active(eeprom_read_station_active()|1<<(active_channel))!=0) Serial.println("Error Writing EEPROM"); 
      }
    }else if(server.arg("setornotset").toInt()==0){
      //Cleare
      if ((~(1<<active_channel)&eeprom_read_station_active())==0){
        if (DEBUG>0) Serial.println("Clear not failed, because it is the last active Cahnnel!");
      }else{
        eeprom_write_station_active(~(1<<active_channel)&eeprom_read_station_active());
      }
    }        
  }

  String ptr = "<?xml version = \"1.0\" ?>\n";
  ptr +="<inputs>\n";
  
  read_url(active_channel,url_read);
  xml_add("url",    (eeprom_read_station_active()>>active_channel)&1>0 ? String(url_read) : "" , &ptr);
  xml_add("channel", String(active_channel)  , &ptr);
  xml_add("act",    String((eeprom_read_station_active()>>active_channel)&1), &ptr);
  ptr +="</inputs>\n";

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "text/xml", ptr);
}


void handle_xml_init() { 
  String tmp_str;
  STATION_INFO *station_info;
  station_info = audio_get_station();
  String ptr = "<?xml version = \"1.0\" ?>\n";
  ptr +="<inputs>\n";
  xml_add("balance",    String(get_balance())   , &ptr);
  xml_add("tone_low",   String(get_tone_low())  , &ptr);
  xml_add("tone_band",  String(get_tone_band()) , &ptr);
  xml_add("tone_high",  String(get_tone_high()) , &ptr);
  //xml_add("ip",  String(get_tone_high()) , &ptr);
  xml_add("bat0",  String(bat_get_level_startup()) , &ptr);
  xml_add("bat1",  String(time_get_operation_Time()) , &ptr);
  xml_add("time1",  String(time_get_last_sleep_time()) , &ptr);
  xml_add("compV",  String(__VERSION__) , &ptr);
  xml_add("compD",  String(String(__DATE__) +" at " + String(__TIME__)) , &ptr);

  char url_read[URL_LENGTH];
  read_url(0,url_read);
  xml_add("url",    String(url_read), &ptr);
  xml_add("act",    String((eeprom_read_station_active())&1), &ptr);
  char hostname_read[HOSTNAME_LENGTH];
  read_hostname(hostname_read);
  xml_add("hostname",    String(hostname_read), &ptr);
  ptr +="</inputs>\n";

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "text/xml", ptr);
}
 void handle_Log(){ 
  String filename="logfile0.current.log";
  File download = SPIFFS.open("/"+filename,  "r");
  
  if (download) {
    // Alle Header VOR dem Senden setzen
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
    server.sendHeader("Connection", "close");

    // streamFile setzt Content-Type selbst über den 2. Parameter
    size_t sent = server.streamFile(download, "text/plain"); 
    // Alternativ erzwingen: "application/octet-stream"

    download.close();
  } else {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "Can not Open File");
  }
}

void handle_alarm() { 
  #define ALARM_ACTIVE_MASK 128
  #define ALARM_MO_MASK 1
  #define ALARM_TU_MASK 2
  #define ALARM_WE_MASK 4
  #define ALARM_TH_MASK 8
  #define ALARM_FR_MASK 16
  #define ALARM_SA_MASK 32
  #define ALARM_SO_MASK 64
  char buffer[17];
  uint8_t alarm_status=eeprom_read_alarm_status();

  
  if (server.hasArg("alarmVol")){eeprom_write_alarm_volumen(server.arg("alarmVol").toInt());} 
  if (server.hasArg("alarm_act")){if (server.arg("alarm_act").equals("false"))  {time_clearAlarm(); alarm_status&=~ALARM_ACTIVE_MASK;}}
  if (server.hasArg("alarm_time")){
    if (server.arg("alarm_time").length()==16)
    {
      strcpy(buffer, server.arg("alarm_time").c_str());
      if (DEBUG>0) Serial.print("Get Value Datetime: ");
      if (DEBUG>0) Serial.println(buffer);
      time_setAlarm(buffer);
      alarm_status|=ALARM_ACTIVE_MASK;
    }else
    {
      if (DEBUG>0) Serial.print("Datetime wrong format. Length is: ");
      if (DEBUG>0) Serial.print(server.arg("alarm_time").length()); 
      if (DEBUG>0) Serial.print("    Value is:");
      if (DEBUG>0) Serial.println(server.arg("alarm_time").c_str());          
     }
  }
  if (server.hasArg("alarm_mo") ){ if (server.arg("alarm_mo").equals("true"))  {alarm_status|=ALARM_MO_MASK;}else{alarm_status&=~ALARM_MO_MASK;}}
  if (server.hasArg("alarm_tu") ){ if (server.arg("alarm_tu").equals("true"))  {alarm_status|=ALARM_TU_MASK;}else{alarm_status&=~ALARM_TU_MASK;}} 
  if (server.hasArg("alarm_we") ){ if (server.arg("alarm_we").equals("true"))  {alarm_status|=ALARM_WE_MASK;}else{alarm_status&=~ALARM_WE_MASK;}}
  if (server.hasArg("alarm_th") ){ if (server.arg("alarm_th").equals("true"))  {alarm_status|=ALARM_TH_MASK;}else{alarm_status&=~ALARM_TH_MASK;}}
  if (server.hasArg("alarm_fr") ){ if (server.arg("alarm_fr").equals("true"))  {alarm_status|=ALARM_FR_MASK;}else{alarm_status&=~ALARM_FR_MASK;}}
  if (server.hasArg("alarm_sa") ){ if (server.arg("alarm_sa").equals("true"))  {alarm_status|=ALARM_SA_MASK;}else{alarm_status&=~ALARM_SA_MASK;}}
  if (server.hasArg("alarm_so") ){ if (server.arg("alarm_so").equals("true"))  {alarm_status|=ALARM_SO_MASK;}else{alarm_status&=~ALARM_SO_MASK;}}
  eeprom_write_alarm_status(alarm_status);
  if (DEBUG>0) Serial.print("Write Alarm Status to value: ");
  if (DEBUG>0) Serial.println(alarm_status);
}

#ifdef IMAGECOVER

void init_cover_image()
{
    if (!SPIFFS.begin(true)) {   // dazu gleich eine Anmerkung
        Serial.println("[cover] SPIFFS mount failed");
        return;
    }

    if (SPIFFS.exists("/cover.jpg")) {
        Serial.println("[cover] /cover.jpg already exists");
        File test = SPIFFS.open("/cover.jpg", "r");
        if (test) {
            Serial.printf("[cover] existing cover size: %u bytes\n", (unsigned)test.size());
            test.close();
        }
        return;
    }

    File f = SPIFFS.open("/cover.jpg", FILE_WRITE);
    if (!f) {
        Serial.println("[cover] Failed to open /cover.jpg for write");
        return;
    }

    Serial.printf("[cover] default_cover_jpg_len = %u\n", (unsigned)default_cover_jpg_len);

    size_t written = f.write(default_cover_jpg, default_cover_jpg_len);
    f.close();

    Serial.printf("[cover] written = %u bytes\n", (unsigned)written);

    if (written == default_cover_jpg_len) {
        Serial.println("[cover] default 4x4 cover written OK");
    } else {
        Serial.println("[cover] ERROR: not all bytes written!");
    }
}



void handle_image() {
    Serial.printf("[web] SPIFFS used=%u total=%u\n",
                  (unsigned)SPIFFS.usedBytes(),
                  (unsigned)SPIFFS.totalBytes());

    bool exists = SPIFFS.exists("/cover.jpg");
    Serial.printf("[web] exists(/cover.jpg) = %s\n", exists ? "true" : "false");

    File img = SPIFFS.open("/cover.jpg", "r");   // NICHT mehr mit String-Bastelei

    if (!img) {
        Serial.println("[web] SPIFFS.open(/cover.jpg, r) FAILED");
        server.sendHeader("Connection", "close");
        server.send(404, "text/plain", "Cover image not found");
        return;
    }

    size_t size = img.size();
    Serial.printf("[web] cover.jpg size on SPIFFS: %u bytes\n", (unsigned)size);

    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.sendHeader("Connection", "close");

    size_t sent = server.streamFile(img, "image/jpeg");
    img.close();

    Serial.printf("[web] Sent cover image (%u bytes)\n", (unsigned)sent);
}


#endif
