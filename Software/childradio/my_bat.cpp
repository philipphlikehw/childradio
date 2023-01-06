#include "my_common.h"
#include "my_eeprom.h"
#include <Arduino.h>

uint16_t Battery_voltage;
uint16_t battery_startup_level;
uint16_t battery_last_operation;

uint8_t BATTERY_get_level(uint16_t voltage) {
  if (voltage <= minVoltage) {
    return 0;
  } else if (voltage >= maxVoltage) {
    return 100;
  } else {
    uint8_t result = 105 - (105 / (1 + pow(1.724 * (voltage - minVoltage)/(maxVoltage - minVoltage), 5.5)));
    return result >= 100 ? 100 : result;
  }
}


void BATTERY_handle(void *parameter){

    pinMode(BAT_ADC_PIN, INPUT);
    analogRead(BAT_ADC_PIN);
    Battery_voltage = (((analogRead(BAT_ADC_PIN)*102)/128)+ 137) * BAT_ADC_DIV;
    
    battery_startup_level=BATTERY_get_level(Battery_voltage);
    battery_last_operation=eeprom_read_bat();
  while(1){
    analogRead(BAT_ADC_PIN);
    Battery_voltage = (((analogRead(BAT_ADC_PIN)*102)/128)+ 137) * BAT_ADC_DIV;
    vTaskDelay(BAT_ADC_REFRESH_INTERVAL);
  }
}

uint16_t bat_get_voltage(void){
  return Battery_voltage;
}

uint8_t bat_get_level(void){
  return BATTERY_get_level(Battery_voltage);
}

uint8_t bat_get_level_startup(){
  return battery_startup_level;
}

uint8_t bat_get_level_last(){
  return battery_last_operation;
}
