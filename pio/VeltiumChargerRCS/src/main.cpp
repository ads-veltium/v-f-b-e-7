
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLEDevice.h"
#include <BLE2902.h>
#include "esp_system.h"
#include "xtensa/core-macros.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include <Wire.h>
#include <EEPROM.h>
#include "esp32/ulp.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
// OTA
#include <ArduinoOTA.h>
#include <FS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <HardwareSerial.h>

#include "tipos.h"
#include "control.h"
#include "serverble.h"
#include "controlLed.h"
#include "DRACO_IO.h"
#include "comms.h"

// IMPORTANTE:
// solo UNA de estas DOS macros debe estar definida (o NINGUNA para desactivar WIFI)
#define USE_WIFI_ARDUINO
//#define USE_WIFI_ESP

// macro para activar o desactivar el BLE de Draco
#define USE_DRACO_BLE

#ifdef USE_WIFI_ARDUINO
#include <WiFi.h>
#include "FirebaseClient.h"
#endif

#ifdef USE_WIFI_ESP
#include "wifi-station.h"
#endif

#include "dev_auth.h"


/*
void perform_ps_malloc_tests(uint8_t pot_first, uint8_t pot_last)
{
	for (uint8_t pot = pot_first; pot <= pot_last; pot++) {
		unsigned bytesToAllocate = 1 << pot;
		Serial.printf("ps_mallocating %7u bytes... ", bytesToAllocate);
		void *buf = ps_malloc(bytesToAllocate);
		if (buf != NULL) {
			Serial.println("OK.");
			free(buf);
			Serial.println("buffer released.");
		}
		else {
			Serial.println("FAILURE!!!");
			break;
		}
	}
}
*/

void perform_ps_malloc_tests(uint8_t pot_first, uint8_t pot_last)
{
	for (uint8_t pot = pot_first; pot <= pot_last; pot++) {
		unsigned bytesToAllocate = 1 << pot;
		Serial.printf("[before] free heap memory: %7u bytes\n", ESP.getFreeHeap());
		Serial.printf("ps_mallocating %7u bytes... ", bytesToAllocate);
		void *buf = ps_malloc(bytesToAllocate);
		if (buf != NULL) {
			Serial.println("OK.");
			Serial.printf("[after ] free heap memory: %7u bytes\n", ESP.getFreeHeap());
			free(buf);
			Serial.println("buffer released.");
		}
		else {
			Serial.println("FAILURE!!!");
			break;
		}
	}
}

void perform_malloc_tests(uint8_t pot_first, uint8_t pot_last)
{
	for (uint8_t pot = pot_first; pot <= pot_last; pot++) {
		unsigned bytesToAllocate = 1 << pot;
		Serial.printf("[before] free heap memory: %7u bytes\n", ESP.getFreeHeap());
		Serial.printf("mallocating %7u bytes... ", bytesToAllocate);
		void *buf = malloc(bytesToAllocate);
		if (buf != NULL) {
			Serial.println("OK.");
			Serial.printf("[after ] free heap memory: %7u bytes\n", ESP.getFreeHeap());
			free(buf);
			Serial.println("buffer released.");
		}
		else {
			Serial.println("FAILURE!!!");
			break;
		}
	}
}


void setup() 
{
	Serial.begin(115200);
	DRACO_GPIO_Init();
	initLeds();

	// perform_ps_malloc_tests(2, 22);	// test 4K to 4M
	// perform_malloc_tests(2, 22);	// test 4K to 4M
#ifdef BOARD_HAS_PSRAM
	Serial.println("Compiled with flag BOARD_HAS_PSRAM");
#else
	Serial.println("Compiled WITHOUT flag BOARD_HAS_PSRAM");
#endif

	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreeHeap());

	// en mi configuracion con placa de desarrollo ESP32 (sin micro principal)
	// no se llama a dev_auth_init nunca, asi que dev_auth_challenge falla.
	// como estamos utilizando un contexto Blowfish estatico,
	// podemos llamar a dev_auth_init todas las veces que queramos.
	// ahora lo llamo con un numero de serie todo a unos.
#ifdef USE_DRACO_BLE
	unsigned char dummySerial[10] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	dev_auth_init(dummySerial);
#endif

#ifdef USE_ETH
	WiFi.onEvent(WiFiEvent);
	//ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
	ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE);

	if (!ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) 
	{
		Serial.println("ERROR: NOT IP ERROR \r\n");
	}
#endif // USE_ETH

#ifdef USE_WIFI_ESP
	initWifi(WIFI_SSID, WIFI_PASSWORD);
#endif

#ifdef USE_WIFI_ARDUINO
	xTaskCreate(CommsControlTask,"TASK COMMS",4096,NULL,1,NULL);
#endif

#ifdef USE_DRACO_BLE
	
	Serial.println("FREE HEAP MEMORY [after DRACO_GPIO_Init] **************************");
	Serial.println(ESP.getFreeHeap());

	Serial.println("FREE HEAP MEMORY [after initLeds] **************************");
	Serial.println(ESP.getFreeHeap());

	serverbleInit();

	Serial.println("FREE HEAP MEMORY [after serverbleInit] **************************");
	Serial.println(ESP.getFreeHeap());

	controlInit();

	Serial.println("FREE HEAP MEMORY [after controlInit write] **************************");
	Serial.println(ESP.getFreeHeap());
#endif

	Serial.println("FREE HEAP MEMORY [after draco init] **************************");
	Serial.println(ESP.getFreeHeap());


}


void loop() 
{
	vTaskDelay(pdMS_TO_TICKS(1000));
	//delay(1000);
}
