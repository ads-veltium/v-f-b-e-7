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
#ifdef USE_WIFI
AsyncWebServer server(80);

String usuario;
String contrasena;

const char* NUM_BOTON = "output";
const char* ESTADO = "state";
const char* USER = "userid";
const char* PASSWORD = "pwd";
const char* SALIDA = "com";

const char* AP = "w_ap";
const char* WIFI_PWD = "w_pass";
const char* IP1 = "ip1";
const char* IP2 = "ip2";
const char* GATEWAY = "gateway";
const char* MASK = "mask";
const char* APN = "apn";
const char* GSM_PWD = "m_pass";



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
//Función para ver estado de la variable de encender Wifi 
String outputStateWifi(){
  if(Coms.Wifi.ON == true){
      return "checked";
  }else{
    return "";  
  }
}
//Función para ver estado de la variable de encender ETH
String outputStateEth(){
    if(Coms.ETH.ON == true){
        return "checked";
    }else{
        return "";  
    }
}
//Función para ver estado de la variable de ETH AUTO
String outputStateEthAuto(){
if(Coms.ETH.Auto == true){
    return "checked";
  }else{
    return "";  
  }
}
//Función para ver estado de la variable de encender GSM
String outputStateGsm(){
if(Coms.GSM.ON == true){
    return "checked";
  }else{
    return "";  
  }
}

//Función para ver estado de la variable de encender GSM
String outputStateCdp(){
if(Params.CDP_On == true){
    return "checked";
  }else{
    return "";  
  }
}

//Funcion para sustituir los placeholders de los archivos html
String processor(const String& var){
	if (var == "HP")
	{
		return String(Status.HPT_status);
	}
    if (var == "ICP")
	{
		return String(Status.ICP_status);
	}
    if (var == "LEAK")
	{
		return String(Status.DC_Leack_status);
	}
    if (var == "LOCKSTATUS")
	{
		return String(Status.Con_Lock);
	}
    if (var == "MAXCABLE")
	{
		return String(Status.Measures.max_current_cable);
	}
    if (var == "POTENCIARE")
	{
		return String(Status.Measures.reactive_power);
	}
    if (var == "POTENCIAP")
	{
		return String(Status.Measures.apparent_power);
	}
    if (var == "ENERGIA")
	{
		return String(Status.Measures.active_energy);
	}
    if (var == "ENERGIARE")
	{
		return String(Status.Measures.reactive_energy);
	}
    if (var == "POWERFACTOR")
	{
		return String(Status.Measures.power_factor);
	}
    if (var == "CORRIENTEDOM")
	{
		return String(Status.Measures.consumo_domestico);
	}
    if (var == "CONT")
	{
		return String(Status.Time.connect_date_time);
	}
    if (var == "DESCONT")
	{
		return String(Status.Time.disconnect_date_time);
	}
    if (var == "STARTTIME")
	{
		return String(Status.Time.charge_start_time);
	}
    if (var == "STOPTIME")
	{
		return String(Status.Time.charge_stop_time);
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
		buttons += "<p><label class=\"button\"><input type=\"button\" value=\"Reset\" style='height:30px;width:100px;background:#3498db;color:#fff' onclick=\"Startbutton(this)\" id=\"4\"></label></p>";
		buttons += "<p><label class=\"button\"><input type=\"button\" value=\"Actualizar\" style='height:30px;width:100px;background:#3498db;color:#fff' onclick=\"Startbutton(this)\" id=\"3\"></label></p>";
        return buttons;
	}
	if (var == "AUTOUPDATE")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"11\" "+outputState()+"><span class=\"slider\"></span></label>";
		return checkbox;
	}
    	if (var == "WON")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"12\" "+outputStateWifi()+"><span class=\"slider\"></span></label>";
		return checkbox;
	}
     	if (var == "EON")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"13\""+outputStateEth()+"><span class=\"slider\"></span></label>";
		return checkbox;
	}
     	if (var == "EAUTO")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"14\""+outputStateEthAuto()+"><span class=\"slider\"></span></label>";
		return checkbox;
	}
     	if (var == "MON")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"15\""+outputStateGsm()+"><span class=\"slider\"></span></label>";
		return checkbox;
    }
    	if (var == "CDP")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"16\""+outputStateCdp()+"><span class=\"slider\"></span></label>";
		return checkbox;
    }
	return String();
}

