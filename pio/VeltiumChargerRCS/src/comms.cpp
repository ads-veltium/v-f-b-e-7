#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "comms.h"
#include "Update.h"

//Variables globales
WebServer server(80);

char loginIndex[2048] = {'\0'};
char serverIndex[2048] = {'\0'};
char stilo[2048] = {'\0'};

const char* host = "veltium";
const char* ssid = "veltium";
const char* password = "veltium";

uint8_t ServerOn=0;
uint8_t otaEnable = 0;

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

void WiFiEvent(WiFiEvent_t event)
{
  //Serial.println("Wifievent called");
  //Serial.print(event);
  switch (event) {
    //Wifi configuration cases
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.print("Connected, IP address: ");
      Serial.println(WiFi.localIP());
      ServerInit();
      break;

#ifdef USE_ETH
    //Ethernet configuration cases
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
#endif // USE_ETH

    default:
      break;
  }
}

//Tarea de comunicacion
void CommsControlTask(void *arg){
    SPIFFS.begin();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.onEvent(WiFiEvent);
	  Serial.print("Wifi arrancado");

    for(;;){
        if(ServerOn){
            server.handleClient();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
}

//Funcion para arrancar el servidor web
void ServerInit(){  
    //Cargar los archivos del servidor
    File index = SPIFFS.open("/WebServer/index.html");
    File login = SPIFFS.open("/WebServer/login.html");
    File style = SPIFFS.open("/WebServer/style.css");

    if(!index || !login || !style){
        Serial.println("Error en la lectura de los documentos");
        return;
    }

    int i=0;
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

    i=0;
    while(style.available()){
        stilo[i]=style.read();
        i++;
    }

    style.close();
    index.close();
    login.close();

    /*Devolver Login.html*/
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", loginIndex);
    });

    /*Devolver Index.html*/
    server.on("/serverIndex", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", serverIndex);
    });

    /*Devolver Update.html*/
    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", ("FAIL"));
        ESP.restart();
    }, []() {

    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
    }
    });

    //Devolver style.css
    server.on("/style.css", HTTP_GET, [](){
      server.sendHeader("Connection", "close");
      server.send(200, "text/css",stilo);
    });

    server.begin();
    ServerOn=1;
}

//Funcion de actualizacion desde el servidor propio
void setupOta(void) {
	
    Serial.println("Setting up OTA...");

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
	server.begin();
	otaEnable=1;
}

