#include <Arduino.h>
#include <time.h>
#include "my_time.h"
#include "my_eeprom.h"
#include "my_webradio2.h"
#include "my_common.h"
#include <MCP7940.h>
#include <ESP32Ping.h>
#include <SpiffsFilePrint.h>  // https://github.com/PRosenb/SPIFFS_FilePrint
extern SpiffsFilePrint filePrint;
//#define Serial filePrint
//#include "I2CScanner.h"
//I2CScanner scanner;

#define SDA_PIN RTC_SDA	
#define SCL_PIN RTC_SCL

extern int DEBUG;

/***************************************************************************************************
** Declare all program constants and enumerated types                                             **
***************************************************************************************************/
const uint32_t SERIAL_SPEED{115200};     ///< Set the baud rate for Serial I/O
//const uint8_t  LED_PIN{13};              ///< Arduino built-in LED pin number
const uint8_t  SPRINTF_BUFFER_SIZE{32};  ///< Buffer size for sprintf()
const uint8_t  ALARM1_INTERVAL{15};      ///< Interval seconds for alarm
/*! ///< Enumeration of MCP7940 alarm types */
enum alarmTypes {
  matchSeconds,
  matchMinutes,
  matchHours,
  matchDayOfWeek,
  matchDayOfMonth,
  Unused1,
  Unused2,
  matchAll,
  Unknown
};

/***************************************************************************************************
** Declare global variables and instantiate classes                                               **
***************************************************************************************************/
MCP7940_Class MCP7940;                           ///< Create instance of the MCP7940M
char          inputBuffer[SPRINTF_BUFFER_SIZE];  ///< Buffer for sprintf() / sscanf()



unsigned long time_startup;
unsigned long time_lastsleepTime;

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Time: Failed to obtain time 1");
    return;
  }
  if (DEBUG&32>0) Serial.println(&timeinfo, "Time: %A, %B %d %Y %H:%M:%S zone %Z %z ");
}

static volatile bool g_shutdownRequested = false;

void TIME_shutdown(void){
  g_shutdownRequested=true;
}

void time_handle(void *parameter){ 

  /*
   * Connect to NTP-Server and capture corrent time
   */
  while(!Ping.ping("www.google.com", 3)){ vTaskDelay(500);}
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  if (DEBUG&32>0) Serial.print("Time: Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    vTaskDelay(1000);
    if (DEBUG&32>0) Serial.print(".");
    now = time(nullptr);
  }
  if (DEBUG&32>0) Serial.println("");
  printLocalTime();
  
  /*
   * Set local Timezone
  */
  setenv("TZ","CEST-1CET,M3.2.0/2:00:00,M11.1.0/2:00:00", 1);
  tzset();
  printLocalTime();
  
  /*
   * Start up epoch Time UCT
   */
  time_startup=time_getEpochTime();
  time_lastsleepTime=eeprom_read_time();//eeprom_read_time();
  
  if (DEBUG&32>0) Serial.print("Time: Read Time last Operation:  ");
  if (DEBUG&32>0) Serial.println(eeprom_read_time());

  //scanner.Init();


#define MCP7940_ENABLE
#ifdef MCP7940_ENABLE

  while (!MCP7940.begin(RTC_SDA,RTC_SCL,RTC_I2C_SPEED))  // Loop until the RTC communications are established  19,23,100000
  {
    if (DEBUG&32>0) {
      Serial.println(F("Time: Unable to find MCP7940. Checking again ..."));
      //scanner.Scan();
      vTaskDelay(10000);
    }

  }  // of loop until device is located
  if (DEBUG&32>0) Serial.println(F("Time: MCP7940 initialized."));
  while (!MCP7940.deviceStatus())  // Turn oscillator on if necessary
  {
    if (DEBUG&32>0) Serial.println(F("Time: Oscillator is off, turning it on."));
    bool deviceStatus = MCP7940.deviceStart();  // Start oscillator and return state
    if (!deviceStatus)                          // If it didn't start
    {
      if (DEBUG&32>0) Serial.println(F("Time: Oscillator did not start, trying again."));
      vTaskDelay(1000);
    }  // of if-then oscillator didn't start
  }    // of while the oscillator is of

  //Now Setting the tinme
  if (DEBUG&32>0) Serial.println("Time: Setting MCP7940M to utc time");
  //MCP7940.adjust();  // Use compile date/time to set clock
  DateTime now__utc_ntp = DateTime(time_getEpochTime());
  MCP7940.calibrateOrAdjust(now__utc_ntp);
  if (DEBUG&32>0) Serial.print("Time: RTC Date/Time set to ");
  DateTime now_rtc = MCP7940.now();  // get the current time
  // Use sprintf() to pretty print date/time with leading zeroes
  sprintf(inputBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now_rtc.year(), now_rtc.month(), now_rtc.day(),
          now_rtc.hour(), now_rtc.minute(), now_rtc.second());
  if (DEBUG&32>0) Serial.println(inputBuffer);


  //Check the Alarm
  uint8_t alarmtype;
  DateTime alarm_utc = MCP7940.getAlarm(0, alarmtype); //This is utc time
  snprintf(inputBuffer, 17, "%04d-%02d-%02d %02d:%02d", alarm_utc.year(), alarm_utc.month(), alarm_utc.day(),alarm_utc.hour(), alarm_utc.minute());
  if (DEBUG&32>0) Serial.print("Time: Read ALARM0 Value (UTC): ");
  if (DEBUG&32>0) Serial.println(inputBuffer);
  alarm_process();
#endif
  
  while(1){
    printLocalTime();
    vTaskDelay(60*1000);
    if (g_shutdownRequested) {
      printf("[TaskTIME] Shutdown requested, cleaning up...\n");
      vTaskDelete(NULL);   // <--- Task beendet sich selbst
    }
  }
}

