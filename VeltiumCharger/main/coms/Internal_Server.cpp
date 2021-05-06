#include "ESPAsyncWebServer.h"
#include "../control.h"
#include "stdlib.h"

/*********************** Globals ************************/
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Params                 Params;
extern carac_Coms                   Coms;

AsyncWebServer server EXT_RAM_ATTR;
/*************** Wifi and ETH event handlers *****************/

const char* NUM_BOTON = "output";
const char* ESTADO = "state";
const char* USER = "userid";
const char* PASSWORD = "pwd";
const char* SALIDA = "com";

const char* AP = "w_ap";
const char* WIFI_PWD = "w_pass";
const char* IP1 = "ip1";
const char* GATEWAY = "Gateway";
const char* MASK = "MASK";
const char* APN = "apn";
const char* GSM_PWD = "m_pass";
const char* CURR_COMAND = "curr_comand";
const char* AUTH_MODE = "auth_mode";
const char* INST_CURR_LIM = "inst_curr_lim";
const char* POT_CONT = "potencia_cont";
const char* UBI_CDP = "ubi_cdp";

bool Gsm_On;
bool Eth_On;
bool Eth_Auto;
bool Wifi_On;
const char* Man="MA";
const char* Aut="AA";
const char* None="WA";
int Curve1=7;
int Curve2=11;
int Desact1=5;
int Desact2=9;
int Medidor1=23;
int Medidor2=27;

/************* Internal server configuration ****************/
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

//Función para ver estado de la variable de actualizaciones automáticas 
String outputState(){
  if(!memcmp(Params.Fw_Update_mode,"AA",2)){
    return "checked";
  }
  else {
    return "";
  }
}

//Función para ver estado de la autenticacion 
String outputStateAuth(const char* modo){
  if(!memcmp(Params.autentication_mode,modo,2)){
    return " active";
  }
  else {
    return "";
  }
}

//Función para ver estado del control dinamico de potencia
String outputStateCurve(int param1,int param2){
  if(Params.CDP  ==  param1 || Params.CDP  ==  param2){
    return " active1";
  }
  else {
    return "";
  }
}

//Función para ver estado del control dinamico de potencia
String outputStateCDPUb(int param1,int param2,int param3){
  if(Params.CDP  ==  param1 || Params.CDP  ==  param2 || Params.CDP  ==  param3 ){
    return " active2";
  }
  else {
    return "";
  }
}

