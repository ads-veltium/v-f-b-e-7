#ifndef VeltFirebase_h
#define VeltFirebase_h

#define ARDUINOJSON_USE_LONG_LONG 1

#include "ArduinoJson.h"
#include "../control.h"
#include "esp32-hal-psram.h"
#include "esp_http_client.h"

#define FIREBASE_API_KEY "AIzaSyBaJA88_Y3ViCzNF_J08f4LBMAM771aZLs"
#define FIREBASE_PROJECT "veltiumbackend.firebaseio.com"
#define FIREBASE_DEV_API_KEY "AIzaSyCYZpVNUOQvrXvc3qETxCqX4DPfp3Fwe3w"
#define FIREBASE_DEV_PROJECT "veltiumdev-default-rtdb.firebaseio.com"

bool initFirebaseClient();
void GetUpdateFile(String URL);
void Firebase_Conn_Task(void *args);
uint8_t getfirebaseClientStatus();
uint16  ParseFirmwareVersion(String Texto);


class Real_Time_Database{
    String RTDB_url, Read_url, GROUPS_url,Write_url, Base_Path;
    esp_http_client_handle_t RTDB_client, *Auth_client; 
    
    
    String Auth_url= "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";
    StaticJsonDocument<256> AuthDoc;

  public:
    #define ESCRIBIR  0
    #define UPDATE    1
    #define TIMESTAMP 4
    #define LEER      5
    #define READ_FW   6

    String deviceID;
    String idToken;
    String localId;
    uint16_t expiration;

    //Auth functions
    void beginAuth (void);
    void endAuth (void);
    bool LogIn(void);
    bool checkPermisions(void);

    //Database functions
    bool Send_Command(String path, JsonDocument *doc, uint8_t Command, bool groups=false);
    long long  Get_Timestamp(String path,JsonDocument *response,bool groups = false);
    void begin(String Host, String DatabaseID);
    void reload();
    void end();
    Real_Time_Database(){}

};

class Cliente_HTTP{
    String _url;
    esp_http_client_handle_t _client; 
    String _response;
    
    int  _timeout;
    
  public:

    #define ESCRIBIR  0
    #define UPDATE    1
    #define TIMESTAMP 4
    #define LEER      5
    #define READ_FW   6
    
    
    //Functions
    bool Send_Command(String url,  uint8_t Command);
    void begin();
    void end();
    String ObtenerRespuesta();
    void set_url(String url);


    //Constructor
    Cliente_HTTP(String url, int Timeout){
      _url=url;
      _timeout = Timeout;
    }

};

#endif