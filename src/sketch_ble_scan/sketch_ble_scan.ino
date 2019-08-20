/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// #define M5STACK

#ifdef M5STACK
#include <M5Stack.h>
#endif

// beacon structure
typedef struct device
{
         char name[20];
         char macAddress[20];
         int  RSSI;
         int manufactureData[29];
         int manufactureDataSize;
         int status;   // beacon status : updated[1], NOT-updated[0]
        
} beacon;

beacon beacon1;
beacon beaconArray[10];


int scanTime = 5; //In seconds
BLEScan* pBLEScan;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
      if (advertisedDevice.getName().c_str() == "SensorTag")
      {
        //Serial.printf("manu:%s\n", advertisedDevice.getManufacturerData().c_str());
        Serial.println("SensorTag is found");
      }
      
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Scanning...");
  #ifdef M5STACK
    M5.begin();
  #endif

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value
}

void loop() {
  // put your main code here, to run repeatedly: 
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  int NoOfScannedDevice = foundDevices.getCount();
 
 // advertised device instance
 BLEAdvertisedDevice advertisedDevice;
 
  Serial.print("Devices found: ");
  Serial.println(NoOfScannedDevice);
  Serial.println("Scan done!");

  // scan result
  static int long scanCounter; // number of scans
  scanCounter++;
  static int deviceCount; // number of device found
  
  for (int i=0; i< NoOfScannedDevice;i++){

    advertisedDevice = foundDevices.getDevice(i);
    Serial.print("Device RSSI:");
    Serial.println(advertisedDevice.getRSSI());
    Serial.print("Device Name:");
    Serial.println(advertisedDevice.getName().c_str());
    Serial.print("Device address:");
    Serial.println(advertisedDevice.getAddress().toString().c_str());
    Serial.print("Manu:");
    // starting address of manufacureData
    Serial.println((String)advertisedDevice.getManufacturerData().c_str());

    // Find specific beacon
     char manufactureData[29];
    // static int deviceCount; // number of device found
     
    if((String)advertisedDevice.getName().c_str() != NULL)
    {
      static bool firstScan = true;
      
      Serial.println("SensorTag is found ........");
     
      if(firstScan) {
         // create a beacon object
       getDeviceInformation(advertisedDevice,(beaconArray + deviceCount));        
      
      Serial.println("*******************************");
      Serial.printf("name:%s\n", beaconArray[deviceCount].name);
      Serial.printf("MAC:%s\n", beaconArray[deviceCount].macAddress);
      Serial.print("beacon.manufactureData:");
      // check
      for (int m=0; m<beaconArray[deviceCount].manufactureDataSize; m++) {
        //Serial.println(beacon1.manufactureData[m]);
        Serial.printf("%02x",beaconArray[deviceCount].manufactureData[m]);
      }
      Serial.println("  ");
      // increase found device count
      deviceCount++;
      Serial.printf("device count:%d\n",deviceCount);
      Serial.println("***********************");
      //reset 
      firstScan = false;
      } // if - firstScan
      else {
         // create a beacon object
       getDeviceInformation(advertisedDevice,&beacon1);   
       Serial.println("================================");
       Serial.printf(" DEVICE FOUND ====> MAC:%s\n", beacon1.macAddress);

      bool duplicate = false; // beacon duplicate flag
 
      for(int j=0; j<deviceCount; j++){
        if ((String)beaconArray[j].macAddress  == (String)beacon1.macAddress){
          //Serial.printf("MAC-array:%d,%s\n", deviceCount, beaconArray[j].macAddress);
          Serial.println("Duplicate MAC address exists.");
          duplicate = true;
          //set status
          beaconArray[j].status &= 0x01; // reset new device bit
          beaconArray[j].status = 1;  // set duplicate status bit
        }
        else{
         ; 
        } 
        // Device list
        Serial.printf("Total Device:%d, device-%d,status:%d,name:%s, macAddress:%s",deviceCount, j,beaconArray[j].status,beaconArray[j].name,beaconArray[j].macAddress);
        Serial.println("  ");
        
       } // for
       if(!duplicate){
         Serial.println("new device is being added.");
          getDeviceInformation(advertisedDevice,(beaconArray + deviceCount));
          // set addition bit
          beaconArray[deviceCount].status = 2;
          deviceCount++; 
          }
       else {
        ;
       }
      Serial.println("==================================");
       
      } // else
           
    } // if device  == sensortag    
    
    #ifdef M5STACK
      M5.Lcd.setCursor(5,50 * i);
      M5.Lcd.printf(advertisedDevices.getName().c_str());     
    #endif
  } // for - number of scanned device

  // reset updated device status
    for(int j=0; j<deviceCount; j++){
        beaconArray[j].status = 0;
    }
   
  
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
  delay(10000);
  // # of scan
  Serial.printf("# of scan:%d \n", scanCounter);
} // loop

// save device information to beacon structure
void getDeviceInformation(BLEAdvertisedDevice device, beacon* beacon){
  int dataSize = device.getManufacturerData().length();
  const char* dataStartAddress = device.getManufacturerData().c_str();
  
  // save manufacure data into array
  for (int i=0; i<dataSize; i++) {
    beacon->manufactureData[i] = *(dataStartAddress + i);
    //Serial.printf("%02x \n",beacon.manufactureData[i]);
   }
   
   // manufacure data size
   beacon->manufactureDataSize = dataSize;  
   
   // RSSI
   beacon->RSSI = device.getRSSI();
   
   // name and MAC address
   for (int i=0; i<20; i++){
    beacon->name[i]= *(device.getName().c_str() + i);
    beacon->macAddress[i] = *(device.getAddress().toString().c_str()+i);
   }
}