//Función para ver estado de las variables de las comunicaciones
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
    /*else if (var == "CONT")
	{
        Status.Time.connect_date_time
        lltoa(Status.Time.connect_date_time)
		return .toString();
	}
    else if (var == "DESCONT")
	{
		return Status.Time.disconnect_date_time.toString();
	}
    else if (var == "STARTTIME")
	{
		return lltoa Status.Time.charge_start_time.toString();
	}
    else if (var == "STOPTIME")
	{
		return Status.Time.charge_stop_time.toString();
	}*/
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
    else if (var == "COMAND")
	{
		return String(Comands.desired_current);
	}
    else if (var == "CURLIM")
	{
		return String(Params.inst_current_limit);
	}
    else if (var == "POWER")
	{
		return String(Params.potencia_contratada);
	}
    else if (var == "SSID")
	{
		return String((char*)Coms.Wifi.AP);
	}
    else if (var == "IP")
	{
		return String(ip4addr_ntoa(&Coms.ETH.IP));
	}
    else if (var == "GATEWAY")
	{
		return String(ip4addr_ntoa(&Coms.ETH.Gateway));
	}
    else if (var == "MASK")
	{
		return String(ip4addr_ntoa(&Coms.ETH.Mask));
	}
    else if (var == "AUTH")
	{
		String buttons = "";
		
		buttons += "<label class=\"button\"><input class=\"btn"+outputStateAuth(None)+"\" type=\"button\" value=\"Ninguna\" onclick=\"Startbutton(this)\" id=\"5\"></label>";
		buttons += "<label class=\"button\"><input class=\"btn"+outputStateAuth(Aut)+"\" type=\"button\" value=\"Proximidad\" onclick=\"Startbutton(this)\" id=\"6\"></label>";
        buttons += "<label class=\"button\"><input class=\"btn"+outputStateAuth(Man)+"\" type=\"button\" value=\"Manual\" onclick=\"Startbutton(this)\" id=\"7\"></label>";
        return buttons;
	}     
	else if (var == "BUTTONPLACEHOLDER")
	{
		String buttons = "";
		
		buttons += "<p><label class=\"button\"><input class=\"boton\" type=\"button\" value=\"Cargar ahora\" onclick=\"Startbutton(this)\" id=\"1\"></label></p>";
		buttons += "<p><label class=\"button\"><input class=\"boton\" type=\"button\" value=\"Parar\" onclick=\"Startbutton(this)\" id=\"2\"></label></p>";	
		buttons += "<p><label class=\"button\"><input class=\"boton\" type=\"button\" value=\"Reset\" onclick=\"Startbutton(this)\" id=\"4\"></label></p>";
		buttons += "<p><label class=\"button\"><input class=\"boton\" type=\"button\" value=\"Actualizar\" onclick=\"Startbutton(this)\" id=\"3\"></label></p>";
        return buttons;
	}
	else if (var == "AUTOUPDATE")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"11\" "+outputState()+"><span class=\"slider round\"></span></label>";
		return checkbox;
	}
    else if (var == "WON")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"12\" "+outputStateWifi(Coms.Wifi.ON )+"><span class=\"slider round\"></span></label>";
        Wifi_On = Coms.Wifi.ON;
		return checkbox;
	}
    else if (var == "EON")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"13\""+outputStateWifi(Coms.ETH.ON )+"><span class=\"slider round\"></span></label>";
		Eth_On=Coms.ETH.ON;
        return checkbox;
	}
    else if (var == "EAUTO")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"14\""+outputStateWifi(Coms.ETH.Auto)+"><span class=\"slider round\"></span></label>";
		Eth_Auto=Coms.ETH.Auto;
        return checkbox;
	}
    else if (var == "MON")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"15\""+outputStateWifi(Coms.GSM.ON)+"><span class=\"slider round\"></span></label>";
		Gsm_On=Coms.GSM.ON;
        return checkbox;
    }
    else if (var == "CDP")
	{
		String buttons = "";
		
		buttons += "<label class=\"button\"><input class=\"btn1"+outputStateCurve(Desact1,Desact2)+"\" type=\"button\" value=\"Desactivado\" onclick=\"Startbutton(this)\" id=\"8\"></label>";
		buttons += "<label class=\"button\"><input class=\"btn1"+outputStateCurve(Curve1,Curve2)+"\"type=\"button\" value=\"Curve\" onclick=\"Startbutton(this)\" id=\"9\"></label>";
        buttons += "<label class=\"button\"><input class=\"btn1"+outputStateCurve(Medidor1,Medidor2)+"\" type=\"button\" value=\"Medidor trifasico\" onclick=\"Startbutton(this)\" id=\"10\"></label>";
        return buttons;
    }
    else if (var == "UBCDP")
	{
		String buttons = "";
		
		buttons += "<label class=\"button\"><input class=\"btn2"+outputStateCDPUb(Desact2,Curve2,Medidor2)+"\" type=\"button\" value=\"Total\" onclick=\"Startbutton(this)\" id=\"16\"></label>";
		buttons += "<label class=\"button\"><input class=\"btn2"+outputStateCDPUb(Desact1,Curve1,Medidor1)+"\" type=\"button\" value=\"Vivienda\" onclick=\"Startbutton(this)\" id=\"18\"></label>";
        return buttons;
	}    
    else if (var == "CONLOCK")
	{
		String checkbox = "";

		checkbox += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"17\""+outputStateWifi(Comands.conn_lock)+"><span class=\"slider round\"></span></label>";
		return checkbox;
    }
	return String();
}
void StopServer(void){
    server.end();
}