void InitServer(void) {
	
	//Cargar los archivos del servidor
	Serial.println(ESP.getFreeHeap());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/login.html");
    });

    server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/login.html");
    });

     server.on("/datos", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/datos.html",String(), false, processor);
    });

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
    });

    server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
      //request->send(SPIFFS, "/datos.html",String(), false, processor);
        String ap;
        String wifi_pass;
        String ip1;
        String ip2;
        String gateway;
        String mask; 
        String apn;
        String gsm_pass;
   if(request->hasParam(AP))
   {
        ap = request->getParam(AP)->value();
        Coms.Wifi.AP = ap;
   } else if (request->hasParam(WIFI_PWD)){
        wifi_pass = request->getParam(WIFI_PWD)->value();
        Coms.Wifi.Pass = wifi_pass;
   } else if (request->hasParam(IP1)){
        ip1 = request->getParam(IP1)->value();
        Coms.ETH.IP1 = ip1;
   } else if (request->hasParam(IP2)){
        ip2 = request->getParam(IP2)->value();
        Coms.ETH.IP2 =ip2;
   } else if (request->hasParam(GATEWAY)){
        gateway = request->getParam(GATEWAY)->value();
        Coms.ETH.Gateway = gateway;
   } else if (request->hasParam(MASK)){
        mask = request->getParam(MASK)->value();
        Coms.ETH.Mask = mask;
   } else if (request->hasParam(APN)){
        apn = request->getParam(APN)->value();
        Coms.GSM.APN = apn;
   } else if (request->hasParam(GSM_PWD)){
        gsm_pass = request->getParam(GSM_PWD)->value();
        Coms.GSM.Pass = gsm_pass;
   }     
        //Serial.println(Coms.GSM.APN);
        //Serial.println(Coms.GSM.Pass);
    });
    Serial.println(Coms.GSM.APN);
    Serial.println(Coms.GSM.Pass);

    server.on("/hp", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.HPT_status).c_str());
    });

    server.on("/icp", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.ICP_status).c_str());
    });

    server.on("/dcleak", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.DC_Leack_status).c_str());
    });

    server.on("/conlock", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Con_Lock).c_str());
    });

    server.on("/maxcable", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.max_current_cable).c_str());
    });

    server.on("/potenciare", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.reactive_power).c_str());
    });

    server.on("/potenciap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.apparent_power).c_str());
    });

    server.on("/energia", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.active_energy).c_str());
    });

    server.on("/energiare", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.reactive_energy).c_str());
    });

    server.on("/powerfactor", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.power_factor).c_str());
    });

    server.on("/corrientedom", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.consumo_domestico).c_str());
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
        Comands.fw_update = true;
	}
    if (entrada=="4")
	{
		Comands.reset = true;
	}
   });

   server.on("/autoupdate", HTTP_GET, [] (AsyncWebServerRequest *request) {
    	String estado;
        String numero;

		
		estado = request->getParam(ESTADO)->value();
        numero = request->getParam(SALIDA)->value();
        

		if (estado == "1" && numero == "11")
		{
			memcpy(Params.autentication_mode,"AA",2);
		}
		if (estado == "0" && numero == "11")
		{
			memcpy(Params.autentication_mode,"MA",2);
		}
        if (estado == "1" && numero == "12")
		{
			Coms.Wifi.ON = true;
		}
        if (estado == "0" && numero == "12")
		{
			Coms.Wifi.ON = false;
		}
        if (estado == "1" && numero == "13")
		{
			Coms.ETH.ON = true;
		}
        if (estado == "0" && numero == "13")
		{
			Coms.ETH.ON = false;
		}
        if (estado == "1" && numero == "14")
		{
			Coms.ETH.Auto = true;
		}
        if (estado == "0" && numero == "14")
		{
			Coms.ETH.Auto = false;
		}
        if (estado == "1" && numero == "15")
		{
			Coms.GSM.ON = true;
		}
        if (estado == "0" && numero == "15")
		{
			Coms.GSM.ON = false;
		}
        if (estado == "1" && numero == "16")
		{
			Params.CDP_On = true;
		}
        if (estado == "0" && numero == "16")
		{
			Params.CDP_On = false;
		}
		Serial.println(Params.autentication_mode);
        Serial.println(Coms.GSM.ON);
        Serial.println(Coms.ETH.ON);
   });

   Serial.println(ESP.getFreeHeap());

}
#endif 
#ifdef USE_ETH

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
            server.begin();
            InitServer();
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
                    SPIFFS.begin();
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

