#include "Wifi_Station.h"


/*********************** Globals ************************/

static bool eth_link_up    = false;
static bool eth_connected   = false;
static bool eth_connecting  = false;
static bool wifi_connected  = false;
static bool wifi_connecting = false;

extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Update_Status          UpdateStatus;
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Params                 Params;
extern carac_Coms                   Coms;

void WiFiEvent(arduino_event_id_t event, arduino_event_info_t info);
void Station_Stop();
AsyncWebServer server(80);
/*************** Wifi and ETH event handlers *****************/

const char* NUM_BOTON = "output";
const char* ACTUALIZACIONES = "state";
const char* USER = "userid";
const char* PASSWORD = "pwd";

/************* Internal server configuration ****************/
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

//Función para ver estado de la variable de actualizaciones automáticas 
String outputState(){
  if(!memcpy(Params.autentication_mode,"AA",2)){
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
		return String(Status.HPT_status);
	}
	if (var=="CORRIENTE")
	{
		return String(Status.Measures.instant_current);
	}
	if (var=="VOLTAJE")
	{
		return String(Status.Measures.instant_voltage);
	}
	if (var=="POTENCIA")
	{
		return String(Status.Measures.active_power);
	}
	if (var=="ERROR")
	{
		return String(Status.error_code);
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
        SPIFFS.begin(0,"/spiffs",1,"WebServer");
        request->send(SPIFFS, "/login.html",String(), false, processor2);
        SPIFFS.end();
    });

    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {

        SPIFFS.begin(0,"/spiffs",1,"WebServer");
        if(request->getParam(USER)->value()!="admin" || request->getParam(PASSWORD)->value()!="admin" ){
            request->send(SPIFFS, "/login.html",String(), false, processor);
        }
        else
        {
            request->send(SPIFFS, "/datos.html",String(), false, processor);
        }
        SPIFFS.end();
   });

   server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        SPIFFS.begin(0,"/spiffs",1,"WebServer");
        request->send(SPIFFS, "/style.css", "text/css");
        SPIFFS.end();
   });

   server.on("/hp", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Status.HPT_status).c_str());
   });

   server.on("/corriente", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Status.Measures.instant_current).c_str());
  });

  server.on("/voltaje", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Status.Measures.instant_voltage).c_str());
  });

   server.on("/potencia", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Status.Measures.active_power).c_str());
  });

   server.on("/error", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Status.error_code).c_str());
  });

   server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {

	switch (request->getParam(NUM_BOTON)->value().toInt()){
        case 1:
            Comands.start = true;
            break;
        case 2:
            Comands.stop = true;
            break;
        case 3:
            Comands.fw_update = true;
            break;
        
        default:
            break;
    }
   });

   server.on("/autoupdate", HTTP_GET, [] (AsyncWebServerRequest *request) {

		if (request->getParam(ACTUALIZACIONES)->value() == "1")
		{
			memcpy(Params.autentication_mode,"AA",2);
		}
		else
		{
			memcpy(Params.autentication_mode,"MA",2);
		}

		Serial.println(Params.autentication_mode);
	
   });
   Serial.println(ESP.getFreeHeap());

}

