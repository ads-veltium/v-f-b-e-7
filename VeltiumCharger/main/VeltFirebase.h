#ifndef VeltFirebase_h
#define VeltFirebase_h

#include "HTTPClient.h"
#include <ArduinoJson.h>
#include "Update.h"
#include "control.h"
#include "esp32-hal-psram.h"

#define FIREBASE_API_KEY "AIzaSyCYZpVNUOQvrXvc3qETxCqX4DPfp3Fwe3w"
#define FIREBASE_PROJECT "veltiumdev-default-rtdb.firebaseio.com"

//const char* STORAGE_BUCKET_ID               = "veltiumdev.appspot.com";

#define DISCONNECTED      0
#define CONNECTING        5
#define CONECTADO        10
#define IDLE             20
#define READING_CONTROL  25
#define READING_PARAMS   26
#define READING_COMS     27
#define WRITTING_CONTROL 35
#define WRITTING_STATUS  36
#define WRITTING_PARAMS  37
#define WRITTING_COMS    38
#define DISCONNECTING    45
#define DOWNLOADING      55
#define UPDATING         65
#define INSTALLING       70

bool initFirebaseClient();
void GetUpdateFile(String URL);
void Firebase_Conn_Task(void *args);
uint8_t getfirebaseClientStatus();
uint16  ParseFirmwareVersion(String Texto);

class Autenticacion { 
    HTTPClient AutenticationClient; 
    String Auth_url= "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";
    StaticJsonDocument<256> doc;
  public:
    String email, pass;
    String idToken;
    String localId;
    uint16_t expiration;

    void begin (void);
    void end (void);

    bool LogIn(void);
};

class Real_Time_Database{
    HTTPClient RTDBClient; 
    String RTDB_url, Read_url, Write_url;
  public:
    #define WRITE     0
    #define UPDATE    1
    #define TIMESTAMP 4
    #define READ      5

    bool Send_Command(String path, JsonDocument *doc, uint8_t Command, String auth);
    void begin(String Host);
};

class Firebase{
    
  public:
    Autenticacion Auth;
    Real_Time_Database RTDB;


};




#endif