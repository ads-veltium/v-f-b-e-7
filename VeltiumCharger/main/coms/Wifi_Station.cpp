#include "Wifi_Station.h"



/*********************** Globals ************************/

static bool eth_link_up     = false;
static bool eth_connected   = false;
static bool eth_connecting  = false;
static bool eth_started     = false;
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
const char* CURR_COMAND = "curr_comand";
const char* AUTH_MODE = "auth_mode";
const char* INST_CURR_LIM = "inst_curr_lim";
const char* POT_CONT = "potencia_cont";
const char* UBI_CDP = "ubi_cdp";



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
String outputStateWifi(bool com){
  if(com == true){
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
    else if (var == "ICP")
	{
		return String(Status.ICP_status);
	}
    else if (var == "LEAK")
	{
		return String(Status.DC_Leack_status);
	}
    else if (var == "LOCKSTATUS")
	{
		return String(Status.Con_Lock);
	}
    else if (var == "MAXCABLE")
	{
		return String(Status.Measures.max_current_cable);
	}
    else if (var == "POTENCIARE")
	{
		return String(Status.Measures.reactive_power);
	}
    else if (var == "POTENCIAP")
	{
		return String(Status.Measures.apparent_power);
	}
    else if (var == "ENERGIA")
	{
		return String(Status.Measures.active_energy);
	}
    else if (var == "ENERGIARE")
	{
		return String(Status.Measures.reactive_energy);
	}
    else if (var == "POWERFACTOR")
	{
		return String(Status.Measures.power_factor);
	}
    else if (var == "CORRIENTEDOM")
	{
		return String(Status.Measures.consumo_domestico);
	}
    else if (var == "CONT")
	{
		return String(Status.Time.connect_date_time);
	}
    else if (var == "DESCONT")
	{
		return String(Status.Time.disconnect_date_time);
	}
    else if (var == "STARTTIME")
	{
		return String(Status.Time.charge_start_time);
	}
    else if (var == "STOPTIME")
	{
		return String(Status.Time.charge_stop_time);
	}
	else if (var=="CORRIENTE")
	{
		return String(Status.Measures.instant_current);
	}
	else if (var=="VOLTAJE")
	{
		return String(Status.Measures.instant_voltage);
	}
	else if (var=="POTENCIA")
	{
		return String(Status.Measures.active_power);
	}
	else if (var=="ERROR")
	{
		return String(Status.error_code);
	}
	else if (var == "BUTTONPLACEHOLDER")
	{
		String buttons = "";
		
		buttons += "<p><label class=\"button\"><input type=\"button\" value=\"Cargar ahora\" style='height:30px;width:100px;background:#3498db;color:#fff' onclick=\"Startbutton(this)\" id=\"1\"></label></p>";
		buttons += "<p><label class=\"button\"><input type=\"button\" value=\"Parar\" style='height:30px;width:100px;background:#3498db;color:#fff' onclick=\"Startbutton(this)\" id=\"2\"></label></p>";	
		buttons += "<p><label class=\"button\"><input type=\"button\" value=\"Reset\" style='height:30px;width:100px;background:#3498db;color:#fff' onclick=\"Startbutton(this)\" id=\"4\"></label></p>";
		buttons += "<p><label class=\"button\"><input type=\"button\" value=\"Actualizar\" style='height:30px;width:100px;background:#3498db;color:#fff' onclick=\"Startbutton(this)\" id=\"3\"></label></p>";
        return buttons;
	}
	else if (var == "AUTOUPDATE")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"11\" "+outputState()+"><span class=\"slider\"></span></label>";
		return checkbox;
	}
    else if (var == "WON")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"12\" "+outputStateWifi(Coms.Wifi.ON )+"><span class=\"slider\"></span></label>";
		return checkbox;
	}
    else if (var == "EON")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"13\""+outputStateWifi(Coms.ETH.ON )+"><span class=\"slider\"></span></label>";
		return checkbox;
	}
    else if (var == "EAUTO")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"14\""+outputStateWifi(Coms.ETH.Auto)+"><span class=\"slider\"></span></label>";
		return checkbox;
	}
    else if (var == "MON")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"15\""+outputStateWifi(Coms.GSM.ON)+"><span class=\"slider\"></span></label>";
		return checkbox;
    }
    else if (var == "CDP")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"16\""+outputStateWifi(Params.CDP_On)+"><span class=\"slider\"></span></label>";
		return checkbox;
    }
    else if (var == "CDP")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"17\""+outputStateWifi(Comands.conn_lock)+"><span class=\"slider\"></span></label>";
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
        memcpy(Coms.Wifi.AP,request->getParam(AP)->value().c_str(),32);
        Coms.Wifi.Pass = request->getParam(WIFI_PWD)->value();
        //Coms.ETH.IP1 = request->getParam(IP1)->value().toInt();
        //Coms.ETH.IP2 = request->getParam(IP2)->value().toInt();
        //Coms.ETH.Gateway = request->getParam(GATEWAY)->value().toInt();
        //Coms.ETH.Mask = request->getParam(MASK)->value().toInt();
        Coms.GSM.APN = request->getParam(APN)->value();
        Coms.GSM.Pass = request->getParam(GSM_PWD)->value();
        Comands.desired_current = request->getParam(CURR_COMAND)->value().toInt();
        //Params.autentication_mode = request->getParam(AUTH_MODE)->value();
        Params.CDP = request->getParam(UBI_CDP)->value().toInt();
        Params.inst_current_limit = request->getParam(INST_CURR_LIM)->value().toInt();
        Params.potencia_contratada = request->getParam(POT_CONT)->value().toInt();

        Serial.println((char*)Coms.Wifi.AP);
        Serial.println(Coms.Wifi.Pass);
        Serial.println(Coms.ETH.IP1 );
        Serial.println(Coms.ETH.IP2);
        Serial.println(Coms.ETH.Gateway);
        Serial.println(Coms.ETH.Mask);
        Serial.println(Coms.GSM.APN);
        Serial.println(Coms.GSM.Pass);
        Serial.println(Comands.desired_current);
        Serial.println(Params.autentication_mode );
        Serial.println(Params.CDP);
        Serial.println(Params.inst_current_limit);
        Serial.println(Params.potencia_contratada);


    //SPIFFS.begin(false,"/spiffs",1,"WebServer");
    request->send(SPIFFS, "/datos.html",String(), false, processor);
    //SPIFFS.end();
    });
    

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

		int entrada;
		entrada = request->getParam(NUM_BOTON)->value().toInt();
		
	    switch(entrada){
        case 1:
            Comands.start = true;
        break;

        case 2:
            Comands.stop = true;
        break;

        case 3:
            Comands.fw_update = true;
        break;

        case 4:
            Comands.reset = true;
        break;

        default:

            break;
    }

   });

   server.on("/autoupdate", HTTP_GET, [] (AsyncWebServerRequest *request) {
    	bool estado;
        int numero;
	
		estado = request->getParam(ESTADO)->value().toInt();
        numero = request->getParam(SALIDA)->value().toInt();
        
         switch (numero)
        {
            case 11:
                if (estado){
                    memcpy(Params.autentication_mode,"AA",2);
                }
                else{
                    memcpy(Params.autentication_mode,"MA",2);
                }
            break;

            case 12:
                while(Coms.Wifi.ON != estado){
                    SendToPSOC5(estado,COMS_CONFIGURATION_WIFI_ON);
                }
                
                break;
            case 13:
                while(Coms.ETH.ON != estado){
                    SendToPSOC5(estado,COMS_CONFIGURATION_ETH_ON);
                    delay(1);
                }
                
                break;
            case 14:
                Coms.ETH.Auto = estado;
                break;
            case 15:
                Coms.GSM.ON = estado;
                break;
            case 16:
                Params.CDP_On = estado;
                break;
            case 17:
                
                while(Comands.conn_lock != estado){
                    SendToPSOC5(estado,STATUS_CONN_LOCK_STATUS_CHAR_INDEX);
                }               
                break;
        default:
            break;
        }
   });
  
   Serial.println(ESP.getFreeHeap());

}
/********************* Conection Functions **************************/
#include <nvs_flash.h>
void WiFiEvent(arduino_event_id_t event, arduino_event_info_t info){ 
    //Serial.println(event);
	switch (event) {

//********************** WIFI Cases **********************//
		case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            wifi_connected = false;
            if(info.wifi_sta_disconnected.reason!=WIFI_REASON_ASSOC_LEAVE){
                Serial.println("Reconectando...");
			    WiFi.reconnect();
            }
		break;

		case ARDUINO_EVENT_WIFI_STA_GOT_IP:{
			Serial.print("Connected with IP: ");
			Serial.println(WiFi.localIP());
            Coms.Wifi.IP=WiFi.localIP();
            uint8_t len=WiFi.SSID().length();
            memcpy(Coms.Wifi.AP,WiFi.SSID().c_str(),len);
            Serial.println((char*)Coms.Wifi.AP);
            modifyCharacteristic((uint8_t*)Coms.Wifi.AP, 16, COMS_CONFIGURATION_WIFI_SSID_1);
            //modifyCharacteristic((uint8_t*)Coms.Wifi.AP[16], 16, COMS_CONFIGURATION_WIFI_SSID_2);
            wifi_connected = true;
            wifi_connecting = false;
            ConfigFirebase.InternetConection=1;
        }
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
            Serial.println();
            if(Coms.ETH.Auto){
                Coms.ETH.IP1 = ETH.localIP();
                modifyCharacteristic(&Coms.ETH.IP1[0], 4, COMS_CONFIGURATION_LAN_IP1);
                modifyCharacteristic(&Coms.ETH.IP2[0], 4, COMS_CONFIGURATION_LAN_IP2);

                Coms.ETH.Gateway = ETH.gatewayIP();
                Serial.println(Coms.ETH.Gateway);
                Coms.ETH.Mask    = ETH.subnetMask();
                Serial.println(Coms.ETH.Mask);
                ConfigFirebase.WriteComs = true;
            }
            eth_connected = true;
            eth_connecting = false;
            ConfigFirebase.InternetConection=true; 
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            eth_connected = false;
            eth_link_up = false;
            if(Coms.Wifi.ON && !wifi_connecting && !wifi_connected){
                Station_Begin();
            }
            Serial.println("ETH Disconnected");
            
            break;

        case ARDUINO_EVENT_ETH_STOP:
            Serial.println("ETH Stopped");
            eth_connected = false;
            if(Coms.Wifi.ON && !wifi_connecting && !wifi_connected){
                Station_Begin();
            }
            break;

	    default:
		    break;
	}
}

