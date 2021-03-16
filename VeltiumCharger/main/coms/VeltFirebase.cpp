#include "VeltFirebase.h"
#include <string>
#include <iostream>
bool FinishReading = false;
int Readed = 0;
char *TotalResponse = new char[1500];

esp_err_t _http_event_handle(esp_http_client_event_t *evt){
    char *Respuesta = new char[512];
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            FinishReading = false;
            Readed = 0;
            TotalResponse = new char[1500];
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            if(evt->data_len>0){
                esp_http_client_read(evt->client, Respuesta, evt->data_len);
                memcpy(&TotalResponse[Readed],Respuesta,evt->data_len);
                Readed+=evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            FinishReading = true;
            break;
        case HTTP_EVENT_DISCONNECTED:
            break;
    }
    return ESP_OK;
}


/*********** Clase autenticacion ************/
void Real_Time_Database::beginAuth (void) {
    char URL[1500] ={"0"} ;
    memcpy(URL,Auth_url.c_str(),Auth_url.length());
    
    #ifdef DEVELOPMENT
        strcat(URL,FIREBASE_DEV_API_KEY);
    #else
        strcat(URL,FIREBASE_API_KEY);
    #endif
        esp_http_client_config_t config = {
        .url = URL,
        .timeout_ms = 1000,
        .event_handler = _http_event_handle,
    };
    Auth_client = esp_http_client_init(&config);
    esp_http_client_set_header(Auth_client,"Content-Type", "application/json");
}

