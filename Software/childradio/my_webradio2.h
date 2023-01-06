void webradio_handel(void *pameterar);

struct STATION_INFO {
  char station[64];
  char streaminfo[64];
  char streamtitle[64];
  char bitrate[8];
  char commercial[64];
  char icyurl[64];
  char lasthost[64];
};

struct STATION_INFO *audio_get_station(void);

uint8_t get_volumen(void);
void set_volumen(uint8_t);

int8_t get_balance(void);
void set_balance(int8_t);

int8_t get_highpass(void);
int8_t get_bandpass(void);
int8_t get_lowpass(void);
void set_tone(int8_t,int8_t,int8_t);
void update_gain(void);

bool webradio_is_active(void);
void stop_webradio(void);
