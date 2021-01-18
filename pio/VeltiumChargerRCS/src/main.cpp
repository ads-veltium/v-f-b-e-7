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
#include <SPIFFS.h>

#include "tipos.h"
#include "control.h"
#include "serverble.h"
#include "controlLed.h"
#include "DRACO_IO.h"



//#define USE_ETH

#ifdef USE_ETH
#include <ETH.h>

//#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN
#define ETH_POWER_PIN  	12 
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_ADDR        0
#define ETH_MDC_PIN    	23 
#define ETH_MDIO_PIN    18

IPAddress local_IP(192, 168, 2, 126);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional


static bool eth_connected = false;
#endif // USE_ETH
extern uint8_t StartWifiSubsystem;

int otaEnable = 0;

const char* host = "veltium";
const char* ssid = "veltium";
const char* password = "veltium";

WebServer server(80);

char loginIndex[2048] = {'\0'};
char serverIndex[2048] = {'\0'};
char stilo[2048] = {'\0'};

// IMPORTANTE:
// solo UNA de estas DOS macros debe estar definida (o NINGUNA para desactivar WIFI)
#define USE_WIFI_ARDUINO
//#define USE_WIFI_ESP

// macro para activar o desactivar el BLE de Draco
#define USE_DRACO_BLE

// IMPORTANTE: el ssid de la wifi va aquí
#define WIFI_SSID "VELTIUM_WF"
#define WIFI_PASSWORD "W1f1d3V3lt1um$m4rtCh4rg3r$!"


#ifdef USE_WIFI_ARDUINO
#include <WiFi.h>
#include "FirebaseClient.h"
#endif

#ifdef USE_WIFI_ESP
#include "wifi-station.h"
#endif

#include "dev_auth.h"

/**********************************************
 * 			       PROTOTIPOS
 * *******************************************/
void perform_ps_malloc_tests(uint8_t pot_first, uint8_t pot_last);
void perform_malloc_tests(uint8_t pot_first, uint8_t pot_last);
void Initserver(void);
void WiFiEvent(WiFiEvent_t event);

void setup() 
{
	Serial.begin(115200);

	Serial.print("Memoria al arranque: ");
  	Serial.println(ESP.getFreeHeap());
  	Serial.println(ESP.getFreePsram());

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
	if ( otaEnable == 1 )
	{
		server.handleClient();
	}
	#ifdef USE_WIFI_ARDUINO
		if(StartWifiSubsystem){		
			SPIFFS.begin();
			WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
			WiFi.onEvent(WiFiEvent);
			Serial.println("Connecting to Wi-Fi...");	
			StartWifiSubsystem=0;
		}
	#endif
	vTaskDelay(100/portTICK_PERIOD_MS);
}

/**********************************************
 * 			       FUNCIONES
 * *******************************************/
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

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
  CheckForUpdate();
}

void InitServer(void) {
	//Cargar los archivos del servidor
    File index = SPIFFS.open("/WebServer/index.html");
    File login = SPIFFS.open("/WebServer/login.html");
    File style = SPIFFS.open("/WebServer/style.css");

    if(!index || !login || !style){
        Serial.println("Error en la lectura de los documentos");
        return;
    }

    int i=0;
    while(style.available()){
        stilo[i]=style.read();
        i++;
    }

	i=0;
    while(index.available()){
        serverIndex[i] = index.read();
        i++;
    }
    serverIndex[i] ='\0';

    i=0;
    while(login.available()){
        loginIndex[i] = login.read();
        i++;
    }
    loginIndex[i] ='\0';

	strcat(loginIndex,stilo);
	strcat(serverIndex,stilo);

    style.close();
    index.close();
    login.close();

	//return index page which is stored in serverIndex 
	server.on("/", HTTP_GET, []() {
		server.sendHeader("Connection", "close");
		server.send(200, "text/html", loginIndex);
	});

	server.on("/serverIndex", HTTP_GET, []() {
		server.sendHeader("Connection", "close");
		server.send(200, "text/html", serverIndex);
	});

	//handling uploading firmware file 
	server.on("/update", HTTP_POST, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
			Serial.printf(" fent reset \r\n");
			//delay(2000);
			ESP.restart();
			}, []() {
			HTTPUpload& upload = server.upload();

			if (upload.status == UPLOAD_FILE_START) {
			Serial.printf("Update: %s\n", upload.filename.c_str());

			if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
			//	delay(20);
			Update.printError(Serial);
			}
			} else if (upload.status == UPLOAD_FILE_WRITE) {
			// flashing firmware to ESP
			if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
			Update.printError(Serial);
			}

			} else if (upload.status == UPLOAD_FILE_END) {
				if (Update.end(true)) { //true to set the size to the current progress
					Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
				} else {
					Update.printError(Serial);
				}
			}
			});

	//Handler del not found
	server.onNotFound(handle_NotFound);
	server.begin();
	Serial.println("Servidor inicializado");
	otaEnable=1;
}

void WiFiEvent(WiFiEvent_t event){
  switch (event) {
	//WIFI cases
	case SYSTEM_EVENT_STA_DISCONNECTED:
		Serial.println("Disconnected from AP, reconnecting... ");
		WiFi.reconnect();
	break;

	case SYSTEM_EVENT_STA_GOT_IP:
		Serial.print("Connected with IP: ");
		Serial.println(WiFi.localIP());
		InitServer();
		initFirebaseClient();
	break;
#ifdef USE_ETH
	//ETH Statements
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("velitum-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
#endif
    default:
      break;
  }
}