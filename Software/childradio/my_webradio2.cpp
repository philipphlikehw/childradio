#include <Arduino.h>
#include "Audio.h"
#include "my_encoder.h"
#include "my_eeprom.h"
#include "my_common.h"
#include "my_time.h"
#include "my_bat.h"
#include <ESP32Ping.h>
#include <SpiffsFilePrint.h>  // https://github.com/PRosenb/SPIFFS_FilePrint
extern SpiffsFilePrint filePrint;
#define Serial filePrint

void my_audio_info(Audio::msg_t m);
extern int DEBUG;

// Define volume control pot connection
// ADC3 is GPIO 39
// const int volControl = 21;
 
// Integer for volume level
int volume = 21;
 
// Create audio object
Audio audio;

bool run_task;
bool vol_changed=false;
bool trigger_tornoff=false;

// Audio status functions

typedef struct {
  char station[64];
  char streaminfo[128];
  char streamtitle[128];
  char bitrate[16];
  char commercial[16];
  char icyurl[128];
  char lasthost[256];
  bool s_ready;
} STATION_INFO;

STATION_INFO* audio_get_station(void);

static STATION_INFO station_info;   // nur in dieser Übersetzungseinheit sichtbar

STATION_INFO* audio_get_station(void) {
  return &station_info;
}
bool webradio_trigger_tornoff(void) { return trigger_tornoff; }


typedef File (*webradio_cover_file_factory_t)();

static webradio_cover_file_factory_t g_cover_file_factory = nullptr;
void webradio_register_cover_file_factory(webradio_cover_file_factory_t fn) {
    g_cover_file_factory = fn;
}

// Einmal global (oder in derselben Datei vor der Nutzung):
#define CPY_FIELD(dst, src) cpy_field((dst), sizeof(dst), (src), #dst)

// Deine bestehende cpy_field mit field_name-Parameter:
static inline void cpy_field(char *dst, size_t dstsz, const char *src, const char *field_name) {
  if (!dst || dstsz == 0) return;

  if (!src) {
    dst[0] = '\0';
    Serial.printf("[cpy_field] %s <- (null)\n", field_name ? field_name : "?");
    return;
  }

  strncpy(dst, src, dstsz - 1);
  dst[dstsz - 1] = '\0';
  Serial.printf("[cpy_field] %s <- \"%s\"\n", field_name ? field_name : "?", dst);
}



int8_t get_balance(void){
  return audio.getBalance();
}
void set_balance(int8_t bal){
  audio.setBalance(bal);
  eeprom_write_balance(bal);
}


void stop_webradio(void){
  run_task=false;
}


int8_t get_tone_high(void){
  int8_t m_gain_low;
  int8_t m_gain_band;
  int8_t m_gain_high;
  audio.getTone(&m_gain_low, &m_gain_band, &m_gain_high);
  return m_gain_high;
}

int8_t get_tone_band(void){
  int8_t m_gain_low;
  int8_t m_gain_band;
  int8_t m_gain_high;
  audio.getTone(&m_gain_low, &m_gain_band, &m_gain_high);
  return m_gain_band;
}

int8_t get_tone_low(void)
{
  int8_t m_gain_low;
  int8_t m_gain_band;
  int8_t m_gain_high;
  audio.getTone(&m_gain_low, &m_gain_band, &m_gain_high);
  return m_gain_low;
}


void set_tone_high(int8_t low,int8_t band, int8_t high){
  int8_t m_gain_low=low;
  int8_t m_gain_band=band;
  int8_t m_gain_high=high;
  audio.setTone(m_gain_low, m_gain_band, m_gain_high);
  eeprom_write_gain_low(m_gain_low);
  eeprom_write_gain_band(m_gain_band);
  eeprom_write_gain_high(m_gain_high);
}

void set_tone_high(int8_t val){ eeprom_write_gain_high(val);    audio.setTone(eeprom_read_gain_low(), eeprom_read_gain_band(),  val);}
void set_tone_band(int8_t val){ eeprom_write_gain_band(val);    audio.setTone(eeprom_read_gain_low(), val,                      eeprom_read_gain_high());}
void set_tone_low(int8_t val) { eeprom_write_gain_low(val);    	audio.setTone(val,                    eeprom_read_gain_band(),  eeprom_read_gain_high());}

uint8_t get_volumen(void){
  return audio.getVolume();
}
void set_volumen(uint8_t p_vol)
{
  if (DEBUG&2>0) Serial.print("Set new Volumen to ");
  if (DEBUG&2>0) Serial.println(p_vol%22);
  rotary_setup(p_vol%22);
  if (p_vol>0) eeprom_write_volumen(p_vol%22);
  audio.setVolume(p_vol%22);
  bool vol_changed=true;
  
}

