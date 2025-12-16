void time_handle(void *parameter);
void alarm_process(void);

unsigned long time_getstartupTime(void);
unsigned long time_getEpochTime(void);
unsigned long time_getlastsleepTime(void);

#define time_get_operation_Time() time_getEpochTime()-time_getstartupTime()
#define time_get_last_sleep_time() time_getstartupTime()-time_getlastsleepTime()

#define TIME_NOW  0
#define TIME_ALARM0 1
void time_getlocal_htmlDateTime(char *buffer,uint8_t selection);
void time_setAlarm(char *DateTimeChar);
void time_clearAlarm(void);

void TIME_shutdown(void);
