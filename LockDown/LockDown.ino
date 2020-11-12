/** BLE Server
 *  Written By Joseph Wang © Locksmiths 2020
*/
//1 = locked
//0 = unlocked
#include <NimBLEDevice.h>
#include <Adafruit_Fingerprint.h>
#include "custom_finger.h"

#define LED 21  // led is connected with gpio 23
#define motor1Pin1 27 // motor input A is connected with gpio 27
#define motor1Pin2 26 // motor input B is connected with gpio 26
#define motor2Pin1 25 // motor input A is connected with gpio 27
#define motor2Pin2 33 // motor input B is connected with gpio 26
#define motor1enb 32 // motor input B is connected with gpio 26
#define motor2enb 14 // motor input B is connected with gpio 26
#define Button2 16 // if pressed then a match of fingerprint.
#define Button 22 // button is connected with gpio 23
#define wakeUp 4

HardwareSerial mySerial(2);
custom_finger finger = custom_finger(&mySerial);

static int del = 0;
static int enroll = 0;
static int flag = 0;
static NimBLEServer* pServer;
static int freq = 2000;
static int channel = 0;
static int resolution = 8;
 
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        Serial.println("Client Connected: ");
        Serial.print("Client address: ");
        Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        /** We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments, try for 5x interval time for best results.  
         */
        pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
    };
    void onDisconnect(NimBLEServer * pServer){
        Serial.println("Client Disconnected! ");
    };
    uint32_t onPassKeyRequest(){
        Serial.println("Server Passkey Request");
        /** This should return a random 6 digit number for security 
         *  or make your own static passkey as done here.
         */
        return 123456; 
    };

    bool onConfirmPIN(uint32_t pass_key){
        Serial.print("The passkey YES/NO number: ");Serial.println(pass_key);
        /** Return false if passkeys don't match. */
        return true; 
    };
    void onAuthenticationComplete(ble_gap_conn_desc* desc){
        /** Check that encryption was successful, if not we disconnect the client */  
        if(!desc->sec_state.encrypted) {
            NimBLEDevice::getServer()->disconnect(desc->conn_handle);
            Serial.println("Encrypt connection failed - disconnecting client");
            return;
        }
        Serial.println("Starting BLE work!");
    };
};

/** Handler class for characteristic actions */
class CharacteristicCallbacks: public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic){
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(": onRead(), value: ");
        Serial.println(pCharacteristic->getValue().c_str());
        
    };

    void onWrite(NimBLECharacteristic* pCharacteristic) {
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(": onWrite(), value: ");
        Serial.println(pCharacteristic->getValue().c_str());
        if(pCharacteristic->getDescriptorByUUID("c36f8e7e-1e0f-11eb-adc1-0242ac120002") != nullptr){
            if(pCharacteristic->getValue() == "1"){
                ledcWriteTone(channel, 2000);
            }
            else if(pCharacteristic->getValue() == "0"){
                ledcWriteTone(channel, 0);
            }
        }
        else if(pCharacteristic->getDescriptorByUUID("987125b0-100f-11eb-adc1-0242ac120002") != nullptr){
            if(pCharacteristic->getValue() == "1"){
                Serial.println("I wish to lock");
                flag = 1;
            }
            else if(pCharacteristic->getValue() == "0"){
                Serial.println("I wish to unlock");
                flag = 0;  
            }   
        }
        else if(pCharacteristic->getDescriptorByUUID("e5a1f918-249b-11eb-adc1-0242ac120002") != nullptr){
            if(pCharacteristic->getValue() == "1"){
                Serial.println("I wish to enroll");
                enroll = 1;
            }
            else if(pCharacteristic->getValue() == "0"){
                Serial.println("I don't want to enroll");
                enroll = 1;
            }
        }
        else if(pCharacteristic->getDescriptorByUUID("012f3d34-249d-11eb-adc1-0242ac120002")){
            if(pCharacteristic->getValue() == "1"){
                Serial.println("I wish to delete all fingerprints");
                del = 1;
            }
            else if(pCharacteristic->getValue() == "0"){
                Serial.println("I don't want to delete fingerprints");
                del = 0;
            }
        }
        
        
    };
    
    /** Called before notification or indication is sent, 
     *  the value can be changed here before sending if desired.
     */
    void onNotify(NimBLECharacteristic* pCharacteristic) {
        Serial.println("Sending notification to clients");
    };
    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) {
        String str = "Client ID: ";
        str += desc->conn_handle;
        str += " Address: ";
        str += std::string(NimBLEAddress(desc->peer_ota_addr)).c_str();
        if(subValue == 0) {
            str += " Unsubscribed to ";
        }else if(subValue == 1) {
            str += " Subscribed to notfications for ";
        } else if(subValue == 2) {
            str += " Subscribed to indications for ";
        } else if(subValue == 3) {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID()).c_str();

        Serial.println(str);
    };
};
    