bool webradio_is_active(){
  
  if (audio.getVolume()==0) return false;
  if (audio.inBufferFilled() > 0 ) return true;
  return false;
}


unsigned long exe_time_start=millis();
unsigned long exe_time_stop=millis();
unsigned long exe_time_idle=millis();
unsigned long exe_time_run=millis();

uint8_t webradio_workload(void){ if ((exe_time_idle+exe_time_run)>0) {return (100*exe_time_run)/(exe_time_idle+exe_time_run);} else { return 100;} }


static volatile bool g_shutdownRequested = false;

void WEBRADIO_shutdown(void){
  g_shutdownRequested=true;
}


void webradio_handel(void *pameterar) {
  uint8_t channel;    //Read last Channel-Number without pre-increment
  uint8_t connection_retry;
  unsigned long lastRefreshTime = millis();
  char url[64];


  int8_t          m_gain_low    = eeprom_read_gain_low();
  int8_t          m_gain_band   = eeprom_read_gain_band();
  int8_t          m_gain_high   = eeprom_read_gain_high();
  int8_t          m_balance     = eeprom_read_balance();

  pinMode(MAX98357A_SD, OUTPUT);
  digitalWrite(MAX98357A_SD, LOW);
  
  audio.setTone(m_gain_low, m_gain_band, m_gain_high);
  audio.setBalance(m_balance);
  
  if ((eeprom_read_high_gain()&1)>0){
    pinMode(MAX98357A_GAIN, OUTPUT);    // sets the digital pin 13 as output
    digitalWrite(MAX98357A_GAIN, LOW);
  }else{
    pinMode(MAX98357A_GAIN, INPUT);    // sets the digital pin 13 as output
  }
  digitalWrite(MAX98357A_SD, HIGH);


   
  // Connect MAX98357 I2S Amplifier Module
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  //read gain values
  if (DEBUG&2>0) Serial.println("Gain Values are:");
  if (DEBUG&2>0) Serial.print("  Low: ");
  if (DEBUG&2>0) Serial.println(m_gain_low);
  if (DEBUG&2>0) Serial.print("  Band: ");
  if (DEBUG&2>0) Serial.println(m_gain_band);
  if (DEBUG&2>0) Serial.print("  High: ");
  if (DEBUG&2>0) Serial.println(m_gain_high);
  if (DEBUG&2>0) Serial.print("  Balance: ");
  if (DEBUG&2>0) Serial.println(m_balance);
  if (DEBUG&2>0) Serial.flush();
  
  // Set the volume
  audio.setVolume(eeprom_read_volumen());
  channel = read_station(false);
  read_url(channel,url);
  //audio.setBufsize(-1,2*1024*1024);

  // Connect to an Internet radio station (select one as desired)

  connection_retry=0;
  lastRefreshTime=millis();
  do{
    if((millis() - lastRefreshTime >= WEBARDAIO_CONNECTION_TI)){
      lastRefreshTime=millis();
      connection_retry++;
      if ((connection_retry>WEBRADIO_MAX_CONNECTION_RETRY))
      {                             //No Connection was possible
        channel=read_station(true); //read next Station-Number
        read_url(channel,url);      //put url-adress into the char url
      }
      if ((connection_retry>WEBRADIO_MAX_CONNECTION_RETRY_REINIT_EEPROM)) { 
        if (Ping.ping("www.google.com", 3)){                                                  //Check if a Internet Connection is established
          connection_retry=0;
          eeprom_init();
          channel=0;
          read_url(channel,url); 
        }
      }
    }
    yield();
  }while(!audio.connecttohost(url));
  
  

  uint8_t trigger_next_station=0;
  uint8_t old_vol;
  
  Serial.print("Webradio: Start Webradio Task on CPU ");
  Serial.println(xPortGetCoreID());
  run_task=true;

  memset(&station_info, 0, sizeof(station_info));  // alles auf 0 setzen
  strcpy(station_info.station, "unbekannt");
  station_info.s_ready = false;
  audio.audio_info_callback = my_audio_info;
  while(run_task){
    // Run audio player
    exe_time_idle=millis()-exe_time_stop;
    exe_time_start=millis();
    audio.loop();
    // Set the volume
    
    if (encoderChanged())
    {
      if (DEBUG&8>0) Serial.print("Rotary Encoder value: ");
      if (DEBUG&8>0) Serial.println(rotary_getVal());
      audio.setVolume(rotary_getVal()%(VOL_MAX+1));
      vol_changed=true;
      lastRefreshTime = millis();
    }

    if (isEncoderButtonClicked()){
      //next_station();
      if (DEBUG&8>0) Serial.println("Rotary Encoder clicked ");
      if (trigger_next_station==0){
        trigger_next_station=1;
        old_vol = audio.getVolume();
        channel = read_station(true);
        read_url(channel,url);
      }
    }

    if (trigger_next_station!=0){
      switch ( trigger_next_station )
      {
        case 1:
          if((millis() - lastRefreshTime >= TONE_DIMMING)){
            lastRefreshTime=millis();
            if (audio.getVolume()==0) {
              trigger_next_station=2;
            }else{
              audio.setVolume(audio.getVolume()-1);
              lastRefreshTime = millis();
              connection_retry=0;
            }
          }break;
        case 2:
          if((millis() - lastRefreshTime >= WEBARDAIO_CONNECTION_TI)){
            lastRefreshTime=millis();
            if (audio.connecttohost(url)){                                 //Try to connect to URL
              trigger_next_station=3;                                         //Go futher if success 
            }else{                                                         //Else 
              connection_retry++;                                             //Increase the retry-counter
              if (connection_retry>WEBRADIO_MAX_CONNECTION_RETRY){           //Check if maximum count of retries is reached
                channel = read_station(true);                                     //increment the channel
                read_url(channel,url);                                            //Read URL
              }
              if ((connection_retry>WEBRADIO_MAX_CONNECTION_RETRY_REINIT_EEPROM)) {      //This should never happen! If no connection is possible.
                if (Ping.ping("www.google.com", 3)){                                                  //Check if a Internet Connection is established
                  connection_retry=0;
                  eeprom_init();
                  channel=0;
                  read_url(channel,url); 
                }
              } 
            }
          }
        break;
        case 3:
              if((millis() - lastRefreshTime >= WEBARDAIO_CONNECTION_TI)){
                lastRefreshTime=millis();
                if (station_info.s_ready==true) trigger_next_station=4;
              } 
        break;
        case 4:
              if((millis() - lastRefreshTime >= TONE_DIMMING)){
                lastRefreshTime=millis();
                if (audio.getVolume()==old_vol) {
                  trigger_next_station=0;   //final comand
                }else{
                  audio.setVolume(audio.getVolume()+1);
                }
              }break;
            default:
       break;
       }
    }
    if(vol_changed && (millis() - lastRefreshTime >= VOL_REFRESH_INTERVAL))
    {
      lastRefreshTime = millis();
      vol_changed=false;
      if ((rotary_getVal()%22)==0){
        if (DEBUG&8>0) Serial.println("Webradio: 10s muted, now go to Sleep Mode");
#if FALLSE
          esp_sleep_enable_ext1_wakeup(RTC_MFP_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
          esp_sleep_enable_ext0_wakeup(ROTARY_WAKUP_PIN, !digitalRead(ROTARY_WAKUP_PIN));

        #ifdef BAT_ACTIVE
          eeprom_write_time(time_getEpochTime());//ToDo
          eeprom_write_bat(bat_get_level());
        #endif
          digitalWrite(MAX98357A_SD, LOW);
          pinMode(MAX98357A_GAIN, INPUT);
          vTaskDelay(1000);
          esp_deep_sleep_start();
#else

        digitalWrite(MAX98357A_SD, LOW);
        pinMode(MAX98357A_GAIN, INPUT);
        trigger_tornoff=true;
#endif
      }else{
        eeprom_write_volumen(rotary_getVal()%22);
      }         
    }

    exe_time_stop=millis();
    exe_time_run=exe_time_stop-exe_time_start;
    //yield();
    // Pufferstand ermitteln
    uint32_t filled = audio.inBufferFilled();
    uint32_t free   = audio.inBufferFree();
    uint32_t cap    = filled + free;
    float     fill  = cap ? (float)filled / (float)cap : 0.0f;

    // --- 10s-Debug-Log ---
    static uint32_t last_dbg_ms = 0;
    uint32_t now_ms = millis();
    if (now_ms - last_dbg_ms >= 10000UL) {  // alle 10 s
      last_dbg_ms = now_ms;
      // nur loggen, wenn dein Webserver-Debugbit gesetzt ist – sonst einfach Serial.printf(...)
      if (DEBUG /* & DBG_WEBSERVER */) {
        Serial.printf("[WS] buf filled=%lu free=%lu cap=%lu fill=%.1f%%\n",
                      (unsigned long)filled,
                      (unsigned long)free,
                      (unsigned long)cap,
                      100.0f * fill);
      }
    }
    if (g_shutdownRequested) {
      printf("[TaskWebradio] Shutdown requested, cleaning up...\n");
      vTaskDelete(NULL);   // <--- Task beendet sich selbst
    }

    if (fill > 0.5f) {          // Kommentar sagt "<10%", ist aber ">50%"
      vTaskDelay(10);
    } else if (fill < 0.4f) {
      vTaskDelay(5);
    } else if (fill < 0.3f) {
      vTaskDelay(3);
    } else if (fill < 0.2f) {
      vTaskDelay(2);    
    }
  }
  audio.~Audio();
  station_info.s_ready=false;
  vTaskDelete(NULL);
}


void update_gain(){
  if (DEBUG&8>0) Serial.print("Webradio: Update Gain");
  digitalWrite(MAX98357A_SD, LOW);
  if ((eeprom_read_high_gain()&1)>0){
    pinMode(MAX98357A_GAIN, OUTPUT);    // sets the digital pin 13 as output
    digitalWrite(MAX98357A_GAIN, LOW);
  }else{
    pinMode(MAX98357A_GAIN, INPUT);    // sets the digital pin 13 as output
  }
  digitalWrite(MAX98357A_SD, HIGH);
}

void my_audio_info(Audio::msg_t m) {
  const char *msg = m.msg ? m.msg : "";

  switch (m.e) {
    case Audio::evt_info:
      if ((DEBUG & 2) > 0) { Serial.print("info        "); Serial.println(msg); }
      if (strcmp(msg, "stream ready") == 0) {
        station_info.s_ready = true;
      }
      break;

    case Audio::evt_id3data:
      if ((DEBUG & 2) > 0) { Serial.print("id3data     "); Serial.println(msg); }
      break;

    case Audio::evt_eof:
      if ((DEBUG & 2) > 0) { Serial.print("eof_mp3     "); Serial.println(msg); }
      break;

    case Audio::evt_name:
      if ((DEBUG & 2) > 0) { Serial.print("station     "); Serial.println(msg); }
      CPY_FIELD(station_info.station, msg);
      break;

    case Audio::evt_streamtitle:
      if ((DEBUG & 2) > 0) { Serial.print("streamtitle "); Serial.println(msg); }
      CPY_FIELD(station_info.streamtitle, msg);
      break;

    case Audio::evt_bitrate:
      if ((DEBUG & 2) > 0) { Serial.print("bitrate     "); Serial.println(msg); }
      CPY_FIELD(station_info.bitrate, msg);
      break;

    case Audio::evt_icyurl:
      if ((DEBUG & 2) > 0) { Serial.print("icyurl      "); Serial.println(msg); }
      CPY_FIELD(station_info.icyurl, msg);
      break;

    case Audio::evt_lasthost:
      if ((DEBUG & 2) > 0) { Serial.print("lasthost    "); Serial.println(msg); }
      CPY_FIELD(station_info.lasthost, msg);
      break;

    case Audio::evt_icylogo:
      if ((DEBUG & 2) > 0) { Serial.print("icylogo     "); Serial.println(msg); }
      break;

    case Audio::evt_icydescription:
      if ((DEBUG & 2) > 0) { Serial.print("icydescr    "); Serial.println(msg); }
      break;

    case Audio::evt_lyrics:
      if ((DEBUG & 2) > 0) { Serial.print("lyrics      "); Serial.println(msg); }
      break;

    case Audio::evt_image:
      if ((DEBUG & 2) > 0) {
        for (int i = 0; i + 1 < (int)m.vec.size(); i += 2) {
          Serial.printf("cover image:  segment %02i, pos %07lu, len %05lu\n",
            i / 2,
            (unsigned long)m.vec[i],
            (unsigned long)m.vec[i + 1]);
          }
        }
        
#ifdef IMAGECOVER
        if (g_cover_file_factory) {
          File out = g_cover_file_factory();
          if (!out) {
            Serial.println("[webradio] cover file factory returned invalid File");
            break;
          }

            // HIER: aus m.vec die Bilddaten lesen und in 'out' schreiben
            // Das hängt davon ab, wie du an die Bytes kommst (SD, Stream, usw.)
            // Beispiel: du hast schon eine Funktion, die die Segmente in einen Stream schreibt:
            //
            //   write_image_segments_to_stream(m.vec, out);
            //
            // Dann:
            // write_image_segments_to_stream(m.vec, out);

          out.close();
          Serial.println("[webradio] cover image written via factory");
      } else {
          Serial.println("[webradio] no cover file factory registered");
      }
#endif
      break;

    case Audio::evt_log:
      if ((DEBUG & 2) > 0) { Serial.print("audio_logs  "); Serial.println(msg); }
      break;

    default:
      if (strstr(msg, "commercial") != nullptr) {
        if ((DEBUG & 2) > 0) { Serial.print("commercial  "); Serial.println(msg); }
        char digits[16] = {0};
        size_t di = 0;
        for (const char *p = msg; *p && di < sizeof(digits) - 1; ++p) {
          if (*p >= '0' && *p <= '9') digits[di++] = *p;
        }
        CPY_FIELD(station_info.commercial, digits);
      } else {
        if ((DEBUG & 2) > 0) { Serial.print("message     "); Serial.println(msg); }
      }
      break;
  }
}
