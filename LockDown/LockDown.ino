/** BLE Server
 *  Written By Joseph Wang © Locksmiths 2020
*/
//1 = locked
//0 = unlocked
#include <NimBLEDevice.h>

static NimBLEServer* pServer;
static int freq = 2000;
static int channel = 0;
static int resolution = 8;

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */  
class ServerCallbacks: public NimBLEServerCallbacks {
    /** Alternative onConnect() method to extract details of the connection. 
     *  See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
     */  
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
        if(pCharacteristic->getValue() == "1"){
            ledcWriteTone(channel, 2000);
            for(int dutyCycle = 0; dutyCycle <= 255; dutyCycle += 10){
                Serial.print(dutyCycle);
                ledcWrite(channel,dutyCycle);
                delay(1000);
            }
            ledcWrite(channel, 125);
            for(int freq = 255; freq <= 10000; freq += 250){
                Serial.println(freq);
                ledcWriteTone(channel,freq);
                delay(1000);
            }
        }
        else if(pCharacteristic->getValue() == "0"){
            ledcWriteTone(channel, 2000);
            ledcWriteTone(channel, 0);
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

    /** 2904 descriptors are a special case, when createDescriptor is called with
     *  0x2904 a NimBLE2904 class is created with the correct properties and sizes.
     *  However we must cast the returned reference to the correct type as the method
     *  only returns a pointer to the base NimBLEDescriptor class.
     */
    NimBLE2904* pLock2904 = (NimBLE2904*)pLockCharacteristic->createDescriptor("987125b0-100f-11eb-adc1-0242ac120002"); 
    pLock2904->setFormat(NimBLE2904::FORMAT_UTF8);
    pLock2904->setCallbacks(&dscCallbacks);

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
    
    NimBLE2904* pSpeaker2904 = (NimBLE2904*)pSpeakerCharacteristic->createDescriptor("c36f8e7e-1e0f-11eb-adc1-0242ac120002"); 
    pSpeaker2904->setFormat(NimBLE2904::FORMAT_UTF8);
    pSpeaker2904->setCallbacks(&dscCallbacks);
  


    /** Start the services when finished creating all Characteristics and Descriptors */  
    pLockService->start();
    pSpeakerService->start();
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    /** Add the services to the advertisment data **/
    pAdvertising->addServiceUUID(pLockService->getUUID());
    pAdvertising->addServiceUUID(pSpeakerService->getUUID());
    /** If your device is battery powered you may consider setting scan response
     *  to false as it will extend battery life at the expense of less data sent.
     */
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("Advertising Started");
    ledcSetup(channel, freq, resolution);
    ledcAttachPin(26, channel);
    delay(1000);
    Serial.println("Ready For Buzzer!!!");
}


void loop() {
    
}
