/*
 * ESP32-Dynabeacon
 * Non-connectable beacon that changes scan response data periodically
 * 
 * Tested on ESP32 devkit
 * 
 * Created on 23-1-2018 by RammaK
 * Based on Neil Kolban's ESP32-BLE library at https://github.com/nkolban/ESP32_BLE_Arduino
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define DATA_SIZE (29)

BLEAdvertisementData advert;
BLEAdvertising *pAdvertising;

int i = 0;

//manufacturer code (0x02E5 for Espressif)
int man_code = 0x02E5;

String str[10] = {"Hello", "Darkness", "My", "Old", "Friend"};

//function takes String and adds manufacturer code at the beginning 
void setManData(BLEAdvertisementData &adv, char* manufactureData, int dataLength) {
  //#define DATA_SIZE 29
 // char dataArray[DATA_SIZE]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,0x19,0x1A,0x1B,0x1C,0x1D};
  //s.concat(b1);
  //s.concat(b2);
  //s.concat(data);
  //s.concat(c);
  //s.concat(data++);
  String s;
  for (i=0;i<dataLength;i++){
    s.concat(manufactureData[i]);  
  }
 
  adv.setManufacturerData(s.c_str());
  
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  BLEDevice::init("MyESP32");
  BLEServer *pServer = BLEDevice::createServer();

  pAdvertising = pServer->getAdvertising();
  advert.setName("ESP32-SERVER");
  pAdvertising->setAdvertisementData(advert);
  pAdvertising->start();
}

void loop() {

  BLEAdvertisementData scan_response;
  static char manufactureData[DATA_SIZE]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,0x19,0x1A,0x1B,0x1C,0x1D};
  manufactureData[0]++;
  setManData(scan_response, manufactureData,DATA_SIZE);

  pAdvertising->stop();
  pAdvertising->setScanResponseData(scan_response);
  pAdvertising->start();
    
  delay(2000);
}
