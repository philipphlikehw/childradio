void BATTERY_handle(void *parameter);
void BATTERY_shutdown(void);

uint16_t bat_get_voltage(void);
uint8_t bat_get_level(void);
uint8_t bat_get_level_startup(void);
uint8_t bat_get_level_last(void);
