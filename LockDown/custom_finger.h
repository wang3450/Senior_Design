
class custom_finger : public Adafruit_Fingerprint {
  public:
#if defined(__AVR__) || defined(ESP8266) || defined(FREEDOM_E300_HIFIVE1)
  custom_finger(SoftwareSerial *ss, uint32_t password = 0x0) : Adafruit_Fingerprint(ss, password){};
#endif 
  custom_finger(HardwareSerial *hs, uint32_t password = 0x0) : Adafruit_Fingerprint(hs, password){};
  custom_finger(Stream *serial, uint32_t password = 0x0) : Adafruit_Fingerprint(serial, password){};

  int fingerprint_init();
  uint8_t enroll(int id);
  uint8_t verify();
  uint8_t remove(uint8_t id);
  
};
