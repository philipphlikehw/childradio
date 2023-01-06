#include "Audio.h"
#include "my_encoder.h"
#include "my_eeprom.h"
#include "my_common.h"
#include "my_time.h"
#include "my_bat.h"
#include <ESP32Ping.h>

extern int DEBUG;

// Define volume control pot connection
// ADC3 is GPIO 39
// const int volControl = 21;
 
// Integer for volume level
int volume = 21;
 
// Create audio object
Audio audio;

bool run_task;

// Audio status functions

struct STATION_INFO {
  char station[64];
  char streaminfo[64];
  char streamtitle[64];
  char bitrate[8];
  char commercial[64];
  char icyurl[64];
  char lasthost[64];
  bool s_ready;
};

STATION_INFO station_info;

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


int8_t get_highpass(void){
  int8_t m_gain_low;
  int8_t m_gain_band;
  int8_t m_gain_high;
  audio.getTone(&m_gain_low, &m_gain_band, &m_gain_high);
  return m_gain_high;
}

int8_t get_bandpass(void){
  int8_t m_gain_low;
  int8_t m_gain_band;
  int8_t m_gain_high;
  audio.getTone(&m_gain_low, &m_gain_band, &m_gain_high);
  return m_gain_band;
}

int8_t get_lowpass(void)
{
  int8_t m_gain_low;
  int8_t m_gain_band;
  int8_t m_gain_high;
  audio.getTone(&m_gain_low, &m_gain_band, &m_gain_high);
  return m_gain_low;
}


void set_tone(int8_t low,int8_t band, int8_t high){
  int8_t m_gain_low=low;
  int8_t m_gain_band=band;
  int8_t m_gain_high=high;
  audio.setTone(m_gain_low, m_gain_band, m_gain_high);
  eeprom_write_gain_low(m_gain_low);
  eeprom_write_gain_band(m_gain_band);
  eeprom_write_gain_high(m_gain_high);
}


uint8_t get_volumen(void){
  return audio.getVolume();
}
void set_volumen(uint8_t p_vol)
{
  eeprom_write_volumen(p_vol%22);
  audio.setVolume(p_vol%22);
  
}

bool webradio_is_active(){
  
  if (audio.getVolume()==0) return false;
  if (station_info.s_ready==true) return true;
  return false;
}

