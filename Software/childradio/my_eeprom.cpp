#include <Arduino.h>
#include "EEPROM.h"
#include "my_common.h"
#include "my_eeprom.h"

extern int DEBUG;


int write_url(uint16_t count, char *destination){
  if (count<0 or count > URL_COUNT-1) return -1;  
  if (DEBUG&4>0) Serial.print("Write Channel ");
  if (DEBUG&4>0) Serial.print(count);
  if (DEBUG&4>0) Serial.print(": ");
  if (DEBUG&4>0) Serial.print(destination);
  //writing byte-by-byte to EEPROM
  for (uint16_t i = 0; i < URL_LENGTH-1; i++) {
    EEPROM.write(i+URL_LENGTH*count, destination[i]);
    EEPROM.commit();
  }
  if (DEBUG&4>0) Serial.println("     done");
  return 0;
}


int eeprom_write_array(uint16_t adr, char *file, uint8_t file_size){
  for (uint8_t i = 0; i < file_size; i++) {
    EEPROM.write(i+adr, file[i]);
    EEPROM.commit();
  } 
  return -1;
}

int eeprom_read_array(uint16_t adr, char *file, uint8_t file_size){
  for (uint16_t i = 0; i < file_size; i++) {
    file[i] = EEPROM.read(i+adr);
  }
  return -1;
}

unsigned long eeprom_read_time() {
  unsigned long file;
  eeprom_read_array(   LAST_TIME_ADR,  (char*)&file,   sizeof(unsigned long));
  // if (DEBUG&4>0) Serial.print("EEPROM READ LAST OPERATION TIME: ");
  // if (DEBUG&4>0) Serial.println(file);
  return file;
}

void eeprom_write_time(unsigned long val){
  unsigned long file = val;
  eeprom_write_array(  LAST_TIME_ADR,  (char *) &file,  sizeof(unsigned long));
}

/*  
 *  Read the Current Station and Increase the Value.
 *  Maximul Value is URL_COUNT
 *  PARAMETER
 *    increment:  if true, the station will be increased by 1
 * 
 */
uint8_t read_station(bool increment){
  // if (DEBUG&4>0) Serial.print("EEPROM Read Station Counter: ");
  uint8_t station_counter=eeprom_read_station_counter();
  if (station_counter<0 or station_counter > URL_COUNT-1){
    station_counter=0;
    eeprom_init();
  }
  // if (DEBUG&4>0) Serial.println(station_counter);
  // if (DEBUG&4>0) Serial.print("Station Active: ");
  // if (DEBUG&4>0) Serial.println(eeprom_read_station_active());
  // if (increment)
  {
    do
    {
      station_counter=((station_counter+1)%URL_COUNT);
      //Serial.println(station_counter);
    }while(((eeprom_read_station_active()>>station_counter)&1)==0);
    
    
    // if (DEBUG&4>0) Serial.print("Write Station Counter ");
    // if (DEBUG&4>0) Serial.println(station_counter);
    eeprom_write_station_counter(station_counter);  
  }
  return station_counter;
}



void setup_eeprom()
{
    if (!EEPROM.begin(EEPROM_SIZE)) {
      if (DEBUG&4>0) Serial.println("failed to init EEPROM");
      delay(1000000);
    }
    if( (eeprom_read_station_counter()<0      or eeprom_read_station_counter()>URL_COUNT-1) or \
        (eeprom_read_volumen()  <VOL_MIN      or eeprom_read_volumen()    >VOL_MAX-1) or \
        (eeprom_read_balance()  <BALANCE_MIN  or eeprom_read_balance()    >BALANCE_MAX-1) or \
        (eeprom_read_gain_low() <TONE_MIN     or eeprom_read_gain_low()   >TONE_MAX-1) or \
        (eeprom_read_gain_band() <TONE_MIN    or eeprom_read_gain_band()   >TONE_MAX-1) or \
        (eeprom_read_gain_high() <TONE_MIN    or eeprom_read_gain_high()   >TONE_MAX-1) or \
        (eeprom_read_station_active()==0) ){
          if (DEBUG&4>0) Serial.println("EEPROM:  Check for consitensy failed!");
          eeprom_init();
        }else{
          if (DEBUG&4>0) Serial.println("EEPROM:  Check for consitensy success!");
        }
        
    
}


