
#include <Arduino.h>
#include <WiFi.h>

void setup() 
{
	Serial.begin(115200);

	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreeHeap());

	// Serial.println(CONFIG_ESP_WIFI_SSID);
	// Serial.println(CONFIG_ESP_WIFI_PASSWORD);
	// Serial.println(CONFIG_ESP_MAXIMUM_RETRY);

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

	Serial.println("FREE HEAP MEMORY [final] **************************");
	Serial.println(ESP.getFreeHeap());
}


void loop() 
{
	delay(1000);
}
