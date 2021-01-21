#include "Wifi_Station.h"

NVS cfg;
char password[32] = {0};
char ssid[32] = {0};

extern carac_Firebase_Configuration ConfigFirebase;

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

    cfg.close();
    return 1;
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){    
	switch (event) {
		case SYSTEM_EVENT_STA_DISCONNECTED:
            if(getfirebaseClientStatus())stopFirebaseClient();
            
            if(info.wifi_sta_disconnected.reason!=WIFI_REASON_ASSOC_LEAVE){
                Serial.println("Reconectando...");
			    WiFi.reconnect();
            }
		break;

		case SYSTEM_EVENT_STA_GOT_IP:
			Serial.print("Connected with IP: ");
			Serial.println(WiFi.localIP());
            ConfigFirebase.InternetConection=1;
            vTaskDelay(pdMS_TO_TICKS(5000));
            initFirebaseClient();
            Serial.println("FREE HEAP MEMORY [after FIREBASE_INIT] **************************");
	        Serial.println(ESP.getFreeHeap());
		break;
		default:
		break;
	}
}

void Station_Begin(){
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