void webradio_handel(void *pameterar) {
  uint8_t channel;    //Read last Channel-Number without pre-increment
  uint8_t connection_retry;
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

  

  
  // Set the volume
  audio.setVolume(eeprom_read_volumen());
  read_url(channel,url);

  // Connect to an Internet radio station (select one as desired)

  connection_retry=0;
  do{
    connection_retry++;
    channel = read_station((connection_retry%WEBRADIO_MAX_CONNECTION_RETRY)==0);
    if ((connection_retry%WEBRADIO_MAX_CONNECTION_RETRY_REINIT_EEPROM)==0)
    {
      eeprom_init();
      channel=0;
      
    }
    if ((connection_retry%WEBRADIO_MAX_CONNECTION_RETRY_REINIT_EEPROM)==0) { 
      if (Ping.ping("www.google.com", 3)){                                                  //Check if a Internet Connection is established
        eeprom_init();
        channel=0;
        read_url(channel,url); 
      }
    }
  }while(!audio.connecttohost(url));

  
  bool vol_changed=false;
  
  uint8_t trigger_next_station=0;
  static unsigned long lastRefreshTime = millis();
  uint8_t old_vol;
  
  Serial.print("Start Webradio Task on CPU ");
  Serial.println(xPortGetCoreID());
  Serial.flush();
  run_task=true;
  while(run_task){
    // Run audio player
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
      trigger_next_station=1;
      old_vol = audio.getVolume();
      channel = read_station(true);
      read_url(channel,url);
    }

    if (trigger_next_station!=0)
    {
      if((millis() - lastRefreshTime >= TONE_DIMMING)){
          switch ( trigger_next_station )
          {
            case 1:
              if (audio.getVolume()==0) {
                trigger_next_station=2;
              }else{
                audio.setVolume(audio.getVolume()-1);
                lastRefreshTime = millis();
                connection_retry=0;
              }break;
             case 2:
              if (audio.connecttohost(url)){                                 //Try to connect to URL
                trigger_next_station=3;                                         //Go futher if success 
              }else{                                                         //Else 
                connection_retry++;                                             //Increase the retry-counter
                lastRefreshTime = millis();                                     //Add a delay
                if (connection_retry%WEBRADIO_MAX_CONNECTION_RETRY==0){           //Check if maximum count of retries is reached
                  channel = read_station(true);                                     //increment the channel
                  read_url(channel,url);                                            //Read URL
                }
                if ((connection_retry%WEBRADIO_MAX_CONNECTION_RETRY_REINIT_EEPROM)==0) {      //This should never happen! If no connection is possible.
                  if (Ping.ping("www.google.com", 3)){                                                  //Check if a Internet Connection is established
                    eeprom_init();
                    channel=0;
                    read_url(channel,url); 
                  }
                 } 
              }
              
              break;
             case 3:
              if (station_info.s_ready==true) trigger_next_station=4;  
              break;
             case 4:
              if (audio.getVolume()==old_vol) {
                trigger_next_station=0;   //final comand
              }else{
                audio.setVolume(audio.getVolume()+1);
                lastRefreshTime = millis();
              }break;
              default:
                break;
          }
      }
    }

    
  
    if(vol_changed && (millis() - lastRefreshTime >= VOL_REFRESH_INTERVAL))
    {
      lastRefreshTime = millis();
      vol_changed=false;
      if ((rotary_getVal()%22)==0){
        if (DEBUG&8>0) Serial.println("Webradio: 10s muted, now go to Sleep Mode");
          esp_sleep_enable_ext1_wakeup(RTC_MFP_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
          esp_sleep_enable_ext0_wakeup(ROTARY_WAKUP_PIN, !digitalRead(ROTARY_WAKUP_PIN));

        #ifdef BAT_ACTIVE
          eeprom_write_time(time_getEpochTime());//ToDo
          eeprom_write_bat(bat_get_level());
        #endif
          digitalWrite(MAX98357A_SD, LOW);
          pinMode(MAX98357A_GAIN, INPUT);
          delay(1000);
          esp_deep_sleep_start();
      }
      eeprom_write_volumen(rotary_getVal()%22);
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



struct STATION_INFO *audio_get_station(void){return &station_info;};

void audio_info(const char *info) {
  if (DEBUG&2>0) Serial.print("info        "); Serial.println(info);
  if (strcmp(info,"stream ready")==0) station_info.s_ready=true;
}
void audio_id3data(const char *info) { //id3 metadata
  if (DEBUG&2>0) Serial.print("id3data     "); Serial.println(info);
}
void audio_eof_mp3(const char *info) { //end of file
  if (DEBUG&2>0) Serial.print("eof_mp3     "); Serial.println(info);
}
void audio_showstation(const char *info) {
  if (DEBUG&2>0) Serial.print("station     "); Serial.println(info);
  strncpy_P(station_info.station, info, sizeof(station_info.station)-1);
  station_info.station[sizeof(station_info.station)-1] = 0;
}
void audio_showstreaminfo(const char *info) {
  if (DEBUG&2>0) Serial.print("streaminfo  "); Serial.println(info);
  strncpy_P(station_info.streaminfo, info, sizeof(station_info.streaminfo)-1);
  station_info.streaminfo[sizeof(station_info.streaminfo)-1] = 0;
}
void audio_showstreamtitle(const char *info) {
  if (DEBUG&2>0) Serial.print("streamtitle "); Serial.println(info);
  strncpy_P(station_info.streamtitle, info, sizeof(station_info.streamtitle)-1);
  station_info.streamtitle[sizeof(station_info.streamtitle)-1] = 0;
}
void audio_bitrate(const char *info) {
  if (DEBUG&2>0) Serial.print("bitrate     "); Serial.println(info);
  strncpy_P(station_info.bitrate, info, sizeof(station_info.bitrate)-1);
  station_info.bitrate[sizeof(station_info.bitrate)-1] = 0;
}
void audio_commercial(const char *info) { //duration in sec
  if (DEBUG&2>0) Serial.print("commercial  "); Serial.println(info);
  strncpy_P(station_info.commercial, info, sizeof(station_info.commercial)-1);
  station_info.commercial[sizeof(station_info.commercial)-1] = 0;
}
void audio_icyurl(const char *info) { //homepage
  if (DEBUG&2>0) Serial.print("icyurl      "); Serial.println(info);
  strncpy_P(station_info.icyurl, info, sizeof(station_info.icyurl)-1);
  station_info.icyurl[sizeof(station_info.icyurl)-1] = 0;
}
void audio_lasthost(const char *info) { //stream URL played
  if (DEBUG&2>0) Serial.print("lasthost    "); Serial.println(info);
  strncpy_P(station_info.lasthost, info, sizeof(station_info.lasthost)-1);
  station_info.lasthost[sizeof(station_info.lasthost)-1] = 0;
}
void audio_eof_speech(const char *info) {
  if (DEBUG&2>0) Serial.print("eof_speech  "); Serial.println(info);
}
