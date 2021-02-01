#include "Wifi_Station.h"


/*********************** Globals ************************/
NVS cfg;
char password[32] = {0};
char ssid[32] = {0};

const char* NUM_BOTON = "output";
const char* ACTUALIZACIONES = "state";
const char* USER = "userid";
const char* PASSWORD = "pwd";

String usuario;
String contrasena;

AsyncWebServer server(80);

extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Update_Status          UpdateStatus;
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Params                 Params;
extern carac_Coms                   Coms;

uint8_t Store_cfg(){
    cfg.init("nvs", "wifi",0); 
    if(!cfg.create("wifi_sta_ssid", "VELTIUM_WF")){
        cfg.write("wifi_sta_ssid", "VELTIUM_WF");
    } 
    if(!cfg.create("wifi_sta_pass", "W1f1d3V3lt1um$m4rtCh4rg3r$!")){
        cfg.write("wifi_sta_pass", "W1f1d3V3lt1um$m4rtCh4rg3r$!");
    } 
    cfg.close();

    return 1;
}

uint8_t Read_cfg(){
    cfg.init("nvs", "wifi",0);

    cfg.read("wifi_sta_ssid", ssid, 15); 
    cfg.read("wifi_sta_pass", password, 32); 

    Coms.Wifi.AP=ssid;
    Coms.Wifi.Pass=password;

    cfg.close();
    return 1;
}

/*************** Wifi and ETH event handlers *****************/
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){    
	switch (event) {
		case SYSTEM_EVENT_STA_DISCONNECTED:
            ConfigFirebase.InternetConection=0;           
            if(info.disconnected.reason!=WIFI_REASON_ASSOC_LEAVE){
                Serial.println("Reconectando...");
			    WiFi.reconnect();
            }
		break;

		case SYSTEM_EVENT_STA_GOT_IP:
			Serial.print("Connected with IP: ");
			Serial.println(WiFi.localIP());
            ConfigFirebase.InternetConection=1;
		break;
		default:
		break;
	}
}

#ifdef USE_ETH
void ETHEvent(WiFiEvent_t event, WiFiEventInfo_t info){
    switch (event) {
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
        SPIFFS.end();
        ConfigFirebase.InternetConection=1; 
        break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
        eth_connected = false;
        ConfigFirebase.InternetConection=0; 
        break;
    case SYSTEM_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        eth_connected = false;
        break;
    default:
      break;
  }
}


/************* Internal server configuration ****************/
void handle_NotFound(){
  //request->send(404);
  //CheckForUpdate();
}

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
		Comands.start = 1;
		Comands.stop = 0;
		Serial.println("Has pulsado el boton start");
	}
	
	if (entrada=="2")
	{
		Comands.stop = 1;
		Comands.start = 0;
		Serial.println("Has pulsado el boton stop");
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
    WiFi.onEvent(ETHEvent);
	ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE);
	SPIFFS.begin(0,"/spiffs",1,"WebServer");
}
#endif

#ifdef USE_WIFI
void Station_Begin(){
    Coms.Wifi.ON=true;

    Store_cfg();
    Read_cfg();

	Serial.println(ssid);
    WiFi.begin(ssid, password);
    WiFi.onEvent(WiFiEvent);
    Serial.println("Connecting to Wi-Fi...");

} 

void Station_Pause(){

    Serial.println(ESP.getFreeHeap());
    WiFi.disconnect(1, 1);
    Serial.println("Wifi Paused");
    Serial.println(ESP.getFreeHeap());

} 

void Station_Resume(){
    WiFi.begin();
    Serial.println("Wifi Resumed");
    Serial.println(WiFi.localIP());

}

void Station_Scan(){
    Serial.println("** Scan Networks **");
    //uint16 numSsid = WiFi.scanNetworks();

    Serial.print("Number of available WiFi networks discovered:");
    //Serial.println(numSsid);
}
#endif