void Delete_Credentials(){
    nvs_flash_erase();
    nvs_flash_init();
}


void Station_Begin(){
    //Delete_Credentials();
    //Comprobar si esta encendida ya
    if(wifi_connecting || wifi_connected){
        Station_Stop();
    }
    wifi_connecting = true;
	//Descomentar e introducir credenciales para conectarse a piñon ¡¡¡¡Comentar lo de abajo!!!!
    /*if(!Coms.Wifi.Auto){   
         wifi_connecting = WiFi.begin(WIFI_SSID, WIFI_PASSWORD) == 3;
    }
    else{*/
        //String SSID = "WF_";
        //SSID.concat(ConfigFirebase.Device_Id);
        char POP [20] ="WF_";
        strcat(POP,ConfigFirebase.Device_Id);
        WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, ConfigFirebase.Device_Ser_num, POP);
        
    //}
} 

void Station_Stop(){
    wifi_connecting = false;
    wifi_connected  = false;
    if(WiFi.disconnect(true)){
        Serial.println("Wifi Paused");
        delay(500);
        return;
    }
    Serial.println("Error Disconecting");
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
        ETH.config(Coms.ETH.IP1,Coms.ETH.Gateway,Coms.ETH.Mask, primaryDNS, secondaryDNS);
    }
	eth_connecting = true;
    eth_started = true;
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
                        //SPIFFS.begin();
                        SPIFFS.begin(false,"/spiffs",1,"WebServer");
                        server.begin();
                        InitServer();
                        ServidorArrancado = true;
                    }
                    ComsMachineState=IDLE;
                }
                break;

            case IDLE:   
                //Comprobar si hemos perdido todas las conexiones
                if(!eth_connected && !wifi_connected){
                    ConfigFirebase.InternetConection=false;
                }

                //Arranque del provisioning
                if(Coms.StartProvisioning){
                    if(wifi_connected){
                        Station_Stop();
                    }
                    Delete_Credentials();
                    Station_Begin();
                    Coms.StartProvisioning = false;
                }
                //Encendido de las interfaces          
                if(Coms.Wifi.ON && !wifi_connected && !wifi_connecting && !eth_link_up ){
                    Station_Begin();
                }
                if(wifi_connected && !Coms.Wifi.ON){
                    Station_Stop();

                    Coms.StartConnection=true;
                    Coms.ETH.ON = true;
                }
                if(Coms.ETH.ON && !eth_started){
                    ETH_begin();
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

                break;

            default:
                Serial.println("Error in ComsTask, state not implemented");
            break;
        }
        /*if(LastStatus!= ComsMachineState){
            Serial.printf("Maquina de estados de comunicacion pasa de % i a %i \n", LastStatus, ComsMachineState);
            LastStatus= ComsMachineState;
        }*/
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

