#include <Arduino.h>
//#include <ArduinoLog.h>       // https://github.com/thijse/Arduino-Log
#include "my_wifi.h"
#include "my_webserver.h"
#include "my_webradio2.h"
#include "my_encoder.h"
#include "my_eeprom.h"
#include "my_common.h"
#include "my_time.h"
#include "my_bat.h"
#include "my_ledstripe.h"
#include <SpiffsFilePrint.h>  // https://github.com/PRosenb/SPIFFS_FilePrint
SpiffsFilePrint filePrint("/logfile", 1, 64 * 1024, &Serial);

// Global variables
// uint8_t DEBUG = 1 | 2 | 4 | 8 | 16 | 32;
enum : uint8_t {
  DBG_WEBSERVER = 1 << 0,
  DBG_WEBRADIO  = 1 << 1,
  DBG_EEPROM    = 1 << 2,
  DBG_ENCODER_A = 1 << 3,
  DBG_ENCODER_B = 1 << 4,
  DBG_TIME      = 1 << 5,
};
uint8_t DEBUG = DBG_WEBSERVER | DBG_WEBRADIO | DBG_EEPROM | DBG_ENCODER_A | DBG_ENCODER_B | DBG_TIME;

#define DBG(mask, ...) do { if (DEBUG & (mask)) { Serial.printf(__VA_ARGS__); } } while(0)



// --- Handles --------------------------------------------------
TaskHandle_t TaskWebradio;
TaskHandle_t TaskWebserver;
TaskHandle_t TaskLED;
TaskHandle_t TaskMonitor;
TaskHandle_t TaskRGB;
TaskHandle_t TaskBattery;
TaskHandle_t TaskTime;

// --- Prioritäten (1 = niedrig) --------------------------------
enum : UBaseType_t {
  PRI_LED       = 1,
  PRI_MONITOR   = 2,
  PRI_WEBSERVER = 5,
  PRI_TIME      = 3,
  PRI_BATTERY   = 4,
  PRI_WEBRADIO  = 6, // ggf. 6 falls Audio wirklich knapp
};

// --- Stackgrößen ----------------------------------------------
constexpr uint32_t STK_LED       = 2048;
constexpr uint32_t STK_MONITOR   = 4096;
constexpr uint32_t STK_WEBSERVER = 6144;
constexpr uint32_t STK_TIME      = 4096;
constexpr uint32_t STK_BATTERY   = 4096;
constexpr uint32_t STK_WEBRADIO  = 12288; // Start großzügig, später messen & senken

// --- Core-Zuordnung -------------------------------------------
constexpr BaseType_t CORE_APP = 1; // App-Tasks (Core 1)


// ---- Stack-Logging Helpers -----------------------------------------------
static inline int32_t hwm_words(TaskHandle_t h) {
  return h ? (int32_t)uxTaskGetStackHighWaterMark(h) : -1;   // -1 = N/A
}

static inline int32_t hwm_bytes(TaskHandle_t h) {
  int32_t w = hwm_words(h);
  return (w < 0) ? -1 : (int32_t)(w * sizeof(StackType_t));
}

static inline void print_stack(const char* name, TaskHandle_t h) {
  const int32_t w = hwm_words(h);
  if (w < 0) {
    Serial.printf("%s:N/A  ", name);
  } else {
    Serial.printf("%s:%dW(%dB)  ", name, w, hwm_bytes(h));
  }
}
// ---------------------------------------------------------------------------

static volatile  bool g_shutdownRequested_LED = false;

void LED_shutdown(void){
  g_shutdownRequested_LED=true;
}