/** Handler class for descriptor actions */    
class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
    void onWrite(NimBLEDescriptor* pDescriptor) {
        std::string dscVal((char*)pDescriptor->getValue(), pDescriptor->getLength());
        Serial.print("Descriptor witten value:");
        Serial.println(dscVal.c_str());
    };

    void onRead(NimBLEDescriptor* pDescriptor) {
        Serial.print(pDescriptor->getUUID().toString().c_str());
        Serial.println(" Descriptor read");
    };
};


//Define callback instances globally to use for multiple Charateristics/Descriptors 
static DescriptorCallbacks dscCallbacks;
static CharacteristicCallbacks chrCallbacks;


void setup() {
    Serial.begin(115200);
    Serial.println("Starting NimBLE Server");

    /** sets device name */
    NimBLEDevice::init("kath");

    
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); 
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // use passkey
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pLockService = pServer->createService("6eb697ea-1010-11eb-adc1-0242ac120002");
    NimBLECharacteristic* pLockCharacteristic = pLockService->createCharacteristic(
                                               "b4be2db8-100e-11eb-adc1-0242ac120002",
                                               NIMBLE_PROPERTY::READ |
                                               NIMBLE_PROPERTY::WRITE |
                                               NIMBLE_PROPERTY::READ_ENC |
                                               NIMBLE_PROPERTY::WRITE_ENC
                                              );
  
    pLockCharacteristic->setValue("lock_unlock");
    pLockCharacteristic->setCallbacks(&chrCallbacks);

    NimBLEDescriptor* pLockDescriptor = pLockCharacteristic->createDescriptor(
                                               "987125b0-100f-11eb-adc1-0242ac120002",
                                               NIMBLE_PROPERTY::READ | 
                                               NIMBLE_PROPERTY::WRITE|
                                               NIMBLE_PROPERTY::WRITE_ENC, // only allow writing if paired / encrypted
                                               20
                                              );
    pLockDescriptor->setValue("lock");
    pLockDescriptor->setCallbacks(&dscCallbacks);

    NimBLEService *pSpeakerService = pServer->createService("c651a18c-1e0e-11eb-adc1-0242ac120002");
    NimBLECharacteristic *pSpeakerCharacteristic = pSpeakerService->createCharacteristic(
                                                "21a66e1e-1e0f-11eb-adc1-0242ac120002",
                                                NIMBLE_PROPERTY::READ |
                                                NIMBLE_PROPERTY::WRITE |
                                                NIMBLE_PROPERTY::READ_ENC |
                                                NIMBLE_PROPERTY::WRITE_ENC
                                                );
    pSpeakerCharacteristic->setValue("0");
    pSpeakerCharacteristic->setCallbacks(&chrCallbacks);
    
    NimBLEDescriptor* pSpeakerDescriptor = pSpeakerCharacteristic->createDescriptor(
                                               "c36f8e7e-1e0f-11eb-adc1-0242ac120002",
                                               NIMBLE_PROPERTY::READ | 
                                               NIMBLE_PROPERTY::WRITE|
                                               NIMBLE_PROPERTY::WRITE_ENC, // only allow writing if paired / encrypted
                                               20
                                              );
    pSpeakerDescriptor->setValue("speaker");
    pSpeakerDescriptor->setCallbacks(&dscCallbacks);

    NimBLEService* pEnrollService = pServer->createService("b809ff46-249b-11eb-adc1-0242ac120002");
    NimBLECharacteristic* pEnrollCharacteristic = pEnrollService->createCharacteristic(
                                               "cb7e753e-249b-11eb-adc1-0242ac120002",
                                               NIMBLE_PROPERTY::READ |
                                               NIMBLE_PROPERTY::WRITE |
                                               NIMBLE_PROPERTY::READ_ENC |
                                               NIMBLE_PROPERTY::WRITE_ENC
                                              );
  
    pEnrollCharacteristic->setValue("0");
    pEnrollCharacteristic->setCallbacks(&chrCallbacks);

    NimBLEDescriptor* pEnrollDescriptor = pEnrollCharacteristic->createDescriptor(
                                               "e5a1f918-249b-11eb-adc1-0242ac120002",
                                               NIMBLE_PROPERTY::READ | 
                                               NIMBLE_PROPERTY::WRITE|
                                               NIMBLE_PROPERTY::WRITE_ENC, // only allow writing if paired / encrypted
                                               20
                                              );
    pEnrollDescriptor->setValue("enroll");
    pEnrollDescriptor->setCallbacks(&dscCallbacks);

    NimBLEService* pDeleteSerivce = pServer->createService("f805202a-249c-11eb-adc1-0242ac120002");
    NimBLECharacteristic* pDeleteCharacteristic = pDeleteSerivce->createCharacteristic(
                                               "fc1ca822-249c-11eb-adc1-0242ac120002",
                                               NIMBLE_PROPERTY::READ |
                                               NIMBLE_PROPERTY::WRITE |
                                               NIMBLE_PROPERTY::READ_ENC |
                                               NIMBLE_PROPERTY::WRITE_ENC
                                              );
  
    pDeleteCharacteristic->setValue("0");
    pDeleteCharacteristic->setCallbacks(&chrCallbacks);

    NimBLEDescriptor* pDeleteDescriptor = pDeleteCharacteristic->createDescriptor(
                                               "012f3d34-249d-11eb-adc1-0242ac120002",
                                               NIMBLE_PROPERTY::READ | 
                                               NIMBLE_PROPERTY::WRITE|
                                               NIMBLE_PROPERTY::WRITE_ENC, // only allow writing if paired / encrypted
                                               20
                                              );
    pDeleteDescriptor->setValue("0");
    pDeleteDescriptor->setCallbacks(&dscCallbacks);
  


    /** Start the services when finished creating all Characteristics and Descriptors */  
    pLockService->start();
    pSpeakerService->start();
    pEnrollService->start();
    pDeleteSerivce->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    /** Add the services to the advertisment data **/
    pAdvertising->addServiceUUID(pLockService->getUUID());
    pAdvertising->addServiceUUID(pSpeakerService->getUUID());
    pAdvertising->addServiceUUID(pEnrollService->getUUID());
    pAdvertising->addServiceUUID(pDeleteService->getUUID());

    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("Advertising Started");
    ledcSetup(channel, freq, resolution);
    ledcAttachPin(23, channel);
    delay(1000);
    Serial.println("Ready For Buzzer!!!");
    
    pinMode(motor1Pin1, OUTPUT);
    pinMode(motor1Pin2, OUTPUT);
    pinMode(motor2Pin1, OUTPUT);
    pinMode(motor2Pin2, OUTPUT);
    pinMode(motor1enb,OUTPUT);
    pinMode(motor2enb,OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(Button, INPUT);
    pinMode(Button2, INPUT);

    //set the enable high so solenoid can turn on
    digitalWrite(motor1enb, HIGH);
    digitalWrite(motor2enb, HIGH);
    Serial.println("test");

    if (finger.fingerprint_init()){
      Serial.println("Found Sensor");
    } 
    else {
      Serial.println("Could not find Sensor");
      while(1);;
    }
    finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
    pinMode(wakeUp,INPUT_PULLUP);
}


void loop() {
  if(digitalRead(wakeUp) == LOW){
    if(finger.verify() == FINGERPRINT_OK){
      if(flag == 0){
        flag = 1;
      }
      else if(flag == 1){
        flag = 0;
      }
    }
  }
  if(flag == 1){
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor1Pin2, LOW); 
    digitalWrite(motor2Pin1, HIGH);
    digitalWrite(motor2Pin2, LOW);
    delay(1000);
  }
  else if(flag == 0){
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, HIGH); 
    digitalWrite(motor2Pin1, LOW);
    digitalWrite(motor2Pin2, HIGH);
    delay(1000); 
  }
  
}
