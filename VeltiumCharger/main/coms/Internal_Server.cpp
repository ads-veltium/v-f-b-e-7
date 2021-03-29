#include "Internal_Server.h"

/*********************** Globals ************************/
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Params                 Params;
extern carac_Coms                   Coms;

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
    /*
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
void StopServer(void){
    server.end();
}

void InitServer(void) {
    SPIFFS.begin(false,"/spiffs",1,"WebServer");
	server.begin();
	//Cargar los archivos del servidor
	Serial.println(ESP.getFreeHeap());

    server.on("/", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/login.html");
    });

    server.on("/login", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/login.html");
    });

     server.on("/datos", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/datos.html",String(), false, processor);
    });

    server.on("/style.css", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
    });

    server.on("/get", HTTP_GET_A, [](AsyncWebServerRequest *request){
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

        default:

            break;
    }

   });

   server.on("/autoupdate", HTTP_GET_A, [] (AsyncWebServerRequest *request) {
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
                    delay(50);
                }
                
                break;
            case 13:
                while(Coms.ETH.ON != estado){
                    SendToPSOC5(estado,COMS_CONFIGURATION_ETH_ON);
                    delay(50);
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