void eeprom_init(){
    eeprom_clear();
    char default_URL[URL_LENGTH];
    char default_HOST[HOSTNAME_LENGTH];
    if (DEBUG&4>0) Serial.println("EEPROM:  Write Init-Data");
    if (DEBUG&4>0) Serial.println("EEPROM:  Write Init-Data URL0");
    memcpy(default_URL,URL_DEFAULT0,sizeof(URL_DEFAULT0));
    write_url(0,default_URL);
    if (DEBUG&4>0) Serial.println("EEPROM:  Write Init-Data URL1");
    memcpy(default_URL,URL_DEFAULT1,sizeof(URL_DEFAULT1));
    write_url(1,default_URL);
    if (DEBUG&4>0) Serial.println("EEPROM:  Write Init-Data URL2");
    memcpy(default_URL,URL_DEFAULT2,sizeof(URL_DEFAULT2));
    if (DEBUG&4>0) Serial.println("EEPROM:  Write Init-Data Vol and Tone");
    write_url(2,default_URL);
    eeprom_write_station_counter(0);
    eeprom_write_station_active(7);   //activat Station 1-3
    eeprom_write_volumen(4);
    eeprom_write_balance(0);
    eeprom_write_gain_low(0);
    eeprom_write_gain_band(0);
    eeprom_write_gain_high(0);
    eeprom_write_time(0);//ToDo
    eeprom_write_bat(100);
    if (DEBUG&4>0) Serial.println("EEPROM:  Write Init-Data Alarm");
    eeprom_write_alarm_status(0);
    eeprom_write_alarm_volumen(10);
    eeprom_write_high_gain(0);
    eeprom_write_rgb_brightness(64);
    eeprom_write_rgb_time(100);
    eeprom_write_rgb_palette(1);
    if (DEBUG&4>0) Serial.println("EEPROM:  Write Init-Data Hostname");
    memcpy(default_HOST,HOSTNAME_default,sizeof(HOSTNAME_default));
    write_hostname(default_HOST);
    EEPROM.commit();    
}

int read_hostname(char *destination)
{
  for (uint16_t i = 0; i < HOSTNAME_LENGTH; i++) {
    destination[i] = EEPROM.read(i+HOSTNAME_ADR);
    // iif (DEBUG&4>0) Serial.print(destination[i]);
  }
  // iif (DEBUG&4>0) Serial.println();
  return 0;
}

int write_hostname(char *destination)
{
  if (DEBUG&4>0) Serial.print("EEPROM:  Write Hostname   ");
  for (uint16_t i = 0; i < HOSTNAME_LENGTH-1; i++) {
    if (DEBUG&4>0) Serial.print(destination[i]);
    EEPROM.write(i+HOSTNAME_ADR, destination[i]);
    EEPROM.commit();
  }
  if (DEBUG&4>0) Serial.println("");
  return 0;
}


int read_url(uint16_t count, char *destination){
  if (count<0 or count > URL_COUNT-1) return -1; 
  //if (DEBUG&4>0) Serial.print("Read Channel ");
  //if (DEBUG&4>0) Serial.print(count);
  //if (DEBUG&4>0) Serial.print(": ");
     
  for (uint16_t i = 0; i < URL_LENGTH-1; i++) {
    destination[i] = EEPROM.read(i+URL_LENGTH*count);
  //  if (DEBUG&4>0) Serial.print(destination[i]);
  }
  // iif (DEBUG&4>0) Serial.println();
  return 0;
}

void eeprom_clear(void)
{
  if (DEBUG&4>0) Serial.println("EEPROM: Clear!!!");
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, -1);
  }
  EEPROM.commit();
  delay(500);
}
/*
 * Read a single Byte from Adress.
 * Parameter:
 *    label:  const char*   A Massage for Writing a massage on Srial
 *    adr:    unit8_t       Address
 *    return: int8_t        value
 */
int8_t eeprom_read_byte(uint16_t addr, const char* label){
  int8_t read_data=(int8_t)EEPROM.read(addr); 
  //  if (count<0 or count > URL_COUNT-1) return -1; 
  //  if (DEBUG&4>0) Serial.print("EEPROM Read ");
  //  if (DEBUG&4>0) Serial.print(label);  
  //  if (DEBUG&4>0) Serial.print(": ");   
  //  if (DEBUG&4>0) Serial.println(read_data);  
  return read_data;
}

int8_t eeprom_write_byte(uint16_t addr,int8_t write_data, const char* label){
  //  if (DEBUG&4>0) Serial.print("EEPROM ADR ");
  //  if (DEBUG&4>0) Serial.print(addr);
  //  if (DEBUG&4>0) Serial.print(" ");
  //  if (DEBUG&4>0) Serial.print(label);  
  //  if (DEBUG&4>0) Serial.print(": "); 
  //  if (DEBUG&4>0) Serial.println(write_data);  
  EEPROM.write(addr, write_data);
  EEPROM.commit(); 
  return 0;
}
