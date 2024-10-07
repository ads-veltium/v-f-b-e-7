#include "VeltFirebase.h"
#ifdef CONNECTED
#include <string>
#include <iostream>


bool FinishReading = false;
int  Readed = 0;
char *TotalResponse = new char[1500];
extern carac_Firebase_Configuration ConfigFirebase;

extern carac_Coms  Coms;

static const char* TAG = "VeltFirebase";

#ifndef DEVELOPMENT
String FIREBASE_API_KEY = FIREBASE_PROD_API_KEY;
#else
#ifdef DEMO_MODE_BACKEND
String FIREBASE_API_KEY = FIREBASE_PROD_API_KEY;
#else
String FIREBASE_API_KEY = FIREBASE_DEV_API_KEY;
#endif
#endif

esp_err_t _http_event_handle(esp_http_client_event_t *evt){

    char *Respuesta = new char[512];
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            FinishReading = false;
            Readed = 0;
            free(TotalResponse);
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
            //printf("END");
            //esp_http_client_cleanup(evt->client);
            break;
    }
    free(Respuesta);
    return ESP_OK;
}


/*********** Autenticacion ************/

void Real_Time_Database::configAuth(String ConfigHost) {
    String Auth_url = "https://" + ConfigHost + "/v1/accounts:signInWithPassword?key=" + FIREBASE_API_KEY;
    esp_http_client_config_t config = {
        .url = Auth_url.c_str(),
        .timeout_ms = 5000,
        .event_handler = _http_event_handle,
    };
    Auth_client = esp_http_client_init(&config);
    esp_http_client_set_header(Auth_client, "Content-Type", "application/json");
    esp_http_client_set_header(Auth_client, "Connection", "close");
}