void handle_LED(void *parameter) {
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, HIGH);
  TickType_t next = xTaskGetTickCount();

  for (;;) {
    const bool active = webradio_is_active();
    if (!active) {
      digitalWrite(LED_GPIO, LOW);
      vTaskDelayUntil(&next, pdMS_TO_TICKS(LED_TIME/2));
      digitalWrite(LED_GPIO, HIGH);
      vTaskDelayUntil(&next, pdMS_TO_TICKS(LED_TIME/2));
    } else {
      digitalWrite(LED_GPIO, HIGH);
      vTaskDelayUntil(&next, pdMS_TO_TICKS(LED_TIME));
    }
    if (g_shutdownRequested_LED) {
      printf("[TaskSystem_LED] Shutdown requested, cleaning up...\n");
      vTaskDelete(NULL);   // <--- Task beendet sich selbst
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static volatile  bool g_shutdownRequested_SystemMonitor = false;

void SystemMonitor_shutdown(void){
  g_shutdownRequested_SystemMonitor=true;
}


void handle_SystemMonitor(void *parameter)
{
  unsigned long last = millis();
  while (1) {
    if (webradio_is_active()) last = millis();
        if (millis() - last >= RESTART_TIME_AFTER_NO_SOUND_OUTPUT) {
      Serial.println("Webradio: 120s no connected Radio, now go to Sleep Mode");
      eeprom_write_time(time_getEpochTime());
      eeprom_write_bat(bat_get_level());
      esp_sleep_enable_ext1_wakeup(RTC_MFP_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
      esp_sleep_enable_ext0_wakeup(ROTARY_WAKUP_PIN, !digitalRead(ROTARY_WAKUP_PIN));
      digitalWrite(MAX98357A_SD, LOW);
      pinMode(MAX98357A_GAIN, INPUT);
      vTaskDelay(pdMS_TO_TICKS(200));
      Serial.flush();
      esp_deep_sleep_start();
    }
    if (bat_get_level() < BAT_min_Operation_level) {
      filePrint.println("Battery: Battery is low, go to sleep mode");
      eeprom_write_time(time_getEpochTime());//ToDo
      eeprom_write_bat(bat_get_level());
      esp_sleep_enable_ext1_wakeup(RTC_MFP_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
      esp_sleep_enable_ext0_wakeup(ROTARY_WAKUP_PIN, !digitalRead(ROTARY_WAKUP_PIN));
      digitalWrite(MAX98357A_SD, LOW);
      pinMode(MAX98357A_GAIN, INPUT);
      filePrint.close();
      vTaskDelay(1000);
      esp_deep_sleep_start();
    }

    static TickType_t lastPrint = 0;
    TickType_t now = xTaskGetTickCount();
    if (now - lastPrint > pdMS_TO_TICKS(60000)) {
      lastPrint = now;
      Serial.printf("[Stacks] LED:%u, MON:%u, WEBRAD:%u, WEBSRV:%u, BAT:%u, Time:%u\n",
        uxTaskGetStackHighWaterMark(TaskLED),
        uxTaskGetStackHighWaterMark(TaskMonitor),
        uxTaskGetStackHighWaterMark(TaskWebradio),
        uxTaskGetStackHighWaterMark(TaskWebserver),
        uxTaskGetStackHighWaterMark(TaskBattery),
        uxTaskGetStackHighWaterMark(TaskTime)
      );
    }
    if (g_shutdownRequested_SystemMonitor) {
      printf("[TaskSystem_Monitor] Shutdown requested, cleaning up...\n");
      vTaskDelete(NULL);   // <--- Task beendet sich selbst
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}




static inline void create_task(TaskFunction_t fn, const char* name,
                               const uint32_t stack, void* arg,
                               UBaseType_t prio, TaskHandle_t* handle,
                               BaseType_t core) {
  BaseType_t ok = xTaskCreatePinnedToCore(fn, name, stack, arg, prio, handle, core);
  if (ok != pdPASS) {
    Serial.printf("ERROR: create_task '%s' failed (prio=%u, stack=%u, core=%d)\n",
                  name, (unsigned)prio, (unsigned)stack, (int)core);
    Serial.flush();
  }
}

//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup() {
  BaseType_t success;
  BaseType_t rotary_wake=false;
  Serial.begin ( 115200 ) ;                            // For debug
  Serial.println() ;
  delay(1500); // so siehst du Bootmeldungen sicher im Monitor

  Serial.printf("esp_reset_reason=%d\n", (int)esp_reset_reason());
  Serial.printf("heap=%u, psram=%u\n",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getFreePsram());
                

if (!SPIFFS.begin(false)) {
  Serial.println("SPIFFS mount failed, formatting...");
  if (!SPIFFS.format()) {
    Serial.println("SPIFFS format failed!");
  }
  // nach dem Formatieren musst du NOCHMAL mounten:
  if (!SPIFFS.begin(false)) {
    Serial.println("SPIFFS mount failed even after format – ABORT");
    while (1) { delay(1000); }
  }
}

Serial.printf("[SPIFFS] total=%u, used=%u\n",
              (unsigned)SPIFFS.totalBytes(),
              (unsigned)SPIFFS.usedBytes());

  filePrint.open();
  
#ifdef IMAGECOVER
  init_cover_image();
#endif  

  //  Log.begin(LOG_LEVEL_NOTICE, &filePrint);
  //  Log.setPrefix(printTimestamp);
  //  Log.setSuffix(printNewline);
  //  Log.notice("Millis since start: %d", millis());

  filePrint.print(F("\nStarting SetAlarms program\n"));
  filePrint.print(F("- Compiled with c++ version "));
  filePrint.print(F(__VERSION__));  // Show compiler information
  filePrint.print(F("\n- On "));
  filePrint.print(F(__DATE__));
  filePrint.print(F(" at "));
  filePrint.print(F(__TIME__));
  filePrint.print(F("\n"));

  filePrint.print("The Debug level is ");
  filePrint.println(DEBUG) ;

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  bool alarm_enable = false;
  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 :
      filePrint.println("Wakeup caused by Rotary Controll");
      rotary_wake=true;
      break;
    case ESP_SLEEP_WAKEUP_EXT1 :
      filePrint.println("Wakeup caused by ALARM-RTC");
      alarm_enable = true;
      break;
    case ESP_SLEEP_WAKEUP_TIMER : filePrint.println("Wakeup caused by timer: This should never be happend"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : filePrint.println("Wakeup caused by touchpad: This should never be happend"); break;
    case ESP_SLEEP_WAKEUP_ULP : filePrint.println("Wakeup caused by ULP program: This should never be happend"); break;
    default : filePrint.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }

  
 // Tasks erzeugen
  create_task(handle_SystemMonitor, "SystemMonitor", STK_MONITOR,  nullptr, PRI_MONITOR,  &TaskMonitor,  CORE_APP);
  create_task(handle_LED,           "LED",           STK_LED,      nullptr, PRI_LED,      &TaskLED,      CORE_APP);
  create_task(handle_RGB_LED,       "RGBStripe",     4096,         nullptr, PRI_LED,      &TaskRGB,      CORE_APP);

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);
  setup_eeprom();
  //  eeprom_init();
  rotary_setup(eeprom_read_volumen());
  if ( rotary_wake ) {
    delay(1000);
    if ( !encoderChanged() ) {
      Serial.println(F("Wakeup not plausible. Turn off"));
      // optional aufräumen:
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); // Wakeup-Quellen wieder setzen
      esp_sleep_enable_ext1_wakeup(RTC_MFP_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
      esp_sleep_enable_ext0_wakeup(ROTARY_WAKUP_PIN,!digitalRead(ROTARY_WAKUP_PIN));
      esp_deep_sleep_start();
    } 
  }
  rotary_setup(eeprom_read_volumen());
  setup_WiFi();


  create_task(time_handle,          "Time",          STK_TIME,     nullptr, PRI_TIME,     &TaskTime,  CORE_APP);
  create_task(BATTERY_handle,       "Battery",       STK_BATTERY,  nullptr, PRI_BATTERY,  &TaskBattery,  CORE_APP);
  create_task(webradio_handel,      "Webradio",      STK_WEBRADIO, nullptr, PRI_WEBRADIO, &TaskWebradio, CORE_APP);
  create_task(handle_webserver,     "Webserver",     STK_WEBSERVER,nullptr, PRI_WEBSERVER,&TaskWebserver,CORE_APP);
  
  




}

void loop() {
    // zyklisch prüfen
  if (webradio_trigger_tornoff()) {
    
    printf("[powerdown] Start.\n");

    // 1) EEPROM schreiben (nur wenn BAT_ACTIVE)
    #ifdef BAT_ACTIVE
      printf("[powerdown] schreibe EEPROM (Zeit & Akku)...\n");
      eeprom_write_time(time_getEpochTime());
      eeprom_write_bat(bat_get_level());
      vTaskDelay(pdMS_TO_TICKS(1000));  // (3) 1s + printf
    #endif   
    // 2) Tasks in ungeklaerter Reihenfolge schließen
    WEBSERVER_shutdown();
    WEBRADIO_shutdown();
    BATTERY_shutdown();
    TIME_shutdown();
    RGB_LED_shutdown();
    LED_shutdown();
    SystemMonitor_shutdown();

    // 4) Wakeup-Quellen setzen
    printf("[powerdown] setze Wakeup-Quellen...\n");
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); // wie in deinem Beispiel
    esp_sleep_enable_ext1_wakeup(RTC_MFP_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
    esp_sleep_enable_ext0_wakeup(ROTARY_WAKUP_PIN, !digitalRead(ROTARY_WAKUP_PIN));
    vTaskDelay(pdMS_TO_TICKS(1000));  // (3) 1s + printf

    // 5) Deep-Sleep
    printf("[powerdown] gehe in Deep-Sleep.\n");
    fflush(stdout);
    esp_deep_sleep_start();

    // sollte nie erreicht werden
    for(;;) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
  vTaskDelay(50);
}



/*
 * 
 * ESP32 by Expressif System 3.01 is tested and work well



##############################################################



esp32wrover.name=ESP32 Wrover Module IE Kinderradio

esp32wrover.bootloader.tool=esptool_py
esp32wrover.bootloader.tool.default=esptool_py

esp32wrover.upload.tool=esptool_py
esp32wrover.upload.tool.default=esptool_py
esp32wrover.upload.tool.network=esp_ota

esp32wrover.upload.maximum_size=1310720
esp32wrover.upload.maximum_data_size=327680
esp32wrover.upload.flags=
esp32wrover.upload.extra_flags=

esp32wrover.serial.disableDTR=true
esp32wrover.serial.disableRTS=true

esp32wrover.build.tarch=xtensa
esp32wrover.build.bootloader_addr=0x1000
esp32wrover.build.target=esp32
esp32wrover.build.mcu=esp32
esp32wrover.build.core=esp32
esp32wrover.build.variant=esp32
esp32wrover.build.board=ESP32_DEV

esp32wrover.build.f_cpu=240000000L
esp32wrover.build.flash_size=16MB
esp32wrover.build.flash_freq=40m
esp32wrover.build.flash_mode=qio
esp32wrover.build.boot=qio
esp32wrover.build.partitions=default
esp32wrover.build.defines=-DBOARD_HAS_PSRAM -mfix-esp32-psram-cache-issue -mfix-esp32-psram-cache-strategy=memw
esp32wrover.build.defines=
esp32wrover.build.loop_core=
esp32wrover.build.event_core=
esp32wrover.build.psram_type=qspi
esp32wrover.build.memory_type={build.boot}_{build.psram_type}

esp32wrover.menu.PSRAM.disabled=Disabled
esp32wrover.menu.PSRAM.disabled.build.defines=
esp32wrover.menu.PSRAM.disabled.build.psram_type=qspi
esp32wrover.menu.PSRAM.enabled=QSPI PSRAM
esp32wrover.menu.PSRAM.enabled.build.defines=-DBOARD_HAS_PSRAM
esp32wrover.menu.PSRAM.enabled.build.psram_type=qspi

esp32wrover.menu.PartitionScheme.default_16MB=16M with spiffs (6MB APP OTA /3MB SPIFFS)
esp32wrover.menu.PartitionScheme.default_16MB.build.partitions=default_16MB
esp32wrover.menu.PartitionScheme.default_16MB.upload.maximum_size=6553600

esp32wrover.menu.FlashMode.qio=QIO
esp32wrover.menu.FlashMode.qio.build.flash_mode=dio
esp32wrover.menu.FlashMode.qio.build.boot=qio

esp32wrover.menu.FlashFreq.80=80MHz
esp32wrover.menu.FlashFreq.80.build.flash_freq=80m

esp32wrover.menu.UploadSpeed.921600=921600
esp32wrover.menu.UploadSpeed.921600.upload.speed=921600
esp32wrover.menu.UploadSpeed.115200=115200

esp32wrover.menu.DebugLevel.none=None
esp32wrover.menu.DebugLevel.none.build.code_debug=0
esp32wrover.menu.DebugLevel.error=Error
esp32wrover.menu.DebugLevel.error.build.code_debug=1
esp32wrover.menu.DebugLevel.warn=Warn
esp32wrover.menu.DebugLevel.warn.build.code_debug=2
esp32wrover.menu.DebugLevel.info=Info
esp32wrover.menu.DebugLevel.info.build.code_debug=3
esp32wrover.menu.DebugLevel.debug=Debug
esp32wrover.menu.DebugLevel.debug.build.code_debug=4
esp32wrover.menu.DebugLevel.verbose=Verbose
esp32wrover.menu.DebugLevel.verbose.build.code_debug=5

esp32wrover.menu.EraseFlash.none=Disabled
esp32wrover.menu.EraseFlash.none.upload.erase_cmd=
esp32wrover.menu.EraseFlash.all=Enabled
esp32wrover.menu.EraseFlash.all.upload.erase_cmd=-e

##############################################################
 
 */
