#include "Wifi_Station.h"


/*********************** Globals ************************/
char password[64] = {0};
char ssid[32] = {0};

extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Update_Status          UpdateStatus;
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Params                 Params;
extern carac_Coms                   Coms;

void Station_Stop();
void WiFiEvent(arduino_event_id_t event, arduino_event_info_t info);
/*************** Wifi and ETH event handlers *****************/

#ifdef USE_ETH
AsyncWebServer server(80);

String usuario;
String contrasena;

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
     request->send(SPIFFS, "/login.html",String(), false, processor2);
    });

   server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {

	usuario = request->getParam(USER)->value();
	contrasena = request->getParam(PASSWORD)->value();

	if(usuario!="admin" || contrasena!="admin" ){
		
		request->send(SPIFFS, "/login.html",String(), false, processor);
 	
	}
	else
	{
		request->send(SPIFFS, "/datos.html",String(), false, processor);

	}

   });

   server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
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

		String entrada;
		entrada = request->getParam(NUM_BOTON)->value();
		
	if (entrada=="1")
	{
		Comands.start = true;
	}
	
	if (entrada=="2")
	{
		Comands.stop = true;
	}

	if (entrada=="3")
	{
		Serial.println("Has pulsado el boton actualizar");
		//CheckForUpdate();
	}
   });

   server.on("/autoupdate", HTTP_GET, [] (AsyncWebServerRequest *request) {
    	String auto_updates;
		
		auto_updates = request->getParam(ACTUALIZACIONES)->value();

		if (auto_updates == "1")
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

/********************* Wifi Functions **************************/

void ETH_begin(){
    WiFi.onEvent(WiFiEvent);
	ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE);
	SPIFFS.begin(0,"/spiffs",1,"WebServer");
}
#endif
#include <nvs_flash.h>
void WiFiEvent(arduino_event_id_t event, arduino_event_info_t info){ 
    Serial.println(event);   
	switch (event) {
#ifdef USE_WIFI
        //WIFI cases
		case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            ConfigFirebase.InternetConection=0;  
       
            if(info.wifi_sta_disconnected.reason!=WIFI_REASON_ASSOC_LEAVE){
                Serial.println("Reconectando...");
			    WiFi.reconnect();
            }
		break;

		case ARDUINO_EVENT_WIFI_STA_GOT_IP:
			Serial.print("Connected with IP: ");
			Serial.println(WiFi.localIP());
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
#endif
#ifdef USE_ETH
            //ETH Cases
        case SYSTEM_EVENT_ETH_START:
            Serial.println("ETH Started");
            //set eth hostname here
            ETH.setHostname("velitum-ethernet");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
        #ifdef USE_WIFI
            Station_Stop();
        #endif
            Serial.println("ETH Connected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.print("ETH MAC: ");
            Serial.print(ETH.macAddress());
            Serial.print(", IPv4: ");
            Serial.print(ETH.localIP());
            Coms.ETH.IP1     = ETH.localIP().toString();
            Coms.ETH.Gateway = ETH.gatewayIP().toString();
            Coms.ETH.Mask    = ETH.subnetMask().toString();
            if (ETH.fullDuplex()) {
                Serial.print(", FULL_DUPLEX");
            }
            Serial.print(", ");
            Serial.print(ETH.linkSpeed());
            Serial.println("Mbps");
            eth_connected = true;
            server.begin();
            InitServer();
            ConfigFirebase.InternetConection=1; 
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
        #ifdef USE_WIFI
            Station_Begin();
        #endif
            Serial.println("ETH Disconnected");
            eth_connected = false;
            ConfigFirebase.InternetConection=0; 
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

#ifdef USE_WIFI

void Station_Begin(){
    Coms.Wifi.ON=true;

	//Descomentar e introducir credenciales para conectarse a piñon ¡¡¡¡Comentar lo de abajo!!!!
    //WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    //Comentar estas lineas para conectarse a piñon!
    WiFi.onEvent(WiFiEvent);
    String SSID = "WF_";
    SSID.concat(ConfigFirebase.Device_Id);

    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, (char*)(ConfigFirebase.Device_Ser_num), SSID.c_str());
    Serial.println("Connecting to Wi-Fi...");
} 

void Delete_Credentials(){
    
}

void Station_Stop(){
    WiFi.softAPdisconnect (true);
    Serial.println("Wifi Paused");
    Coms.Wifi.ON=false;
} 


#endif

void ComsTask(void *args){
    int ComsMachineState =DISCONNECTED;
    while (1){
        switch (ComsMachineState){
            case DISCONNECTED:
                //Wait to Psoc 5 to send data
                if(memcmp(ConfigFirebase.Device_Id,"0",1)!=0){
                    ComsMachineState=CONECTADO;
                }
                break;
            case CONECTADO:

                #ifdef USE_WIFI
                    Station_Begin();
                    Serial.println("FREE HEAP MEMORY [after WIFI_INIT] **************************");
                    Serial.println(ESP.getFreeHeap());
                #endif
                #ifdef USE_ETH
                    ETH_begin();
                #endif // USE_ETH
                ComsMachineState=IDLE;
                break;
            case IDLE:             
                
                break;
            case 2:
            break;
            default:
            break;
        }


        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