bool Real_Time_Database::LogIn(String Host) {
    configAuth(Host);

    String email = "charger+VCD" + deviceID + "@veltium.com";
    String password = deviceID.substring(2, 8) + "JM";
    AuthDoc["email"] = email;
    AuthDoc["password"] = password;
    AuthDoc["returnSecureToken"] = true;

    String AuthPostData;
    serializeJson(AuthDoc, AuthPostData);

    esp_http_client_set_method(Auth_client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(Auth_client, AuthPostData.c_str(), AuthPostData.length());
    int err = esp_http_client_perform(Auth_client);
    
    if (err != ESP_OK) {
        if (err == ESP_ERR_HTTP_CONNECT) {
            SetDNS();
            endAuth();
        }
        return false;
    }

   for (uint8_t Timeout = 0; !FinishReading && Timeout <= 30; Timeout++){
        delay(50);
    }

    DynamicJsonDocument Response(4096);
    DeserializationError jsonErr = deserializeJson(Response, TotalResponse);
    if (jsonErr){
        ESP_LOGE(TAG,"LogIn: Error al parsear la respuesta JSON");
        return false;
    }

    idToken = Response["idToken"].as<String>();
    localId = Response["localId"].as<String>();
    refreshToken = Response["refreshToken"].as<String>();
    expiration = atoi(Response["expiresIn"].as<String>().c_str());
    ESP_LOGD(TAG,"idToken= %s",idToken.c_str());
    ESP_LOGD(TAG,"localId= %s",localId.c_str());
    ESP_LOGD(TAG,"refreshToken= %s",refreshToken.c_str());
    ESP_LOGD(TAG,"expiration of idToken= %d",expiration);

    if (idToken != NULL){
        //endAuth();
        return true;
    }
    else{
        return false;
    }
}

void Real_Time_Database::endAuth(void){
    esp_http_client_cleanup(Auth_client);
    Auth_client = NULL;
}

bool Real_Time_Database::checkPermisions(void){

    uint8 Timeout = 0;
    DynamicJsonDocument Response(1024);

    esp_http_client_set_url(RTDB_client, String(Base_Path+"/prod/users/"+localId+"/perms.json?auth="+idToken).c_str());
    esp_http_client_set_method(RTDB_client, HTTP_METHOD_GET);
    int err = esp_http_client_perform(RTDB_client);

    if (err != ESP_OK) {
        return false;
    }

    while(!FinishReading ){
        delay(50);
        Timeout ++;
        if(Timeout > 30){
            return false;
        }
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

/*********** Clase RTDB ***********************/

bool Real_Time_Database::begin(String Host, String DatabaseID){
    Base_Path = "https://" + Host;
    RTDB_url = Base_Path + "/prod/devices/" + DatabaseID;
    GROUPS_url = Base_Path + "/groups/";

    Write_url = RTDB_url + "/status/ts_app_req.json?auth=" + idToken + "&timeout=2500ms";

    esp_http_client_config_t config = {
        .url = Write_url.c_str(),
        .timeout_ms = 5000,
        .event_handler = _http_event_handle,
        .buffer_size_tx = 2048,
    };

    RTDB_client = esp_http_client_init(&config);
    if(RTDB_client != NULL){
        esp_http_client_set_header(RTDB_client,"Content-Type", "application/json");
        return true;
    }
    else {
        return false;
    }
}

void Real_Time_Database::end(){
    esp_http_client_cleanup(RTDB_client);
    RTDB_client = NULL;
}

void Real_Time_Database::reload (){
    ESP_LOGI(TAG, "Real_Time_Database reload");
    end();
    Write_url = RTDB_url+"/status/ts_app_req.json?auth="+idToken+"&timeout=2500ms";
    esp_http_client_config_t config = {
        .url = Write_url.c_str(),
        .timeout_ms = 5000,
        .event_handler = _http_event_handle,
        .buffer_size_tx = 2048,
    };
    RTDB_client = esp_http_client_init(&config);
    esp_http_client_set_header(RTDB_client,"Content-Type", "application/json");
}

bool Real_Time_Database::Send_Command(String path, JsonDocument *doc, uint8_t Command, bool groups){   
    String SerializedData;
    uint8 Timeout = 0;
    Write_url = "";
    Write_url = RTDB_url+path+".json?auth="+idToken;
    if(groups){
        Write_url = GROUPS_url+path+".json?auth="+idToken;
    }

    if(Command < 3){      
        Write_url += "&timeout=2500ms";           
        serializeJson(*doc, SerializedData);
    }
    
    esp_http_client_set_url(RTDB_client, Write_url.c_str());

    switch(Command){
        case FB_WRITE:        
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_POST);
            esp_http_client_set_post_field(RTDB_client, SerializedData.c_str(), SerializedData.length());
            break;
        case FB_UPDATE:
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_PATCH);
            esp_http_client_set_post_field(RTDB_client, SerializedData.c_str(), SerializedData.length());
            break;
        case FB_TIMESTAMP:     
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_PUT);
            esp_http_client_set_post_field(RTDB_client, "{\".sv\": \"timestamp\"}", strlen("{\".sv\": \"timestamp\"}"));
            break;
        case FB_READ:
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_GET);
            break;
        case FB_READ_ROOT:
            Write_url = Base_Path+path+".json?auth="+idToken;
            ESP_LOGD(TAG,"FB_READ_ROOT URL=%s",Write_url.c_str());
            esp_http_client_set_url(RTDB_client, Write_url.c_str());
            esp_http_client_set_method(RTDB_client, HTTP_METHOD_GET);    
            break;

        default:
            Serial.print("Accion no implementada!");
            return false;
            break;
    }
    int err = esp_http_client_perform(RTDB_client);
    if (err != ESP_OK ) {
        ESP_LOGE(TAG,"Send_Command: HTTP request failed: %s\n", esp_err_to_name(err));
        if (err == ESP_ERR_HTTP_CONNECT) {
            SetDNS();
        }
        reload();
        return false;
    }

    doc->clear();

    if(Command < 5){
        FinishReading = false;
        Readed = 0;
        return true;
    }

    while(!FinishReading ){
        delay(50);
        Timeout ++;
        if(Timeout > 30){
            return false;
        }
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

long long  Real_Time_Database::Get_Timestamp(String path, JsonDocument *respuesta, bool groups){
    String SerializedData;
    respuesta->clear();
    Write_url = " ";
    uint8 Timeout = 0;
    if(groups){
        Write_url = GROUPS_url+path+".json?auth="+idToken+"&timeout=2000ms";
    }
    else{
        Write_url = RTDB_url+path+".json?auth="+idToken+"&timeout=2000ms";
        ESP_LOGD(TAG,"Get_Timestamp:Write_url (%i): %s",Write_url.length(),Write_url.c_str());
    }
    ESP_ERROR_CHECK(esp_http_client_set_url(RTDB_client, Write_url.c_str()));
    ESP_ERROR_CHECK(esp_http_client_set_method(RTDB_client, HTTP_METHOD_GET));
    int err = esp_http_client_perform(RTDB_client);
   if (err != ESP_OK) {
        ESP_LOGE(TAG,"Get_Timestamp: esp_http_client_perform fail with err: %s", esp_err_to_name(err));
        if (err == ESP_ERR_HTTP_CONNECT) {
            SetDNS();
            reload();
            FinishReading = false;
            Readed = 0;
            ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
            ConfigFirebase.FirebaseConnState = DISCONNECTING;
            ESP_LOGI(TAG, "Get_Timestamp: Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
        }
        return -1;
    }

    while(!FinishReading ){
        delay(50);
        Timeout ++;
        if(Timeout > 30){
            return false;
        }
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

#endif