#include <Arduino.h>
#include "my_wifi.h"
#include "my_webserver.h"
#include "my_webradio2.h"
#include "my_encoder.h"
#include "my_eeprom.h"
#include "my_common.h"
#include "my_time.h"
#include "my_bat.h"
#include "my_ledstripe.h"

// Global variables
uint8_t DEBUG = 1|2|4|8|16|32;
//&&32:  Bit 6   Time
//&&16:  Bit 5   Rotary Encoder
//&&8:  Bit 4   Rotary Encoder
//&&4:  Bit 3   EEPROM
//&&2:  Bit 2   Webradio
//&&1:  Bit 1   Webserver
        


TaskHandle_t Core0Task;
TaskHandle_t Core1Task1;
TaskHandle_t Core1Task2;
TaskHandle_t Core1Task3;
TaskHandle_t Core1Task4;
TaskHandle_t Core1Task5;
TaskHandle_t Core1Task6;


void handle_LED(void *parameter)
{
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, HIGH);
  while(1){
    while(webradio_is_active()==false)
    {
      digitalWrite(LED_GPIO, LOW);
      vTaskDelay(LED_TIME/2);
      digitalWrite(LED_GPIO, HIGH);
      vTaskDelay(LED_TIME/2);
    }
    digitalWrite(LED_GPIO, HIGH); 
    vTaskDelay(LED_TIME);
  }
}


void handle_SystemMonitor(void *parameter)
{
  static unsigned long lastRefreshTime = millis();
  while(1){
    if (webradio_is_active()==true) lastRefreshTime = millis();
    if((millis() - lastRefreshTime >= RESTART_TIME_AFTER_NO_SOUND_OUTPUT)){
        Serial.println("Webradio: 120s no connected Radio, now go to Sleep Mode");
        eeprom_write_time(time_getEpochTime());//ToDo
        eeprom_write_bat(bat_get_level());
        esp_sleep_enable_ext1_wakeup(RTC_MFP_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
        esp_sleep_enable_ext0_wakeup(ROTARY_WAKUP_PIN, !digitalRead(ROTARY_WAKUP_PIN));
        digitalWrite(MAX98357A_SD, LOW);
        pinMode(MAX98357A_GAIN, INPUT);
        delay(1000);
        esp_deep_sleep_start();
    }
    vTaskDelay(1000);
  }
}


//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup() {
  BaseType_t success;
  Serial.begin ( 115200 ) ;                            // For debug
  Serial.println() ;

  Serial.print(F("\nStarting SetAlarms program\n"));
  Serial.print(F("- Compiled with c++ version "));
  Serial.print(F(__VERSION__));  // Show compiler information
  Serial.print(F("\n- On "));
  Serial.print(F(__DATE__));
  Serial.print(F(" at "));
  Serial.print(F(__TIME__));
  Serial.print(F("\n"));

  Serial.print("The Debug level is ");
  Serial.println(DEBUG) ;

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  bool alarm_enable=false;
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : 
        Serial.println("Wakeup caused by Rotary Controll"); break;
    case ESP_SLEEP_WAKEUP_EXT1 :
        Serial.println("Wakeup caused by ALARM-RTC"); 
        alarm_enable=true;
        break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer: This should never be happend"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad: This should never be happend"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program: This should never be happend"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }

  success=xTaskCreatePinnedToCore(
                    handle_SystemMonitor,                /* Task function. */
                    "SystemMonitorhandle",            /* name of task. */
                    10000,                      /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Core1Task4, /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */  
  if (!success) {
      Serial.println("Failed to create LED task. Aborting.\n");
      Serial.flush();
  }
  
  success=xTaskCreatePinnedToCore(
                    handle_LED,                /* Task function. */
                    "LEDhandle",            /* name of task. */
                    10000,                      /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Core1Task3, /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */  
  if (!success) {
      Serial.println("Failed to create LED task. Aborting.\n");
      Serial.flush();
  }


  success=xTaskCreatePinnedToCore(
                    handle_RGB_LED,                /* Task function. */
                    "RGBStripe",            /* name of task. */
                    10000,                      /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Core1Task5, /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */  
  if (!success) {
      Serial.println("Failed to create RGB LED task. Aborting.\n");
      Serial.flush();
  }


  

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);
  setup_eeprom();
  //eeprom_init();
  rotary_setup(eeprom_read_volumen());
  setup_WiFi();

  setup_time();
  alarm_process();



    /*
   * Setup BatteryHandle
   */
  success=xTaskCreatePinnedToCore(
                    BATTERY_handle,             /* Task function. */
                    "Battery_SERVICE",          /* name of task. */
                    10000,                        /* Stack size of task */
                    NULL,                       /* parameter of the task */
                    4,                          /* priority of the task */
                    &Core1Task6,                /* Task handle to keep track of created task */
                    1);                         /* pin task to core 1 */
  if (!success) {
      Serial.println("Failed to create handlewebradio task. Aborting.\n");
      Serial.flush();
  }

  

  
    /*
   * Setup Webradio
   */
  success=xTaskCreatePinnedToCore(
                    webradio_handel,                /* Task function. */
                    "WEBRADIO_SERVICE",          /* name of task. */
                    10000,                      /* Stack size of task */
                    NULL,        /* parameter of the task */
                    4,           /* priority of the task */
                    &Core0Task,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */
  if (!success) {
      Serial.println("Failed to create handlewebradio task. Aborting.\n");
      Serial.flush();
  }


  
  
  /*
   * Setup Webserver
   */
  setup_webserver();
  success=xTaskCreatePinnedToCore(
                    loop_webserver,                /* Task function. */
                    "Webserver",          /* name of task. */
                    10000,                      /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Core1Task1,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */  
  if (!success) {
      Serial.println("Failed to create handlewebserver task. Aborting.\n");
      Serial.flush();
  }




}

void loop() {
  vTaskDelay(1000);
}