bool Real_Time_Database::LogIn(void){
    beginAuth();
    uint8 Timeout = 0;
    String AuthPostData;
    DynamicJsonDocument Response(4096);

    AuthDoc["email"] = "charger+VCD"+deviceID+"@veltium.com";
    String ID  = deviceID;
    for(int i=0;i<6;i++){
        ID[i] = ID[i+2];
    }
    ID[6]='J';
    ID[7]='M';

    AuthDoc["password"]  = ID;
    AuthDoc["returnSecureToken"] = true;

    serializeJson(AuthDoc, AuthPostData);

    esp_http_client_set_method(Auth_client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(Auth_client, AuthPostData.c_str(), AuthPostData.length());
    if (esp_http_client_perform(Auth_client) != ESP_OK) {
        return false;
    }
    while(!FinishReading && ++Timeout <30){
        delay(50);
    }

    if(Timeout >= 30 ){
        return false;
    }
    
    std::string s(TotalResponse, TotalResponse+Readed);

    deserializeJson(Response, s);

    idToken = Response["idToken"].as<String>();
    localId = Response["localId"].as<String>();

    endAuth();
    return true;
}

void Real_Time_Database::endAuth(void){
    esp_http_client_cleanup(Auth_client);
}

bool Real_Time_Database::checkPermisions(void){
    char Respuesta[1024]= {"0"};
    uint8 Timeout = 0;
    DynamicJsonDocument Response(1024);

    esp_http_client_set_url(RTDB_client, String(Base_Path+"/prod/users/"+localId+"/perms.json?auth="+idToken).c_str());
    esp_http_client_set_method(RTDB_client, HTTP_METHOD_GET);

    if (esp_http_client_perform(RTDB_client) != ESP_OK) {
        return false;
    }

    while(!FinishReading && ++Timeout <30){
        delay(50);
    }

    if(Timeout >= 30){
        return false;
    }
    
    std::string s(TotalResponse, TotalResponse+Readed);

    deserializeJson(Response, s);
    FinishReading = false;
    Readed = 0;

    return Response["fw_beta"];
}

/*********** Clase RTDB ************/

void Real_Time_Database::begin(String Host, String DatabaseID){
    RTDB_url="https://";
    RTDB_url+=Host+("prod/devices/"+DatabaseID);
    Base_Path="https://";
    Base_Path+=Host;

    Write_url = RTDB_url+"/status/ts_app_req.json?auth="+idToken+"&timeout=2500ms";

    esp_http_client_config_t config = {
        .url = Write_url.c_str(),
        .timeout_ms = 2000,
        .event_handler = _http_event_handle,
        .buffer_size_tx = 2048,
    };

    RTDB_client = esp_http_client_init(&config);
    esp_http_client_set_header(RTDB_client,"Content-Type", "application/json");
}

void Real_Time_Database::end(){
    esp_http_client_cleanup(RTDB_client);

}
void Real_Time_Database::reload (){
    
    Write_url = RTDB_url+"/status/ts_app_req.json?auth="+idToken+"&timeout=2500ms";

    esp_http_client_config_t config = {
        .url = Write_url.c_str(),
        .timeout_ms = 2000,
        .event_handler = _http_event_handle,
        .buffer_size_tx = 2048,
    };
    end();
    RTDB_client = esp_http_client_init(&config);
    esp_http_client_set_header(RTDB_client,"Content-Type", "application/json");

}

bool Real_Time_Database::Send_Command(String path, JsonDocument *doc, uint8_t Command){   
    String SerializedData;
    uint8 Timeout = 0;
    Write_url = RTDB_url+path+".json?auth="+idToken;

    if(Command < 3){      
        Write_url += +"&timeout=2500ms";           
        serializeJson(*doc, SerializedData);
    }
    
    esp_http_client_set_url(RTDB_client, Write_url.c_str());

    switch(Command){
        case WRITE:        
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_POST);
            esp_http_client_set_post_field(RTDB_client, SerializedData.c_str(), SerializedData.length());
            break;
        case UPDATE:
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_PATCH);
            esp_http_client_set_post_field(RTDB_client, SerializedData.c_str(), SerializedData.length());
            break;
        case TIMESTAMP:     
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_PUT);
            esp_http_client_set_post_field(RTDB_client, "{\".sv\": \"timestamp\"}", strlen("{\".sv\": \"timestamp\"}"));
            break;
        case READ:
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_GET);
            break;
        case READ_FW:
            esp_http_client_set_url(RTDB_client, String(Base_Path+path+".json?auth="+idToken).c_str());
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_GET);
            break;

        default:
            Serial.print("Accion no implementada!");
            return false;
            break;
    }
    int err = esp_http_client_perform(RTDB_client);
    if (err != ESP_OK ) {
        Serial.printf("HTTP request failed: %s\n", esp_err_to_name(err));
        return false;
    }

    doc->clear();

    if(Command < 5){
        FinishReading = false;
        Readed = 0;
        return true;
    }

    while(!FinishReading && ++Timeout <30 ){
        delay(50);
    }

    if(Timeout >= 30){
        return false;
    }
    
    
    std::string s(TotalResponse, TotalResponse+Readed);

    deserializeJson(*doc, s);
    FinishReading = false;
    Readed = 0;

    return true;
}

long long  Real_Time_Database::Get_Timestamp(String path, JsonDocument *respuesta){
    String SerializedData;
    respuesta->clear();
    uint8 Timeout = 0;
    Write_url = RTDB_url+path+".json?auth="+idToken+"&timeout=2000ms";

    esp_http_client_set_url(RTDB_client, Write_url.c_str());
    esp_http_client_set_method(RTDB_client, HTTP_METHOD_GET);
    int err = esp_http_client_perform(RTDB_client);
    if (err != ESP_OK ) {
        Serial.printf("HTTP request failed: %s\n", esp_err_to_name(err));
        FinishReading = false;
        Readed = 0;
        if(err == -1){
            return -1;
        } 
        reload();
        return 1;
    }

    while(!FinishReading && ++Timeout <30 ){
        delay(50);
    }

    if(Timeout >= 30){
        return false;
    }
    
    
    std::string s(TotalResponse, TotalResponse+Readed);

    FinishReading = false;
    Readed = 0;

    long long data1 = atoll(s.c_str());
    if(data1<1){
        respuesta->clear();
        deserializeJson(*respuesta,s);
    }

    return data1;
}