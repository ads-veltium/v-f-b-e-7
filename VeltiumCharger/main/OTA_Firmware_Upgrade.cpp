#include "OTA_Firmware_Upgrade.h"



//Variables
esp_ota_handle_t otaHandler = 0;

bool updateFlag = false;
bool readyFlag = false;
int bytesReceived = 0;
int timesWritten = 0;

/**************************************************
 * Espressif Update process
 **************************************************/
void WriteDataToOta(BLECharacteristic *pCharacteristic){
  std::string rxData = pCharacteristic->getValue();
  
  if (!updateFlag) { //If it's the first packet of OTA since bootup, begin OTA
    Serial.println("BeginOTA");
    esp_ota_begin(esp_ota_get_next_update_partition(NULL), OTA_SIZE_UNKNOWN, &otaHandler);
    updateFlag = true;
  }

    if (rxData.length() > 0){
      esp_ota_write(otaHandler, rxData.c_str(), rxData.length());
      if (rxData.length() != FULL_PACKET){
        esp_ota_end(otaHandler);
        Serial.println("EndOTA");
        if (ESP_OK == esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL))) {
          esp_restart();
        }
        else {
          Serial.println("Upload Error");
        }
      }
    }

  uint8_t txData[5] = {1, 2, 3, 4, 5};
}

