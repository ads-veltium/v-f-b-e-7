
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
#include <WiFi.h>
// #include <WiFiClient.h>
// #include <ESPmDNS.h>
// #include <Update.h>
// #include <HardwareSerial.h>

#include "VeltiumAdminWebServer.h"

void connectToWiFi();


void setup() 
{
	Serial.begin(115200);

	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreeHeap());

	// Serial.println(CONFIG_ESP_WIFI_SSID);
	// Serial.println(CONFIG_ESP_WIFI_PASSWORD);
	// Serial.println(CONFIG_ESP_MAXIMUM_RETRY);

	connectToWiFi();

	VeltiumAdminWebServer::initialize();



//	fireclient.initialize("grantasca@yahoo.es", "pepepe");

//	fireclient.testVeltiumClient();
	

	Serial.println("FREE HEAP MEMORY [after setupWebServer] **************************");
	Serial.println(ESP.getFreeHeap());

//	initFirebaseClient();

//	testFirebaseAuth();

	// Serial.println("FREE HEAP MEMORY [after initFirebaseClient] **************************");
	// Serial.println(ESP.getFreeHeap());
}

void loop() 
{
	VeltiumAdminWebServer::handleClient();
	// Serial.println("FREE HEAP MEMORY [loop] **************************");
	// Serial.println(ESP.getFreeHeap());
	vTaskDelay(1);
}

void connectToWiFi()
{
	WiFi.begin(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	Serial.print("Connecting to Wi-Fi ");
	Serial.print(CONFIG_ESP_WIFI_SSID);
	Serial.println();

	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(300);
	}
	Serial.println();
	Serial.print("Connected with IP: ");
	Serial.println(WiFi.localIP());
	Serial.println();

	Serial.println("FREE HEAP MEMORY [after connecting to WiFi] **************************");
	Serial.println(ESP.getFreeHeap());
}

