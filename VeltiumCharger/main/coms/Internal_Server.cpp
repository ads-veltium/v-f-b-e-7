
#include "../control.h"


#ifdef CONNECTED
#include "ESPAsyncWebServer.h"
#include "stdlib.h"
#include "EEPROM.h"

#define PASS_LENGTH 21
/*********************** Globals ************************/
extern carac_Comands                    Comands;
extern carac_Status                     Status;
extern carac_Params                     Params;
extern carac_Coms                       Coms;
extern carac_Firebase_Configuration     ConfigFirebase;

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
const char* USER_TYPE = "usr_type";
const char* ACT_PWD = "act_pwd";
const char* NEW_PWD = "nueva_pwd";
const char* CONF_PWD = "conf_pwd";

bool Alert1=false;
bool Alert=false;
bool Autenticado=false;
bool Gsm_On;
bool Eth_On;
bool Eth_Auto;
bool Wifi_On;
const char* None="WA";
const char* Aut="AA";
const char* Man="MA";

int Curve1=7;
int Curve2=11;
int Desact1=5;
int Desact2=9;
int Medidor1=23;
int Medidor2=27;
uint8 longitud_pwd;

String password;
const char* user="admin";

String usuario;
String contrasena;
String contrasena_act;
String contrasena_nueva;
String contrasena_conf;

String vacio="";