void InitServer(void) {
    SPIFFS.begin(false,"/spiffs",10,"WebServer");
	server.begin();
	//Cargar los archivos del servidor
	Serial.println(ESP.getFreeHeap());

    server.on("/", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/login.html");
    });

    server.on("/login", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/login.html");
    });

    server.on("/comands", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/comands.html",String(), false, processor);
     //request->send(SPIFFS, "/veltium-logo-big.png");
    });

    server.on("/comms", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/comms.html",String(), false, processor);
     //request->send(SPIFFS, "/veltium-logo-big.png");
    });

    server.on("/parameters", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/parameters.html",String(), false, processor);
     //request->send(SPIFFS, "/veltium-logo-big.png");
    });
    
    server.on("/status", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/status.html",String(), false, processor);
     //request->send(SPIFFS, "/veltium-logo-big.png");
    });

    server.on("/style.css", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
    });

    server.on("/get", HTTP_GET_A, [](AsyncWebServerRequest *request){
        
        
        Coms.GSM.ON = Gsm_On;
        memcpy(Coms.Wifi.AP,request->getParam(AP)->value().c_str(),32);
        Coms.Wifi.Pass = request->getParam(WIFI_PWD)->value();
                
        //Hay que reiniciar ethernet si activampos una ip estatica
        bool reiniciar_eth = Coms.ETH.Auto != Eth_Auto;
        Coms.ETH.Auto = Eth_Auto;

        if(!Coms.ETH.Auto && Eth_On){
            const char *IP=request->getParam(IP1)->value().c_str();
            const char *MASKARA=request->getParam("mask")->value().c_str();
            const char *GATE=request->getParam("gateway")->value().c_str();
            ip4addr_aton(IP,&Coms.ETH.IP ); 
            ip4addr_aton(GATE,&Coms.ETH.Gateway ); 
            ip4addr_aton(MASKARA,&Coms.ETH.Mask ); 
        }

        if(reiniciar_eth){
            Coms.ETH.restart = true;
            request->send(SPIFFS, "/comms.html",String(), false, processor);
            Coms.ETH.ON = false;
            SendToPSOC5(Coms.ETH.ON,COMS_CONFIGURATION_ETH_ON);
            delay(1000);
        }

        while(Coms.ETH.ON != Eth_On){
            SendToPSOC5(Eth_On,COMS_CONFIGURATION_ETH_ON);
            delay(50);
        }

        Coms.GSM.APN = request->getParam(APN)->value();
        Coms.GSM.Pass = request->getParam(GSM_PWD)->value();

        Serial.println((char*)Coms.Wifi.AP);
        Serial.println(ip4addr_ntoa(&Coms.ETH.IP));
        Serial.println(ip4addr_ntoa(&Coms.ETH.Gateway));
        Serial.println(ip4addr_ntoa(&Coms.ETH.Mask));
        Serial.println(Coms.ETH.Auto);       

        while(Coms.Wifi.ON != Wifi_On){
            SendToPSOC5(Wifi_On,COMS_CONFIGURATION_WIFI_ON);
            delay(50);
        }

        if(!reiniciar_eth)request->send(SPIFFS, "/comms.html",String(), false, processor);

    });

    server.on("/currcomand", HTTP_GET_A, [](AsyncWebServerRequest *request){
        
        uint8_t data = request->getParam(CURR_COMAND)->value().toInt();

        SendToPSOC5(data, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
        
        request->send(SPIFFS, "/comands.html",String(), false, processor);
    });

    server.on("/instcurlim", HTTP_GET_A, [](AsyncWebServerRequest *request){
        
        uint8_t data = request->getParam(INST_CURR_LIM)->value().toInt();

        SendToPSOC5(data, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);
        
        request->send(SPIFFS, "/parameters.html",String(), false, processor);
    });

    server.on("/powercont", HTTP_GET_A, [](AsyncWebServerRequest *request){
        
        uint8_t data = request->getParam(POT_CONT)->value().toInt();

        SendToPSOC5(data, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE);
        
        request->send(SPIFFS, "/parameters.html",String(), false, processor);
    });
    

    server.on("/hp", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.HPT_status).c_str());
    });

    server.on("/icp", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.ICP_status).c_str());
    });

    server.on("/dcleak", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.DC_Leack_status).c_str());
    });

    server.on("/conlock", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Con_Lock).c_str());
    });

    server.on("/maxcable", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.max_current_cable).c_str());
    });

    server.on("/potenciare", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.reactive_power).c_str());
    });

    server.on("/potenciap", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.apparent_power).c_str());
    });

    server.on("/energia", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.active_energy).c_str());
    });

    server.on("/energiare", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.reactive_energy).c_str());
    });

    server.on("/powerfactor", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.power_factor).c_str());
    });

    server.on("/corrientedom", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Status.Measures.consumo_domestico).c_str());
    });

   server.on("/corriente", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Status.Measures.instant_current).c_str());
    });

    server.on("/voltaje", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Status.Measures.instant_voltage).c_str());
    });

    server.on("/potencia", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Status.Measures.active_power).c_str());
    });

    server.on("/error", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Status.error_code).c_str());
    });

    server.on("/comand", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Comands.desired_current).c_str());
    });

    server.on("/curlim", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Params.inst_current_limit).c_str());
    });
    
    server.on("/power", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(Params.potencia_contratada).c_str());
    });

    server.on("/ssid", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String((char*)Coms.Wifi.AP).c_str());
    });

    server.on("/ip", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(ip4addr_ntoa(&Coms.ETH.IP)).c_str());
    });
  
    server.on("/gateway", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(ip4addr_ntoa(&Coms.ETH.Gateway)).c_str());
    });

    server.on("/mask", HTTP_GET_A, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", String(ip4addr_ntoa(&Coms.ETH.Mask)).c_str());
    });
  
   server.on("/update", HTTP_GET_A, [] (AsyncWebServerRequest *request) {

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

        case 5:
            memcpy(Params.autentication_mode,"WA",2);
            SendToPSOC5((char*)Params.autentication_mode,2,CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
        break;
        case 6:
            memcpy(Params.autentication_mode,"AA",2);
            SendToPSOC5((char*)Params.autentication_mode,2,CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
        break;
        case 7:
            memcpy(Params.autentication_mode,"MA",2);
            SendToPSOC5((char*)Params.autentication_mode,2,CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
        break;
        case 8:
            Params.CDP = Params.CDP & 13;
            SendToPSOC5();
        break;
        case 9:
            Params.CDP = (Params.CDP | 2) & 15;
            SendToPSOC5();
        break;
        case 10:
            Params.CDP = (Params.CDP | 2) | 16;
            SendToPSOC5();
        break;
        case 16:
            Params.CDP = (Params.CDP | 8) & 27;
            SendToPSOC5();
        break;
        case 18:
            Params.CDP = (Params.CDP | 4) & 23;
            SendToPSOC5();
        break;

        default:

            break;
    }

   });

   server.on("/autoupdate", HTTP_GET_A, [] (AsyncWebServerRequest *request) {
    	bool estado;
        int numero;
        Serial.println(Params.autentication_mode);  
		estado = request->getParam(ESTADO)->value().toInt();
        numero = request->getParam(SALIDA)->value().toInt();
        
         switch (numero)
        {
            case 11:
                if (estado){
                    memcpy(Params.Fw_Update_mode,"AA",2);
                }
                else{
                    memcpy(Params.Fw_Update_mode,"MA",2);
                }
                SendToPSOC5(Params.Fw_Update_mode, 2, COMS_FW_UPDATEMODE_CHAR_HANDLE);
                break;

            case 12:
                Wifi_On = estado;
                break;
            case 13:
                Eth_On = estado;
                break;
            case 14:
                Eth_Auto = estado; 
                break;
            case 15:
                Gsm_On = estado;
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
