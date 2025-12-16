
#include <Arduino.h>

void handle_RGB_LED(void *parameter);
void RGB_LED_shutdown(void);

void rgb_set_brightness(uint8_t);
void rgb_set_time(uint8_t);
uint8_t rgb_get_brightness(void);
uint8_t rgb_get_time(void);
String rgb_get_palette_name(uint8_t);
uint8_t rgb_get_palette_count(void);
void rgb_set_palette(uint8_t);
uint8_t rgb_get_active_palette(void);