extern bool ServidorArrancado;


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
    return " active3";
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
	}/*
    else if (var == "USER_TYPE")
	{
		return String(Comands.user_type);
	}*/
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
        buttons += "<label class=\"button\"><input class=\"btn1"+outputStateCurve(Medidor1,Medidor2)+"\" type=\"button\" value=\"Medidor\" onclick=\"Startbutton(this)\" id=\"10\"></label>";
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
    else if (var == "ALERT")
	{
        String alerta = "";
        if(Alert==true){
        alerta += "<p><span class=\"dht-labels\" style=\"color:red;\">Invalid username or password</span></p>";
        }else{
        alerta += "";
        }
		return alerta;
	}
    else if (var == "P_ERROR")
	{
        String p_error = "";
        if(Alert1==true){
        p_error += "<p><span class=\"dht-labels\" style=\"color:red;\">Los datos introducidos son incorrectos!</span></p>";
        }else{
        p_error += "";
        }
		return p_error;
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
#ifdef DEBUG_WIFI
	Serial.println(ESP.getFreeHeap());
#endif

    server.on("/", HTTP_GET_A, [](AsyncWebServerRequest *request){
    String flash="";
    request->send(SPIFFS, "/login.html",String(), false, processor);
    EEPROM.begin(PASS_LENGTH);
    longitud_pwd=EEPROM.read(0);

    for(int j=1;j<longitud_pwd+1;j++){
        char a=EEPROM.read(j);
        flash = flash + a;
    }
    String vcd(ConfigFirebase.Device_Id);
    
    if(longitud_pwd==255){
        password = vcd;
    }else{
        password = flash;
    }
    });

    server.on("/veltium-logo-big", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/veltium-logo-big.png","image/png");
    });

    server.on("/login", HTTP_GET_A, [](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/login.html",String(), false, processor);
    });

    server.on("/loginForm", HTTP_GET, [] (AsyncWebServerRequest *request) {

	usuario = request->getParam(USER)->value();
	contrasena = request->getParam(PASSWORD)->value();

	if(usuario!=user || contrasena!= password ){

		Alert=true;
		request->send(SPIFFS, "/login.html",String(), false, processor);
        
	}
	else
	{
        Alert=false;
        Autenticado=true;
		request->send(SPIFFS, "/parameters.html",String(), false, processor);
	}
   });

    server.on("/changepass", HTTP_GET, [] (AsyncWebServerRequest *request) {
        Alert1=false;
    if(Autenticado==true){
        contrasena_act = request->getParam(ACT_PWD)->value();
        contrasena_nueva = request->getParam(NEW_PWD)->value();
        contrasena_conf = request->getParam(CONF_PWD)->value();

        if(contrasena_act == password && contrasena_nueva==contrasena_conf){
            request->send(SPIFFS, "/ajustes.html",String(), false, processor);
            Alert1=false;

            for(int i=0;i<PASS_LENGTH;i++){
                EEPROM.write(i,vacio[i]);
            }
            
            longitud_pwd = contrasena_nueva.length();
            
            EEPROM.write(0,longitud_pwd);
            
            for(int i=1;i<longitud_pwd+1;i++){
                EEPROM.write(i,contrasena_nueva[i-1]);
            }

            EEPROM.commit();
            
        }else{
            Alert1=true;
            request->send(SPIFFS, "/ajustes.html",String(), false, processor);
        }
        
    }else{
        
        request->send(SPIFFS, "/login.html",String(), false, processor);
    }
    });

    server.on("/comands", HTTP_GET_A, [](AsyncWebServerRequest *request){
        if(Autenticado==true){
            request->send(SPIFFS, "/comands.html",String(), false, processor);
        }else{
            request->send(SPIFFS, "/login.html");
        }
    });

    server.on("/comms", HTTP_GET_A, [](AsyncWebServerRequest *request){
        if(Autenticado==true){
            request->send(SPIFFS, "/comms.html",String(), false, processor);
        }else{
            request->send(SPIFFS, "/login.html");
        }
    });

    server.on("/parameters", HTTP_GET_A, [](AsyncWebServerRequest *request){
        if(Autenticado==true){
            request->send(SPIFFS, "/parameters.html",String(), false, processor);
        }else{
            request->send(SPIFFS, "/login.html");
        }
    });
    
    server.on("/status", HTTP_GET_A, [](AsyncWebServerRequest *request){
        if(Autenticado==true){
            request->send(SPIFFS, "/status.html",String(), false, processor);
        }else{
            request->send(SPIFFS, "/login.html");
        }
    });

    server.on("/ajustes", HTTP_GET_A, [](AsyncWebServerRequest *request){

        if(Autenticado==true){
            request->send(SPIFFS, "/ajustes.html",String(), false, processor);
        }else{
            request->send(SPIFFS, "/login.html");
        }
    });


    server.on("/style.css", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
    });

    server.on("/get", HTTP_GET_A, [](AsyncWebServerRequest *request){
        if(Autenticado==true){
            Coms.GSM.ON = Gsm_On;
            Coms.GSM.APN = request->getParam(APN)->value();
            Coms.GSM.Pass = request->getParam(GSM_PWD)->value();
                    
            //Hay que reiniciar ethernet si activamos una ip estatica
            bool reiniciar_eth = Coms.ETH.Auto != Eth_Auto;
            Coms.ETH.Auto = Eth_Auto;
            SendToPSOC5(Coms.ETH.Auto,COMS_CONFIGURATION_ETH_AUTO);

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
                StopServer();
                ServidorArrancado = false;
            }

            while(Coms.ETH.ON != Eth_On){
                SendToPSOC5(Eth_On,COMS_CONFIGURATION_ETH_ON);
                delay(50);
            }

            uint8_t IPBuffer[12];

            IPBuffer[0]  = ip4_addr1(&Coms.ETH.IP);
            IPBuffer[1]  = ip4_addr2(&Coms.ETH.IP);
            IPBuffer[2]  = ip4_addr3(&Coms.ETH.IP);
            IPBuffer[3]  = ip4_addr4(&Coms.ETH.IP);

            IPBuffer[4]  = ip4_addr1(&Coms.ETH.Gateway);
            IPBuffer[5]  = ip4_addr2(&Coms.ETH.Gateway);
            IPBuffer[6]  = ip4_addr3(&Coms.ETH.Gateway);
            IPBuffer[7]  = ip4_addr4(&Coms.ETH.Gateway);

            IPBuffer[8]  = ip4_addr1(&Coms.ETH.Mask);
            IPBuffer[9]  = ip4_addr2(&Coms.ETH.Mask);
            IPBuffer[10] = ip4_addr3(&Coms.ETH.Mask);
            IPBuffer[11] = ip4_addr4(&Coms.ETH.Mask);

            SendToPSOC5((char*)IPBuffer,12,COMS_CONFIGURATION_LAN_IP);
            delay(100);
            SendToPSOC5(Coms.ETH.Auto,COMS_CONFIGURATION_ETH_AUTO);

            //Configuracion del wifi            

            char W_AP[32];
            char Pass[64];

            memcpy(W_AP,request->getParam(AP)->value().c_str(),request->getParam(AP)->value().length());
            memcpy(Pass,request->getParam(WIFI_PWD)->value().c_str(),request->getParam(WIFI_PWD)->value().length());
            
            if(Wifi_On && (memcmp(Coms.Wifi.AP, W_AP, strlen(W_AP)) || memcmp(Coms.Wifi.Pass, Pass, strlen(Pass)))){
                memcpy(Coms.Wifi.AP, W_AP, strlen(W_AP));
                memcpy(Coms.Wifi.Pass, Pass, strlen(Pass));

                Coms.Wifi.Pass[strlen(Pass)]='\0';
                Coms.Wifi.SetFromInternal = true;
                if(Coms.Wifi.ON){
                    Coms.Wifi.restart = true;
                }
            }
            while(Coms.Wifi.ON != Wifi_On){
                SendToPSOC5(Wifi_On,COMS_CONFIGURATION_WIFI_ON);
                delay(50);
            }

            if(!reiniciar_eth)request->send(SPIFFS, "/comms.html",String(), false, processor);
        }else{
            request->send(SPIFFS, "/login.html");
        }

    });
