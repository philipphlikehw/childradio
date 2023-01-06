#include <WebServer.h>
#include <Update.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>

#include "my_common.h"
#include "my_time.h"
#include "my_encoder.h"

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

extern int DEBUG;

// Function headers
void setup_webserver();
void loop_webserver();
void handle_OnConnect();
void handle_NotFound();
String SendHTML();

// Global variables
WebServer server(80);

void forcedReset(){
// use the watchdog timer to do a hard restart
// It sets the wdt to 1 second, adds the current process and then starts an
// infinite loop.
  esp_task_wdt_init(1, true);
  esp_task_wdt_add(NULL);
  while(true);  // wait for watchdog timer to be triggered
}

//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup_webserver(){
  server.on("/", HTTP_GET, handle_OnConnect);
  server.onNotFound(handle_NotFound);

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "NOK" : "OK");
    eeprom_write_time(time_getEpochTime());//ToDo
    eeprom_write_bat(bat_get_level());
    vTaskDelay(1000);
    //ESP.restart();
    forcedReset();      //For Battery based Systems, a restart have to be triggered by watchdog
  }, []() {
   
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {

      stop_webradio();
      if (DEBUG>0) Serial.print("Wait until Webradio is closed ");
      while(webradio_is_active()){
        vTaskDelay(500);
        if (DEBUG>0) Serial.print(". ");
      }
      if (DEBUG>0) Serial.println(" done.");
    
      Serial.setDebugOutput(true);
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (1048576 - 0x1000) & 0xFFFFF000;
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
      //if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });

  
  server.begin();
  if (DEBUG>0) Serial.println(" HTTP: server started");  
}

void loop_webserver(void *parameter){
  if (DEBUG>0) Serial.print("Start WebServer Task on CPU ");
  if (DEBUG>0) Serial.println(xPortGetCoreID());
  if (DEBUG>0) Serial.flush();
  while(1)
  {
    server.handleClient();
    vTaskDelay(50);
  }
}


void handle_OnConnect() {
  if (DEBUG>0) Serial.println(" HTTP: Connection established");
  server.send(200, "text/html", SendHTML());  
}

void handle_NotFound(){
  if (DEBUG>0) Serial.println(" HTTP: Page not Found");
  server.send(404, "text/plain", "Not found");
}

String SendHTML(){
  char url_read[URL_LENGTH];
  char channel;
  String tmp_str;
  String ptr = "<!DOCTYPE html> <html>\n";

    ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
    ptr +="<title>Kinderradio</title>\n";
    ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
    ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
    ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
    ptr +="</style>\n";
    ptr +="</head>\n";
    ptr +="<body>\n";
    ptr +="<div id=\"webpage\">\n";
    ptr +="<h1>Webradio</h1>\n";
    ptr +="<table>\n";
    ptr +="<tr>\n";
    ptr +="<form action=\"/\" method=\"get\">";
    ptr +="<label for=\"URL\">URL: </label>";
    ptr +=  "<input type=\"text\"   name=\"url\">";
    ptr += "<select name=\"Channel\">";
    for(int i=0;i<URL_COUNT;i++)
    {    
      ptr += "<option value=\"";
      ptr += i;
      ptr += "\">";
      ptr += "Channel ";
      ptr += i;
      ptr += "</option>";
    }
    ptr += "</select>";
    ptr +=  "<input type=\"submit\" name=\"update\" value=\"Submit\">";
    ptr +="</form>";
     ptr +="</tr>\n";

  if (server.hasArg("update"))
  {
      tmp_str=server.arg("url");
      for(int i=0;i<URL_LENGTH;i++){url_read[i]=tmp_str[i];}
      tmp_str=server.arg("Channel");
      channel=tmp_str[0];
      /*
      ptr +=url_read;
      ptr += " ";
      ptr +=channel;
      */
      if (DEBUG>0) Serial.print("Write into EEPROM Channel ");
      if (DEBUG>0) Serial.print(channel);
      if (DEBUG>0) Serial.print("   Data: ");
      if (DEBUG>0) Serial.println(url_read);
      //Write URL into EEPROM:
      if(write_url(channel-48,url_read)!=0) Serial.println("Error Writing EEPROM");
      //Activate Channel by setting bit in ACTIVAT_CHANNEL in EEPROM:
      if(eeprom_write_station_active(eeprom_read_station_active()|1<<(channel-48))!=0) Serial.println("Error Writing EEPROM");
     
  }

#ifdef EEPROM_INFO

  if (server.hasArg("clear"))
  {
    tmp_str=server.arg("clear");
    uint8_t m_clear=tmp_str.toInt();
      if ((~(1<<m_clear)&eeprom_read_station_active())==0){
        if (DEBUG>0) Serial.println("Clear not faild, because it is the last active Cahnnel!");
      }else{
        eeprom_write_station_active(~(1<<m_clear)&eeprom_read_station_active());
      }
    }

    
  ptr +="<tr>\n";
  ptr +="<p><p/>\n<table>";
  for(int i=0;i<URL_COUNT;i++)
  {
    if (((eeprom_read_station_active()>>i)&1)==1)
    {
      read_url(i,url_read);
      
      ptr +="<form action=\"\" method=\"get\">";
      ptr +="<tr>  <th align=\"left\">Channel ";
      ptr +=i;
      ptr +="</th>\n<td align=\"left\">\n";
      ptr +=url_read;
      ptr +="</td>\n<th>";
      ptr +=  "<button type=\"submit\" name=\"clear\" value=\"";
      ptr +=i;
      ptr +="\">clear</button></form>\n</th>\n</tr>\n";
    }    
  }
  ptr +="</table>";
  ptr +="/<tr>\n";
#endif


#ifdef WEBRADIO_INFO
   STATION_INFO *station_info;
   station_info = audio_get_station();
   ptr +="<tr></tr>\n";
   ptr +="<tr></tr>\n";
   ptr +="<tr>\n <h3 align=\"left\">Station Info:</h3></tr>";
   ptr +="<tr>\n";
   ptr +="<p><p/>\n<table>";
   ptr +="<tr>  <th align=\"left\">Station</th>\n<td align=\"left\">\n";
   ptr +=station_info->station;
   ptr +="</td >\n</tr>\n";

   ptr +="<tr>  <th align=\"left\">Streaminfo</th>\n<td align=\"left\">\n";
   ptr +=station_info->streaminfo;
   ptr +="</td>\n</tr>\n";

   ptr +="<tr>  <th align=\"left\">Streamtitle</th>\n<td align=\"left\">\n";
   ptr +=station_info->streamtitle;
   ptr +="</td>\n</tr>\n";

   ptr +="<tr>  <th align=\"left\">Bitrate</th>\n<td align=\"left\">\n";
   ptr +=station_info->bitrate;
   ptr +="</td>\n</tr>\n";

   ptr +="<tr>  <th>Commercial</th>\n<td>\n";
   ptr +=station_info->commercial;
   ptr +="</td>\n</tr>\n";

   ptr +="<tr>  <th align=\"left\">icyurl</th>\n<td align=\"left\">\n";
   ptr +=station_info->icyurl;
   ptr +="</td>\n</tr>\n";

   ptr +="<tr>  <th align=\"left\">lasthost</th>\n<td align=\"left\">\n";
   ptr +=station_info->lasthost;
   ptr +="</td>\n</tr>\n";

   
   ptr +="</table>";
   ptr +="</tr>\n";
#endif


#define ALARM
#ifdef ALARM
    uint8_t alarm_status = 0;
    char buffer[17];
    #define ALARM_ACTIVE_MASK 128
    #define ALARM_MO_MASK 1
    #define ALARM_TU_MASK 2
    #define ALARM_WE_MASK 4
    #define ALARM_TH_MASK 8
    #define ALARM_FR_MASK 16
    #define ALARM_SA_MASK 32
    #define ALARM_SO_MASK 64

    #define HIGH_GAIN_MASK 1

  if (server.hasArg("alarm"))
  {

      int8_t m_alarm_vol=0;
      alarm_status=0;
      
      tmp_str=server.arg("vol");
      m_alarm_vol=tmp_str.toInt();
      eeprom_write_alarm_volumen(m_alarm_vol);

      tmp_str=server.arg("alarm");             
      if (tmp_str.equals("on"))
      {
        //Enable the Alarm to the given Appointment
        alarm_status+=ALARM_ACTIVE_MASK;
        tmp_str=server.arg("datetime"); 
        if (tmp_str.length()==16)
        {
          strcpy(buffer, tmp_str.c_str());
          if (DEBUG>0) Serial.print("Get Value Datetime: ");
          if (DEBUG>0) Serial.println(tmp_str);
          time_setAlarm(buffer);
        }else
        {
          if (DEBUG>0) Serial.print("Datetime wrong format. Length is: ");
          if (DEBUG>0) Serial.print(tmp_str.length()); 
          if (DEBUG>0) Serial.print("    Value is:");
          if (DEBUG>0) Serial.println(tmp_str);          
        }
      }else{
        //Disable the ALARM!
        time_clearAlarm();
      }

      tmp_str=server.arg("monday");  
      if (tmp_str.equals("on")) alarm_status+=ALARM_MO_MASK;

      tmp_str=server.arg("tuesday");  
      if (tmp_str.equals("on"))  alarm_status+=ALARM_TU_MASK;

      tmp_str=server.arg("wednesday");  
      if (tmp_str.equals("on"))  alarm_status+=ALARM_WE_MASK;

      tmp_str=server.arg("thursday");  
      if (tmp_str.equals("on"))  alarm_status+=ALARM_TH_MASK;

      tmp_str=server.arg("friday");  
      if (tmp_str.equals("on"))  alarm_status+=ALARM_FR_MASK;

      tmp_str=server.arg("saterday");  
      if (tmp_str.equals("on"))  alarm_status+=ALARM_SA_MASK;

      tmp_str=server.arg("sonday");  
      if (tmp_str.equals("on"))  alarm_status+=ALARM_SO_MASK;

      eeprom_write_alarm_status(alarm_status);

      if (DEBUG>0) Serial.print("Write Alarm Status to value: ");
      if (DEBUG>0) Serial.println(alarm_status);
  }

    alarm_status = eeprom_read_alarm_status();
    
    ptr +="<tr></tr>\n";
    ptr +="<tr></tr>\n";
    ptr +="<tr>\n <h3 align=\"left\">Wecker:</h3></tr>";
    ptr +="<tr>\n";
    ptr +="<table>\n";
    ptr +="<form action=\"/\" method=\"get\">\n";

    //Datetime Calender
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Datum und Uhrzeit: </label></th>\n";
    ptr +="<td align=\"left\">\n  <input type=\"datetime-local\" id=\"alarm-time\"  name=\"datetime\" value=\"";
    time_getlocal_htmlDateTime(buffer,TIME_ALARM0);
    ptr += buffer;ptr += "\"";
    time_getlocal_htmlDateTime(buffer,TIME_NOW);
    ptr +=" min=\"";
    ptr += buffer;ptr += "\"";
    ptr += "></td>\n</tr>\n"; 

    //Active
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Wecker an: </label></th>\n";
   // ptr +="<input type=\"hidden\" name=\"alarm\" value=\"off\">";
    ptr +="<td align=\"left\">\n<input type=\"checkbox\" name=\"alarm\" value=\"on\" ";      //checked>
    if ((alarm_status&ALARM_ACTIVE_MASK)>0) ptr +="checked ";
    ptr +="></td>\n</tr>\n";

    //Volumen
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Volumen: </label></th>\n";
    ptr +="<td align=\"left\">\n<input type=\"range\" min=\"1\" max=\"21\" step=\"1\" name=\"vol\" value=\"";
    ptr +=eeprom_read_alarm_volumen()%22;
    ptr += "\"></td>\n</tr>\n"; 

    //Repeat:
    ptr +="<tr><th align=\"left\"></th><td></td>\n</tr>\n";
    ptr +="<tr><th align=\"left\"><label>Wierderhole: </label></th><td></td>\n</tr>\n";
    
    //Monday
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Montag: </label></th>\n";
    //ptr +="<input type=\"hidden\" name=\"monday\" value=\"off\">";
    ptr +="<td align=\"left\">\n<input type=\"checkbox\" name=\"monday\" value=\"on\" ";      //checked>
    if ((alarm_status&ALARM_MO_MASK)>0) ptr +="checked ";
    ptr +="></td>\n</tr>\n";

    //Tuesday
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Dienstag: </label></th>\n";
    //ptr +="<input type=\"hidden\" name=\"tuesday\" value=\"off\">";
    ptr +="<td align=\"left\">\n<input type=\"checkbox\" name=\"tuesday\" value=\"on\" ";      //checked>
    if ((alarm_status&ALARM_TU_MASK)>0) ptr +="checked ";
    ptr +="></td>\n</tr>\n";

    //Wednesday
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Mittwoch: </label></th>\n";
    //ptr +="<input type=\"hidden\" name=\"wednesday\" value=\"off\">";
    ptr +="<td align=\"left\">\n<input type=\"checkbox\" name=\"wednesday\" value=\"on\" ";      //checked>
    if ((alarm_status&ALARM_WE_MASK)>0) ptr +="checked ";
    ptr +="></td>\n</tr>\n";

    //Thursday
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Donnerstag: </label></th>\n";
    //ptr +="<input type=\"hidden\" name=\"thursday\" value=\"off\">";
    ptr +="<td align=\"left\">\n<input type=\"checkbox\" name=\"thursday\" value=\"on\" ";      //checked>
    if ((alarm_status&ALARM_TH_MASK)>0) ptr +="checked ";
    ptr +="></td>\n</tr>\n";

    //Friday
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Freitag: </label></th>\n";
    //ptr +="<input type=\"hidden\" name=\"friday\" value=\"off\">";
    ptr +="<td align=\"left\">\n<input type=\"checkbox\" name=\"friday\" value=\"on\" ";      //checked>
    if ((alarm_status&ALARM_FR_MASK)>0) ptr +="checked ";
    ptr +="></td>\n</tr>\n";

    //Saterday
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Samstag: </label></th>\n";
    //ptr +="<input type=\"hidden\" name=\"saterday\" value=\"off\">";
    ptr +="<td align=\"left\">\n<input type=\"checkbox\" name=\"saterday\" value=\"on\" ";      //checked>
    if ((alarm_status&ALARM_SA_MASK)>0) ptr +="checked ";
    ptr +="></td>\n</tr>\n";

    //Sunday
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Sonntag: </label></th>\n";
    //ptr +="<input type=\"hidden\" name=\"sonday\" value=\"off\">";
    ptr +="<td align=\"left\">\n<input type=\"checkbox\" name=\"sonday\" value=\"on\" ";      //checked>
    if ((alarm_status&ALARM_SO_MASK)>0) ptr +="checked ";
    ptr +="></td>\n</tr>\n";

    

    ptr += "<tr><th align=\"left\"><input type=\"submit\" name=\"alarm\" value=\"Senden\">\n</th>\n</tr>";
    ptr +="</form>\n";
    ptr +="</table>";
    ptr +="</tr>\n";       
#endif

#define SPEAKER
#ifdef SPEAKER

  int8_t m_balance;
  int8_t m_vol;
  int8_t m_gain_low;
  int8_t m_gain_band;
  int8_t m_gain_high;
  
  if (server.hasArg("audio"))
  {
      tmp_str=server.arg("balance");
      m_balance=tmp_str.toInt();

      tmp_str=server.arg("vol");
      m_vol=tmp_str.toInt();
      
      tmp_str=server.arg("lowpass");
      m_gain_low=tmp_str.toInt();
      
      tmp_str=server.arg("bandpass");
      m_gain_band=tmp_str.toInt();
      
      tmp_str=server.arg("highpass");
      m_gain_high=tmp_str.toInt();

      tmp_str=server.arg("highgain");  
      if (tmp_str.equals("on")) {
        if ((eeprom_read_high_gain()&1)==0){
          eeprom_write_high_gain(1);
          update_gain();
        }
      } else {
        if ((eeprom_read_high_gain()&1)==1){
          eeprom_write_high_gain(0);
          update_gain();
        }
      }
      
      
      if (DEBUG>0) Serial.print("Balance: ");
      if (DEBUG>0) Serial.println(m_balance);
      if (DEBUG>0) Serial.print("gain Lowpass: ");
      if (DEBUG>0) Serial.println(m_gain_low);
      if (DEBUG>0) Serial.print("Gain Bandpass: ");
      if (DEBUG>0) Serial.println(m_gain_band);
      if (DEBUG>0) Serial.print("Gain Highpass: ");
      if (DEBUG>0) Serial.println(m_gain_high);

      set_volumen(m_vol%(VOL_MAX+1));
      rotary_setup(m_vol%(VOL_MAX+1));
      set_balance(m_balance);
      set_tone(m_gain_low,m_gain_band,m_gain_high);
     
  }
    ptr +="<tr></tr>\n";
    ptr +="<tr></tr>\n";
    ptr +="<tr>\n <h3 align=\"left\">Equalizer:</h3></tr>";
    ptr +="<tr>\n";
    ptr +="<table>\n";
    ptr +="<form action=\"/\" method=\"get\">\n";
    
    //Volumen
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Volumen: </label></th>\n";
    ptr +="<td align=\"left\">\n<input type=\"range\" min=\"1\" max=\"21\" step=\"1\" name=\"vol\" value=\"";
    ptr +=get_volumen(); // Balance Value
    ptr += "\"></td>\n</tr>\n"; 

    //Balance
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Balance: </label></th>\n";
    ptr +="<td align=\"left\">\n<input type=\"range\" min=\"-16\" max=\"16\" step=\"1\" name=\"balance\" value=\"";
    ptr +=get_balance(); // Balance Value
    ptr += "\"></td>\n</tr>\n"; 
    
    //Tone Low
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Low Pass: </label></th>\n";
    ptr +="<td align=\"left\">\n<input type=\"range\" min=\"-40\" max=\"6\" step=\"1\" name=\"lowpass\" value=\"";
    ptr +=get_lowpass(); // Low Pass Band
    ptr += "\"></td>\n</tr>\n";

    //Tone Band Pass
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Band Pass: </label></th>\n";
    ptr +="<td align=\"left\">\n<input type=\"range\" min=\"-40\" max=\"6\" step=\"1\" name=\"bandpass\" value=\"";
    ptr +=get_bandpass(); // Band pass
    ptr += "\"></td>\n</tr>\n";

    //Tone High Pass
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>High Pass: </label></th>\n";
    ptr +="<td align=\"left\">\n<input type=\"range\" min=\"-40\" max=\"6\" step=\"1\" name=\"highpass\" value=\"";
    ptr +=get_highpass(); // HighPass
    ptr += "\"></td>\n</tr>\n";

    //Active High Gain
    ptr +="<tr><th align=\"left\">";
    ptr +="<label>Hoche Verst\&aumlrkung: </label></th>\n";
    ptr +="<td align=\"left\">\n<input type=\"checkbox\" name=\"highgain\" value=\"on\" ";      //checked>
    if ((eeprom_read_high_gain()&HIGH_GAIN_MASK)>0) ptr +="checked ";
    ptr +="></td>\n</tr>\n";
    
    ptr += "<tr><th align=\"left\"><input type=\"submit\" name=\"audio\" value=\"Submit\">\n</th>\n</tr>";
    ptr +="</form>\n";
    ptr +="</table>\n";
    ptr +="</tr>\n";

  
    
//<input type="range" min="-5" max="5" step="1.0" value="0">
#endif

#ifdef BAT_ACTIVE
    ptr +="<tr></tr>\n";
    ptr +="<tr></tr>\n";
    ptr +="<tr>\n <h3 align=\"left\">Battery:</h3></tr>";
    ptr +="<tr>\n";
    ptr +="<table>\n";

   ptr +="<tr>  <th align=\"left\">Level:</th>\n<td align=\"left\">\n";
   ptr +=bat_get_level();
   ptr +="%\n</td>\n</tr>\n";
   ptr +="<tr>  <th align=\"left\">Voltage:</th>\n<td align=\"left\">\n";
   ptr +=bat_get_voltage();
   ptr +="mV\n</td>\n</tr>\n";
    
    ptr +="</table>\n";
    ptr +="</tr>\n";
    
#endif

#define SYSTEM_INFO
#ifdef SYSTEM_INFO
    ptr +="<tr></tr>\n";
    ptr +="<tr>\n <h3 align=\"left\">System Information:</h3></tr>";
    ptr +="<tr>\n";
    ptr +="<table>\n";

    ptr +="<tr>  <th align=\"left\">Battery Level at Last Power-Down:</th>\n<td align=\"left\">\n";
    ptr +=bat_get_level_last();
    ptr +="%\n</td>\n</tr>\n";

    ptr +="<tr>  <th align=\"left\">Battery Level at Start-up:</th>\n<td align=\"left\">\n";
    ptr += bat_get_level_startup();
    ptr +="%\n</td>\n</tr>\n";

    ptr +="<tr>  <th align=\"left\">Operation Time:</th>\n<td align=\"left\">\n";
    ptr +=time_get_operation_Time();
    ptr +="\n</td>\n</tr>\n";

    ptr +="<tr>  <th align=\"left\">Sleep Time:</th>\n<td align=\"left\">\n";
    ptr +=time_get_last_sleep_time();
    ptr +="\n</td>\n</tr>\n";
  
    ptr +="<tr>  <th align=\"left\">Compiled with c++ version</th>\n<td align=\"left\">\n";
    ptr +=F(__VERSION__);
    ptr +="\n</td>\n</tr>\n";

    ptr +="<tr>  <th align=\"left\">on</th>\n<td align=\"left\">\n";
    ptr +=F(__DATE__);
    ptr +=" at ";
    ptr +=F(__TIME__);
    ptr +="\n</td>\n</tr>\n";


    ptr +="</table>\n";
    ptr +="</tr>\n";    
#endif



#define OTA_WEB
#ifdef OTA_WEB
    ptr +="<tr></tr>\n";
    ptr +="<tr>\n <h3 align=\"left\">System Update</h3></tr>";
    ptr +="<tr>\n";
    ptr +="<table>\n";
    
    ptr +="<tr align=\"left\">";    
    ptr += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
    ptr += "</tr>\n";
    
    ptr +="</table>\n";
    ptr +="</tr>\n";  
  
#endif
    ptr +="</table>\n";
    ptr +="</div>\n";
    ptr +="</body>\n";
    ptr +="</html>\n";
    
  return ptr;
}
