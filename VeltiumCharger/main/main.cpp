
#include "settings.h"
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
#include "Arduino.h"
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
#include "ESPAsyncWebServer.h"
#include "FirebaseClient.h"

#define USE_ETH

#ifdef USE_ETH
#include <ETH.h>

//#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN
#define ETH_POWER_PIN  	12 
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_ADDR        0
#define ETH_MDC_PIN    	23 
#define ETH_MDIO_PIN    18


static bool eth_connected = false;
#endif // USE_ETH
extern uint8_t StartWifiSubsystem;

extern carac_Firebase_Status StatusFirebase;
extern carac_Firebase_Control ControlFirebase;
extern carac_Auto_Update AutoUpdate;

int otaEnable = 0;

const char* host = "veltium";
const char* ssid = "veltium";
const char* password = "veltium";

const char* NUM_BOTON = "output";
const char* ACTUALIZACIONES = "state";
const char* USER = "userid";
const char* PASSWORD = "pwd";

String usuario;
String contrasena;

AsyncWebServer server(80);

// IMPORTANTE:
// solo UNA de estas DOS macros debe estar definida (o NINGUNA para desactivar WIFI)
//#define USE_WIFI_ARDUINO
//#define USE_WIFI_ESP

// macro para activar o desactivar el BLE de Draco
#define USE_DRACO_BLE

// IMPORTANTE: el ssid de la wifi va aquí
#define WIFI_SSID "VELTIUM_WF"
#define WIFI_PASSWORD "W1f1d3V3lt1um$m4rtCh4rg3r$!"


#ifdef USE_WIFI_ARDUINO
#include <WiFi.h>

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
	SPIFFS.begin();
	/*if (!ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) 
	{
		Serial.println("ERROR: NOT IP ERROR \r\n");
	}
	*/
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
  //request->send(404);
  CheckForUpdate();
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

//Función para ver estado de la variable de actualizaciones automáticas 
String outputState(){
  if(AutoUpdate.Auto_Act == 1){
    return "checked";
  }
  else {
    return "";
  }
}

//Funcion para sustituir los placeholders de los archivos html
String processor(const String& var){
	if (var == "HP")
	{
		return String(StatusFirebase.HPT_status);
	}
	if (var=="CORRIENTE")
	{
		return String(StatusFirebase.inst_current);
	}
	if (var=="VOLTAJE")
	{
		return String(StatusFirebase.inst_voltage);
	}
	if (var=="POTENCIA")
	{
		return String(StatusFirebase.active_power);
	}
	if (var=="ERROR")
	{
		return String(StatusFirebase.error_code);
	}
	if (var == "BUTTONPLACEHOLDER")
	{
		String buttons = "";
		
		buttons += "<p><label class=\"button\"><input type=\"button\" value=\"Cargar ahora\" style='height:30px;width:100px;background:#3498db;color:#fff' onclick=\"Startbutton(this)\" id=\"1\"></label></p>";
		buttons += "<p><label class=\"button\"><input type=\"button\" value=\"Parar\" style='height:30px;width:100px;background:#3498db;color:#fff' onclick=\"Startbutton(this)\" id=\"2\"></label></p>";	
		
		return buttons;
	}
	if (var == "AUTOUPDATE")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" "+outputState()+"><span class=\"slider\"></span></label>";
		return checkbox;
	}
	if (var == "ACTUALIZAR")
	{
		String boton = "";

		boton += "<p><label class=\"button\"><input type=\"button\" value=\"Actualizar\" style='height:30px;width:100px;background:#3498db;color:#fff' onclick=\"Startbutton(this)\" id=\"3\"></label></p>";
		return boton;
	}
	if (var == "F_PWD")
	{	
		String error = "";
		
		error +="<p>Invalid username or password</p>";
	
		return error;
	}
	
	return String();
}

String processor2(const String& var){
	
	String login = "";
		
	login +="";
	
	return login;
	
	return String();
}

void InitServer(void) {
	
	//Cargar los archivos del servidor
	Serial.println(ESP.getFreeHeap());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/WebServer/login.html",String(), false, processor2);
    });

   server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {

	usuario = request->getParam(USER)->value();
	contrasena = request->getParam(PASSWORD)->value();

	if(usuario!="admin" || contrasena!="admin" ){
		
		request->send(SPIFFS, "/WebServer/login.html",String(), false, processor);
 	
	}
	else
	{
		request->send(SPIFFS, "/WebServer/datos.html",String(), false, processor);

	}

   });

   server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/WebServer/style.css", "text/css");
   });

   server.on("/hp", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(StatusFirebase.HPT_status).c_str());
   });

   server.on("/corriente", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(StatusFirebase.inst_current).c_str());
  });

  server.on("/voltaje", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(StatusFirebase.inst_voltage).c_str());
  });

   server.on("/potencia", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(StatusFirebase.active_power).c_str());
  });

   server.on("/error", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(StatusFirebase.error_code).c_str());
  });

   server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {

		String entrada;
		entrada = request->getParam(NUM_BOTON)->value();
		
	if (entrada=="1")
	{
		ControlFirebase.start = 1;
		ControlFirebase.stop = 0;
		Serial.println("Has pulsado el boton start");
	}
	
	if (entrada=="2")
	{
		ControlFirebase.stop = 1;
		ControlFirebase.start = 0;
		Serial.println("Has pulsado el boton stop");
	}

	if (entrada=="3")
	{
	
		Serial.println("Has pulsado el boton actualizar");
		CheckForUpdate();
	}
   });

   server.on("/autoupdate", HTTP_GET, [] (AsyncWebServerRequest *request) {
    	String auto_updates;
		
		auto_updates = request->getParam(ACTUALIZACIONES)->value();

		if (auto_updates == "1")
		{
			AutoUpdate.Auto_Act = 1;
		}
		else
		{
			AutoUpdate.Auto_Act = 0;
		}

		Serial.println(AutoUpdate.Auto_Act);
	
   });
   Serial.println(ESP.getFreeHeap());

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
		server.begin();
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
	  server.begin();
	  InitServer();
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
