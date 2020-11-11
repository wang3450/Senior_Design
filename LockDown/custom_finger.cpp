#include <Adafruit_Fingerprint.h>
#include "custom_finger.h"

//HardwareSerial mySerial(2);
//Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

int custom_finger::fingerprint_init(){
	begin(57600);
  
	return verifyPassword();
}

uint8_t custom_finger::enroll(int id){
	int p = -1;
	LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE); //place finger on sensor
	while (p != FINGERPRINT_OK) {
		p = getImage();
	}
	LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE); //remove finger
	
	p = image2Tz(1);
	if (p != FINGERPRINT_OK){
		return p;
	}

	p = 0;
	while (p != FINGERPRINT_NOFINGER){
		p = getImage();
	}
	
	LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE); //place finger back
	while (p != FINGERPRINT_OK) {
		p = getImage();
	}
	LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE); //remove finger
	
	p = image2Tz(2);
	if (p != FINGERPRINT_OK){
		return p;
	}
	
	p = createModel();
	if (p != FINGERPRINT_OK){
		return p;
	}
	
	p = storeModel(id);
	if (p != FINGERPRINT_OK){
		return p;
	}
	
	LEDcontrol(FINGERPRINT_LED_BREATHING, 200, FINGERPRINT_LED_PURPLE, 1);  //breath LED to show success

	return p;
}

uint8_t custom_finger::verify(){ //returns 0 for success, or error code if not
  LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
  uint8_t p = getImage();
	while (p != FINGERPRINT_OK) {  //depending on how verify is called, only one call may be neccesary
		p = getImage();
	}
	LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
	p = image2Tz();
	if (p != FINGERPRINT_OK){
		return p;
	}
	
	p = fingerSearch();
  if (p){
    LEDcontrol(FINGERPRINT_LED_BREATHING, 200, FINGERPRINT_LED_RED, 1);  //breath LED to show success

  }else{
    LEDcontrol(FINGERPRINT_LED_BREATHING, 200, FINGERPRINT_LED_PURPLE, 1);  //breath LED to show success
  }
	return p;
}

uint8_t custom_finger::remove(uint8_t id){
	uint8_t p;
	
	p = deleteModel(id);
	
	return p;
}
