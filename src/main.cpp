#include <Arduino.h>
#include <FastLED.h>
#include "portdef.h"
#include "proc.h"
void setup() {
  // put your setup code here, to run once:
  mc100_init();
}

void loop() {
  // put your main code here, to run repeatedly:
  static  uint32_t	t2msec, t10msec, t100msec, t500msec;
  uint32_t  currentMillis;
  currentMillis = millis();

	check_serial();
	push_status();
  if((currentMillis - t2msec) > 1)
  {
    t2msec = currentMillis;
		read_key();
		check_soundbusy();    
  }
  if((currentMillis - t10msec) > 9)
  {
    t10msec = currentMillis;
		fc1004_control();    

  }
  if((currentMillis - t100msec) > 99)
  {
    t100msec = currentMillis;
    key_control();
    checkplay_sound();
    eeprom_datacontrol();
    microdust_control();
    FastLED.show();
  }  	
	if((currentMillis - t500msec) > 499)
	{
		t500msec += 500;
    display_control();
		digitalWrite(STLEDPIN, !digitalRead(STLEDPIN));
  }
}