unsigned long time_getEpochTime(){time_t now; time(&now);return now;}
unsigned long time_getstartupTime(){return time_startup;}
unsigned long time_getlastsleepTime(){return time_lastsleepTime;}


/*
 * Return a String of the Current Time. Formated to use in HTML DateTime Form
 * Parameter:   0 for now
 */
void time_getlocal_htmlDateTime(char *buffer,uint8_t selection) 
{
  struct tm timeinfo;;
  if(!getLocalTime(&timeinfo)){
      //if (DEBUG&32>0) Serial.println("Time: Failed to obtain time 1");
      //if (DEBUG&32>0) sprintf(buffer, "2022-12-24T20:00M");
      return;
  }
      
  if (selection==TIME_NOW){
      //Serial.print("Read RTC Value (UTC): ");
      snprintf(buffer, 17, "%04d-%02d-%02dT%02d:%02d", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);
      //Serial.println(buffer);
      return;
  }else if (selection==TIME_ALARM0){
    uint8_t x;
    struct tm * alarminfo;
    time_t     alarmtime;
    DateTime alarm_utc_woy = MCP7940.getAlarm(0, x); //This is utc time
    DateTime alarm_utc     = DateTime(timeinfo.tm_year+1900, alarm_utc_woy.month(), alarm_utc_woy.day(),alarm_utc_woy.hour(), alarm_utc_woy.minute(),0);
    alarmtime=(time_t)alarm_utc.unixtime();
    alarminfo=localtime(&alarmtime);

    snprintf(buffer, 17, "%04d-%02d-%02dT%02d:%02d", alarminfo->tm_year+1900, alarminfo->tm_mon+1, alarminfo->tm_mday, alarminfo->tm_hour, alarminfo->tm_min);;
    //if (DEBUG&32>0) Serial.print("Time: Read ALARM0 Value (UTC): ");
    //if (DEBUG&32>0) Serial.println(buffer);
    return;
  }
  return;
}

void time_clearAlarm(void){
    #ifdef MCP7940_ENABLE
    uint8_t x;
    DateTime alarm_utc = MCP7940.getAlarm(0, x); //This is utc time
    MCP7940.setAlarm(0, matchAll,alarm_utc,false);
  #endif
}

/*
 * Function to set the Alarm.
 * DateTimeChar: Char with the Format:    YYYY-MM-DDTHH-MM
 */
