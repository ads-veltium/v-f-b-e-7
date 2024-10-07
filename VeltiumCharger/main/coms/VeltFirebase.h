#ifndef VeltFirebase_h
#define VeltFirebase_h

#define ARDUINOJSON_USE_LONG_LONG 1

#include "ArduinoJson.h"
#include "../control.h"
#include "esp32-hal-psram.h"
#include "esp_http_client.h"


void Firebase_Conn_Task(void *args);
uint16 ParseFirmwareVersion(String Texto);
void SetDNS();

class Real_Time_Database{
    /*String RTDB_url; 
    String Read_url; 
    String GROUPS_url; 
    String Write_url;
    String Base_Path;
    String Auth_url;
    esp_http_client_handle_t RTDB_client;
    esp_http_client_handle_t Auth_client;
    StaticJsonDocument<256> AuthDoc;*/

  public:
    String RTDB_url; 
    String Read_url; 
    String GROUPS_url; 
    String Write_url;
    String Base_Path;
    String Auth_url;
    esp_http_client_handle_t RTDB_client;
    esp_http_client_handle_t Auth_client;
    StaticJsonDocument<256> AuthDoc;

    #define FB_WRITE     0
    #define FB_UPDATE    1
    #define FB_TIMESTAMP 4
    #define FB_READ      5
    #define FB_READ_ROOT 6

    String deviceID;
    String idToken;
    String localId;
    uint16_t expiration;
    String refreshToken;


    //Auth functions
    void configAuth (String ConfigHost);
    void endAuth (void);
    bool LogIn(String Host);
    bool checkPermisions(void);

    //Database functions
    bool Send_Command(String path, JsonDocument *doc, uint8_t Command, bool groups=false);
    long long  Get_Timestamp(String path,JsonDocument *response,bool groups = false);
    bool begin(String Host, String DatabaseID);
    void reload();
    void end();
    Real_Time_Database(){}
};

#endif