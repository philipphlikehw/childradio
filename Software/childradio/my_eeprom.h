#include "my_common.h"
#include <stdint.h>

int write_hostname(char *destination);
int read_hostname(char *destination);
int write_url(uint16_t count, char *destination);
int read_url(uint16_t count, char *destination);
uint8_t read_station(bool increment=false);
void setup_eeprom(void);
void eeprom_clear(void);
void eeprom_init(void);

int8_t eeprom_read_byte(uint16_t addr,const char* label);
int8_t eeprom_write_byte(uint16_t addr,int8_t write_data,const char* label);

int eeprom_write_array(uint16_t adr, char *file, uint8_t file_size);
int eeprom_read_array(uint16_t adr, char *file, uint8_t file_size);

#define COUNTER_ADR         URL_LENGTH*URL_COUNT
#define VOLUME_ADR          URL_LENGTH*URL_COUNT+1
#define BALANCE_ADR         URL_LENGTH*URL_COUNT+2
#define GAIN_LOW_PASS_ADR   URL_LENGTH*URL_COUNT+3
#define GAIN_BAND_PASS_ADR  URL_LENGTH*URL_COUNT+4
#define GAIN_HIGH_PASS_ADR  URL_LENGTH*URL_COUNT+5
#define STATION_ACTIVE_ADR  URL_LENGTH*URL_COUNT+6
#define LAST_TIME_ADR       URL_LENGTH*URL_COUNT+7            //Data-type is unsigned Long
#define LAST_BAT_ADR        LAST_TIME_ADR+sizeof(unsigned long)   //Data-Type is uint8_t
#define ALARM_STATUS_ADR    LAST_BAT_ADR+1
#define ALARM_VOLUMEN_ADR   LAST_BAT_ADR+2
#define HIGH_GAIN_ADR       LAST_BAT_ADR+3
#define RGB_BRIGHTNESS_ADR  LAST_BAT_ADR+4
#define RGB_TIME_ADR        LAST_BAT_ADR+5
#define RGB_PALETTE_ADR     LAST_BAT_ADR+6
#define HOSTNAME_ADR        RGB_PALETTE_ADR+1
#define EEPROM_SIZE         HOSTNAME_ADR+HOSTNAME_LENGTH

#define eeprom_read_station_counter()       eeprom_read_byte(   COUNTER_ADR,            "STATION COUNTER")
#define eeprom_write_station_counter(val)   eeprom_write_byte(  COUNTER_ADR,  val,      "STATION COUNTER")
#define eeprom_read_volumen()               eeprom_read_byte(   VOLUME_ADR,             "VOLUMEN")
#define eeprom_write_volumen(vol)           eeprom_write_byte(  VOLUME_ADR,   vol,      "VOLUMEN")
#define eeprom_read_balance()               eeprom_read_byte(   BALANCE_ADR,            "BALANCE")
#define eeprom_write_balance(bal)           eeprom_write_byte(  BALANCE_ADR,  bal,      "BALANCE")
#define eeprom_read_gain_low()              eeprom_read_byte(   GAIN_LOW_PASS_ADR,      "GAIN LOW PASS")
#define eeprom_write_gain_low(ton)          eeprom_write_byte(  GAIN_LOW_PASS_ADR,  ton,"GAIN LOW PASS")
#define eeprom_read_gain_band()             eeprom_read_byte(   GAIN_BAND_PASS_ADR,     "GAIN BAND PASS")
#define eeprom_write_gain_band(ton)         eeprom_write_byte(  GAIN_BAND_PASS_ADR,ton, "GAIN BAND PASS")
#define eeprom_read_gain_high()             eeprom_read_byte(   GAIN_HIGH_PASS_ADR,     "GAIN HIGH PASS")
#define eeprom_write_gain_high(ton)         eeprom_write_byte(  GAIN_HIGH_PASS_ADR,ton, "GAIN HIGH PASS")
#define eeprom_read_station_active()        eeprom_read_byte(   STATION_ACTIVE_ADR,     "STATION ACTIVE")   //Station can be active or passiv, if not used
#define eeprom_write_station_active(val)    eeprom_write_byte(  STATION_ACTIVE_ADR,val, "STATION ACTIVE")
#define eeprom_read_alarm_status()          eeprom_read_byte(   ALARM_STATUS_ADR,       "ALARM STATUS")
#define eeprom_write_alarm_status(val)      eeprom_write_byte(  ALARM_STATUS_ADR,val,   "ALARM STATUS")
#define eeprom_read_alarm_volumen()         eeprom_read_byte(   ALARM_VOLUMEN_ADR,      "ALARM VOLUMEN")
#define eeprom_write_alarm_volumen(val)     eeprom_write_byte(  ALARM_VOLUMEN_ADR,val,  "ALARM VOLUMEN")
#define eeprom_read_high_gain()             eeprom_read_byte(   HIGH_GAIN_ADR,          "ALARM VOLUMEN")    //Bit 0 is High gain
#define eeprom_write_high_gain(val)         eeprom_write_byte(  HIGH_GAIN_ADR,val,      "ALARM VOLUMEN")    //Bit 0 is High gain

#define eeprom_read_rgb_brightness()         eeprom_read_byte(   RGB_BRIGHTNESS_ADR,      "RGB BRIGHTNESS")
#define eeprom_write_rgb_brightness(val)     eeprom_write_byte(  RGB_BRIGHTNESS_ADR,val,  "RGB BRIGHTNESS")
#define eeprom_read_rgb_time()               eeprom_read_byte(   RGB_TIME_ADR,        "RGB TIME")
#define eeprom_write_rgb_time(val)           eeprom_write_byte(  RGB_TIME_ADR,val,    "RGB TIME")
#define eeprom_read_rgb_palette()            eeprom_read_byte(   RGB_PALETTE_ADR,      "RGB PALETTE")
#define eeprom_write_rgb_palette(val)        eeprom_write_byte(  RGB_PALETTE_ADR,val, "RGB PALETTE")

#define eeprom_write_bat(val)                eeprom_write_byte(   LAST_BAT_ADR,   val,  "BATTERY LEVEL")
#define eeprom_read_bat()                    eeprom_read_byte(   LAST_BAT_ADR,  "BATTERY LEVEL")
unsigned long eeprom_read_time(void);
void eeprom_write_time(unsigned long);