void time_setAlarm(char *DateTimeChar)
{
  struct tm timeinfo;
  uint16_t  tmp_int;

  //Year
  tmp_int =(DateTimeChar[0]-48)*1000;
  tmp_int+=(DateTimeChar[1]-48)*100;
  tmp_int+=(DateTimeChar[2]-48)*10;
  tmp_int+=(DateTimeChar[3]-48)*1;
  timeinfo.tm_year=tmp_int-1900;

  //Month
  tmp_int =(DateTimeChar[5]-48)*10;
  tmp_int+=(DateTimeChar[6]-48)*1;
  timeinfo.tm_mon=tmp_int-1;

  //Day
  tmp_int =(DateTimeChar[8]-48)*10;
  tmp_int+=(DateTimeChar[9]-48)*1;
  timeinfo.tm_mday=tmp_int;

  //Hour
  tmp_int =(DateTimeChar[11]-48)*10;
  tmp_int+=(DateTimeChar[12]-48)*1;
  timeinfo.tm_hour=tmp_int;

  //Minute
  tmp_int =(DateTimeChar[14]-48)*10;
  tmp_int+=(DateTimeChar[15]-48)*1;
  timeinfo.tm_min=tmp_int;

  //Seconds
  timeinfo.tm_sec=0;
  timeinfo.tm_isdst=-1;


  Serial.print("Alarm: Set Alarmtime to local time ");
  char buffer[17];
  snprintf(buffer, 17, "Time: %04u-%02u-%02uT%02u:%02u", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);
  if (DEBUG&32>0) Serial.println(buffer);

  time_t t_utc_alarm;
  t_utc_alarm=mktime(&timeinfo);
  if (DEBUG&32>0) Serial.print("Alarm: Set Alarmtime to utc time ");
  if (DEBUG&32>0) Serial.println(t_utc_alarm);

  #ifdef MCP7940_ENABLE
  DateTime alarm_utc = DateTime(t_utc_alarm);
  MCP7940.setAlarm(0, matchAll,alarm_utc,true);
  #endif
}



/*
 * Check if the Alarm is triggerd
 * If it triggered, it calculate the next Alarm and set the ALARM to the rtc
 */

 void alarm_process(void)
 {
     if (MCP7940.isAlarm(0))  // When alarm 0 is triggered
    {
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      uint8_t weekday=timeinfo.tm_wday;
      uint8_t alarmstatus=eeprom_read_alarm_status()>>7;
      uint8_t weekday_nextAlarm = (eeprom_read_alarm_status()&0x7F);
      
      Serial.println("ALARM: Alarm0 is triggering");
      MCP7940.clearAlarm(0);
      set_volumen(eeprom_read_alarm_volumen());

      if (alarmstatus>0){   //proceed only, if ALRAM is enabled
        if (weekday_nextAlarm==0){
          eeprom_write_alarm_status(0); 
          Serial.println("ALARM: No further alarm is set");  
        }else{
          //Sort Weekday starting with today weekday
          Serial.print("ALARM: weekday_is:");
          Serial.println(weekday);
          Serial.print("ALARM: weekday_nextAlarm start Monday: ");
          Serial.println(weekday_nextAlarm);
          weekday_nextAlarm=((weekday_nextAlarm >> (weekday))|(weekday_nextAlarm << (7 - (weekday))));
          Serial.print("ALARM: weekday_nextAlarm start tomorrow: ");
          Serial.println(weekday_nextAlarm);
          
          uint8_t counter=1;
          while((weekday_nextAlarm&1)==0){
            weekday_nextAlarm=weekday_nextAlarm>>1;
            counter++;
          }
          //Now, counter represent the day, when the next alarm have to perform.
          Serial.print("ALARM: Perform the next Alarm in days: ");
          Serial.println(counter);
        uint8_t x;
         DateTime alarm_now = MCP7940.getAlarm(0, x);
         MCP7940.setAlarm(0, matchAll,alarm_now + TimeSpan(counter,0, 00, 0),true);
         
        }
      }else{
        Serial.println("ALARM: Alarm0 is disabled");  
      }
    }else{
          Serial.println("ALARM: Alarm0 is not triggering");                   
    }
 }
