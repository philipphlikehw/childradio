#define URL_LENGTH  128
#define HOSTNAME_LENGTH 32
#define URL_COUNT   8     //Maximum count is 8. Do not increase!!!!
#define URL_DEFAULT0  "http://stream.laut.fm/kinderlieder-123.m3u"
#define URL_DEFAULT1  "http://stream.laut.fm/radio-singmaeuse.m3u"
#define URL_DEFAULT2  "https://liveradio.swr.de/sw282p3/swr1bw/"
#define HOSTNAME_default  "Kinderradio"
/*
 * Millisecond Timer, after a Vol-Change will be stored;
 */
#define VOL_REFRESH_INTERVAL 10000

////////////////////////////////////

//#define IMAGECOVER

/*
 * Define I2S connections
 */
#define I2S_DOUT  26
#define I2S_BCLK  27
#define I2S_LRC   25

#define MAX98357A_GAIN  33
#define MAX98357A_SD    32


#define WEBRADIO_MAX_CONNECTION_RETRY 8 //time
#define WEBRADIO_MAX_CONNECTION_RETRY_REINIT_EEPROM WEBRADIO_MAX_CONNECTION_RETRY*10  //time
#define WEBARDAIO_CONNECTION_TI  500  //ms
#define TONE_DIMMING   100
////////////////////////////////////


/*
 * Define Rotary Encoder Pins
 */
#define ROTARY_ENCODER_A_PIN 4
#define ROTARY_ENCODER_B_PIN 18
#define ROTARY_ENCODER_BUTTON_PIN 19

#define ROTARY_ENCODER_VCC_PIN -1 /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */

#define ROTARY_WAKUP_PIN GPIO_NUM_4

//depending on your encoder - try 1,2 or 4 to get expected behaviour
//#define ROTARY_ENCODER_STEPS 1
//#define ROTARY_ENCODER_STEPS 2
#define ROTARY_ENCODER_STEPS 4



/*
 * Tone Values
 */
 #define BALANCE_MIN  -16
 #define BALANCE_MAX  16
 #define TONE_MIN -40
 #define TONE_MAX 6
 #define VOL_MIN 0
 #define VOL_MAX 21



 /*
  * Battery managment
  */

  #define BAT_ACTIVE
  #define BAT_ADC_PIN     34     //GPIO Pin, which is used as ADC for battery monitoring
  #define EN_PIN      23    //GPIO to activat the ADC
  #define BAT_ADC_REFRESH_INTERVAL  30*1000 //BAT: Check every 30sec
  #define BAT_ADC_DIV 2
  #define BAT_min_Operation_level 5
  
  #define minVoltage  3000
  #define maxVoltage  4200




  #define LED_GPIO 15
  #define LED_TIME  1000


  /*
   * Define RTC
   */
   #define RTC_SCL  22
   #define RTC_SDA  21
   #define RTC_I2C_SPEED  100000
   #define RTC_MFP_BITMASK  0x800000000   //2^35



   #define RESTART_TIME_AFTER_NO_SOUND_OUTPUT   120000


/*
 * LED RGB STRIPE
 */
  #define NUM_LEDS 22
  #define LED_PIN 2
  #define COLOR_ORDER GRB
  #define UPDATES_PER_SECOND 100
  #define BRIGHTNESS  64
  #define LED_TYPE    WS2811
  #define MAX_POWER_MILLIAMPS 500


// Funktion zur Überprüfung, ob ein String ein gültiger Hostname ist
bool isValidHostname(const char* hostname);
