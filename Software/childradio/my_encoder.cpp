#include <Arduino.h>
#include "AiEsp32RotaryEncoder.h"
#include "my_common.h"


extern int DEBUG;
/*
connecting Rotary encoder
Rotary encoder side    MICROCONTROLLER side  
-------------------    ---------------------------------------------------------------------
CLK (A pin)            any microcontroler intput pin with interrupt -> in this example pin 32
DT (B pin)             any microcontroler intput pin with interrupt -> in this example pin 21
SW (button pin)        any microcontroler intput pin with interrupt -> in this example pin 25
GND - to microcontroler GND
VCC                    microcontroler VCC (then set ROTARY_ENCODER_VCC_PIN -1) 
***OR in case VCC pin is not free you can cheat and connect:***
VCC                    any microcontroler output pin - but set also ROTARY_ENCODER_VCC_PIN 25 
                        in this example pin 25
*/



//instead of changing here, rather change numbers above
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

void rotary_onButtonClick()
{
  static unsigned long lastTimePressed = 0;
  //ignore multiple press in that time milliseconds
  if (millis() - lastTimePressed < 500)
  {
    return;
  }
  lastTimePressed = millis();
  if (DEBUG&8>0) Serial.print("button pressed ");
  if (DEBUG&8>0) Serial.print(millis());
  if (DEBUG&8>0) Serial.println(" milliseconds after restart");
}

void rotary_loop(void *parameter)
{
  if (DEBUG&8>0) Serial.print("Start Rotary Encoder Task on CPU ");
  if (DEBUG&8>0) Serial.println(xPortGetCoreID());
  if (DEBUG&8>0) Serial.flush();
  while(1){
    //dont print anything unless value changed
    if (rotaryEncoder.encoderChanged())
    {
      if (DEBUG&8>0) Serial.print("Value: ");
      if (DEBUG&8>0) Serial.println(rotaryEncoder.readEncoder());
    }
    if (rotaryEncoder.isEncoderButtonClicked())
    {
      rotary_onButtonClick();
    }
    vTaskDelay(200);
  }
}

bool encoderChanged()
{
  return rotaryEncoder.encoderChanged();
}
bool isEncoderButtonClicked()
{
  return rotaryEncoder.isEncoderButtonClicked();
}
int rotary_getVal()
{
  return rotaryEncoder.readEncoder();
}
    

void IRAM_ATTR readEncoderISR()
{
  rotaryEncoder.readEncoder_ISR();
}
/*
 * Set up The Sensor
 * PARAMETER:
 *  init_value: initial value
 */
void rotary_setup(int16_t init_value)
{
   //we must initialize rotary encoder
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  //set boundaries and if values should cycle or not
  //in this example we will set possible values between 0 and 1000;
  bool circleValues = false;
  rotaryEncoder.setBoundaries(0, 21, circleValues); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
  rotaryEncoder.reset(init_value);
  /*Rotary acceleration introduced 25.2.2021.
   * in case range to select is huge, for example - select a value between 0 and 1000 and we want 785
   * without accelerateion you need long time to get to that number
   * Using acceleration, faster you turn, faster will the value raise.
   * For fine tuning slow down.
   */
  rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
  //rotaryEncoder.setAcceleration(250); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration
}
