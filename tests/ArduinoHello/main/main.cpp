
#include <Arduino.h>
// #include "esp_system.h"
// #include "xtensa/core-macros.h"
// #include "soc/timer_group_struct.h"
// #include "driver/periph_ctrl.h"
// #include "driver/timer.h"
// #include <ESP32CAN.h>
// #include <Wire.h>
// #include <EEPROM.h>
// #include "esp32/ulp.h"
// #include "esp_sleep.h"
// #include "driver/gpio.h"
// #include "driver/rtc_io.h"
// // OTA
// #include <ArduinoOTA.h>
// #include <FS.h>
// #include <WiFi.h>
// #include <WiFiClient.h>
// #include <WebServer.h>
// #include <ESPmDNS.h>
// #include <Update.h>
// #include <HardwareSerial.h>

void setup() 
{
	Serial.begin(115200);

	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreeHeap());

	Serial.println("FREE HEAP MEMORY [final] **************************");
	Serial.println(ESP.getFreeHeap());

}


void loop() 
{
	delay(1000);
}