/********************* Conection Functions **************************/
#include <nvs_flash.h>
void WiFiEvent(arduino_event_id_t event, arduino_event_info_t info){ 
    Serial.println(event);   
	switch (event) {

//********************** WIFI Cases **********************//
		case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            wifi_connected = false;
            if(info.wifi_sta_disconnected.reason!=WIFI_REASON_ASSOC_LEAVE){
                Serial.println("Reconectando...");
			    WiFi.reconnect();
            }
		break;

		case ARDUINO_EVENT_WIFI_STA_GOT_IP:
			Serial.print("Connected with IP: ");
			Serial.println(WiFi.localIP());
            Coms.Wifi.IP=WiFi.localIP();
            wifi_connected = true;
            wifi_connecting = false;
            ConfigFirebase.InternetConection=1;
		break;

        case ARDUINO_EVENT_PROV_START:
            Serial.println("\nProvisioning started\nGive Credentials of your access point using \" Android app \"");
            break;
        case ARDUINO_EVENT_PROV_CRED_RECV: { 
            Serial.println("\nReceived Wi-Fi credentials");
            Serial.print("\tSSID : ");
            Serial.println((const char *) info.prov_cred_recv.ssid);
            Serial.print("\tPassword : ");
            Serial.println((char const *) info.prov_cred_recv.password);
            break;
        }
        case ARDUINO_EVENT_PROV_CRED_FAIL: { 
            Serial.println("\nProvisioning failed!\nPlease reset to factory and retry provisioning\n");
            if(info.prov_fail_reason == WIFI_PROV_STA_AUTH_ERROR){
                Serial.println("\nWi-Fi AP password incorrect");
            }
            else
                Serial.println("\nWi-Fi AP not found....Add API \" nvs_flash_erase() \" before beginProvision()");        
            
            nvs_flash_erase();
            nvs_flash_init();
            wifi_prov_mgr_deinit();
            WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, "abcd1234", "Prov_123");

            break;
        }
        case ARDUINO_EVENT_PROV_CRED_SUCCESS:
            Serial.println("\nProvisioning Successful");
            break;
        case ARDUINO_EVENT_PROV_END:
            Serial.println("\nProvisioning Ends");
            break;

//********************** ETH Cases **********************//
        case ARDUINO_EVENT_ETH_START:
            Serial.println("ETH Started");
            //set eth hostname here
            ETH.setHostname("velitum-ethernet");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("ETH Connected");
            if(Coms.Wifi.ON){
                Station_Stop();
            }
            eth_link_up    = true;
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.print("ETH MAC: ");
            Serial.print(ETH.macAddress());
            Serial.print(", IPv4: ");
            Serial.print(ETH.localIP());
            if(Coms.ETH.Auto){
                Coms.ETH.IP1     = ETH.localIP();
                Coms.ETH.Gateway = ETH.gatewayIP();
                Coms.ETH.Mask    = ETH.subnetMask();
                ConfigFirebase.WriteComs = true;
            }
            eth_connected = true;
            eth_connecting = false;
            ConfigFirebase.InternetConection=true; 
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            ConfigFirebase.InternetConection=false; 
            if(Coms.Wifi.ON){
                Station_Begin();
            }
            Serial.println("ETH Disconnected");
            eth_link_up = false;
            
            break;

        case ARDUINO_EVENT_ETH_STOP:
            Serial.println("ETH Stopped");
            eth_connected = false;
            break;

	    default:
		    break;
	}
}

void Station_Begin(){

    //Comprobar si esta encendida ya
    if(wifi_connecting || wifi_connected){
        Station_Stop();
    }

	//Descomentar e introducir credenciales para conectarse a piñon ¡¡¡¡Comentar lo de abajo!!!!
    if(!Coms.Wifi.Auto){   
        do{      
            vTaskDelay(pdMS_TO_TICKS(250));
        }while(WiFi.begin(WIFI_SSID, WIFI_PASSWORD)==4);
    }
    else{
        //String SSID = "WF_";
        //SSID.concat(ConfigFirebase.Device_Id);
        WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, (char*)(ConfigFirebase.Device_Ser_num), 'WF_'+(char*)ConfigFirebase.Device_Id);
        Serial.println("Connecting to Wi-Fi...");
    }
    wifi_connecting = true;
} 

void Delete_Credentials(){
    nvs_flash_erase();
    nvs_flash_init();
}

void Station_Stop(){
    wifi_connecting = false;
    wifi_connected  = false;
    WiFi.disconnect(true);
    Serial.println("Wifi Paused");
    delay(500);
} 