/*
    server.on("/usertype", HTTP_GET_A, [](AsyncWebServerRequest *request){
        
        uint8_t data = request->getParam(USER_TYPE)->value().toInt();
        Comands.user_type= data;
        SendToPSOC5(data, VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE);
        
        request->send(SPIFFS, "/comands.html",String(), false, processor);
        
    });
*/
    server.on("/currcomand", HTTP_GET_A, [](AsyncWebServerRequest *request){
        if(Autenticado==true){
            uint8_t data = request->getParam(CURR_COMAND)->value().toInt();

            SendToPSOC5(data, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
            
            request->send(SPIFFS, "/comands.html",String(), false, processor);
        }else{
            request->send(SPIFFS, "/login.html");
        }
    });

    server.on("/instcurlim", HTTP_GET_A, [](AsyncWebServerRequest *request){
        if(Autenticado==true){
            uint8_t data = request->getParam(INST_CURR_LIM)->value().toInt();

            SendToPSOC5(data, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);
            
            request->send(SPIFFS, "/parameters.html",String(), false, processor);
        }else{
            request->send(SPIFFS, "/login.html");
        }
    });

    server.on("/powercont", HTTP_GET_A, [](AsyncWebServerRequest *request){
         if(Autenticado==true){
            uint8_t data = request->getParam(POT_CONT)->value().toInt();

            SendToPSOC5(data, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE);
            
            request->send(SPIFFS, "/parameters.html",String(), false, processor);
         }else{
            request->send(SPIFFS, "/login.html");
         }
    });
/*
    server.on("/usertype", HTTP_GET_A, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(Comands.user_type).c_str());
    });
*/
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
        request->send_P(200, "text/plain", String(Status.Measures.instant_voltage/10).c_str());
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
    if(Autenticado==true){ 
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
            Serial.printf("Update firmware order received");
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
            SendToPSOC5(Params.CDP,DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
        break;
        case 9:
            Params.CDP = (Params.CDP | 2) & 15;
            SendToPSOC5(Params.CDP,DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
        break;
        case 10:
            Params.CDP = (Params.CDP | 2) | 16;
            SendToPSOC5(Params.CDP,DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
        break;
        case 16:
            Params.CDP = (Params.CDP | 8) & 27;
            SendToPSOC5(Params.CDP,DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
        break;
        case 18:
            Params.CDP = (Params.CDP | 4) & 23;
            SendToPSOC5(Params.CDP,DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
        break;
        case 19:
            Autenticado=false;
        break;

        default:

            break;
        }
    }else{
        request->send(SPIFFS, "/login.html");
    }
   });

   server.on("/autoupdate", HTTP_GET_A, [] (AsyncWebServerRequest *request) {
    if(Autenticado==true){
    	bool estado;
        int numero;  
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
    }else{
        request->send(SPIFFS, "/login.html");
    }
   });

#ifdef DEBUG_WIFI
   Serial.println(ESP.getFreeHeap());
#endif
}

#endif