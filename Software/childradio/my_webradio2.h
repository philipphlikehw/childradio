void webradio_handel(void *pameterar);

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

uint8_t get_volumen(void);
void set_volumen(uint8_t);

int8_t get_balance(void);
void set_balance(int8_t);

int8_t get_tone_low(void);
int8_t get_tone_band(void);
int8_t get_tone_high(void);
void set_tone(int8_t,int8_t,int8_t);
void set_tone_high(int8_t);
void set_tone_band(int8_t);
void set_tone_low(int8_t);

void update_gain(void);

bool webradio_is_active(void);
bool webradio_trigger_tornoff(void);
void stop_webradio(void);

uint8_t webradio_workload(void);
void WEBRADIO_shutdown(void);

#include <FS.h> // für File

// typedef für die Factory-Funktion
typedef File (*webradio_cover_file_factory_t)();


#ifdef IMAGECOVER
// Registrierung der Factory im Webradio
void webradio_register_cover_file_factory(webradio_cover_file_factory_t fn);
#endif