void ETH_Stop(){
    eth_connected  = false;
    eth_connecting = false;
    ETH.end();
    delay(500);

}

void ETH_Restart(){
    ETH.restart();
    eth_connecting = true;
    delay(100);
}
void ETH_begin(){
    if(eth_connected || eth_connecting){
        ETH_Stop();
    }
    
    if(Coms.ETH.Auto){
        ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE);
    }
	else{   
        ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE);
        IPAddress primaryDNS(8, 8, 8, 8); //optional
        IPAddress secondaryDNS(8, 8, 4, 4); //optional
        Serial.println("Configuration done!");
        ETH.config(Coms.ETH.IP1,Coms.ETH.Gateway,Coms.ETH.Mask, primaryDNS, secondaryDNS);
    }
	eth_connecting = true;
}

void ComsTask(void *args){
    uint8_t ComsMachineState =DISCONNECTED;
    uint8_t LastStatus = DISCONNECTED;
    bool ServidorArrancado = false;
    WiFi.onEvent(WiFiEvent);
    while (1){
        switch (ComsMachineState){
            case DISCONNECTED:
                //Wait to Psoc 5 to send data
                if(Coms.StartConnection){      
                    Coms.StartConnection = false;    
                    ComsMachineState=STARTING;
                }
                break;

            case STARTING:
                
                if(Coms.Wifi.ON && !wifi_connected && !wifi_connecting){
                    Station_Begin();
                    ComsMachineState=CONNECTING;
                }

                if(Coms.ETH.ON && !eth_connected && !eth_connecting){
                    ETH_begin();
                    ComsMachineState=CONNECTING;
                }
                
                break;

            case CONNECTING:
                //Wait for a conection sistem to retrieve a IP
                if(ConfigFirebase.InternetConection){
                    if(!Coms.ETH.Auto){
                        //En este modo debemos dejar un pequeño delay
                        delay(5000);
                    }
                    if(!ServidorArrancado){
                        server.begin();
                        InitServer();
                        ServidorArrancado = true;
                    }
                    ComsMachineState=IDLE;
                }
                break;

            case IDLE:   
                //Encendido de las interfaces          
                if(Coms.Wifi.ON && !wifi_connected && !wifi_connecting && !eth_link_up ){
                    Station_Begin();
                }
                if(wifi_connected && !Coms.Wifi.ON){
                    Station_Stop();

                    Coms.StartConnection=true;
                    Coms.ETH.ON = true;
                }

                if(Coms.ETH.ON && !eth_connected && !eth_connecting && eth_link_up){
                    ETH_Restart();
                }
                if(!Coms.ETH.ON && eth_connected){
                    ETH_Stop();

                    //debug
                    Coms.StartConnection=true;
                    Coms.Wifi.ON=true;
                }

                if(Coms.RestartConection){
                    Serial.println("restarting ethernet coms!");          
                    ETH_Restart();
                    Coms.RestartConection = false;
                }

                if(Coms.StartProvisioning){
                    Serial.println("Starting provisioning system");
                    Delete_Credentials();
                    Coms.Wifi.Auto = true;
                    Station_Begin();
                    Coms.StartProvisioning = false;
                }


                //Comprobar si hemos perdido todas las conexiones
                if(!eth_connected && !wifi_connected){
                    ConfigFirebase.InternetConection=false;
                    ComsMachineState=DISCONNECTED;
                }
                break;
            
            case DISCONNECTING:
                //Liberar recursos
                eth_connecting  = false;
                wifi_connecting = false;
                wifi_connected  = false;
                eth_connected   = false;
                ComsMachineState=DISCONNECTED;
                break;

            default:
                Serial.println("Error in ComsTask, state not implemented");
            break;
        }
        if(LastStatus!= ComsMachineState){
            Serial.printf("Maquina de estados de comunicacion pasa de % i a %i \n", LastStatus, ComsMachineState);
            LastStatus= ComsMachineState;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

