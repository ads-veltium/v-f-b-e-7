#ifndef VeltFirebase_h
#define VeltFirebase_h

#include "HTTPClient.h"
#include "ArduinoJson.h"
#include "Update.h"
#include "../control.h"
#include "esp32-hal-psram.h"

#define FIREBASE_API_KEY "AIzaSyCYZpVNUOQvrXvc3qETxCqX4DPfp3Fwe3w"
#define FIREBASE_PROJECT "veltiumdev-default-rtdb.firebaseio.com"

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
#define WRITTING_TIMES   39
#define DISCONNECTING    45
#define DOWNLOADING      55
#define UPDATING         65
#define INSTALLING       70

bool initFirebaseClient();
void GetUpdateFile(String URL);
void Firebase_Conn_Task(void *args);
uint8_t getfirebaseClientStatus();
uint16  ParseFirmwareVersion(String Texto);


class Real_Time_Database{
    String RTDB_url, Read_url, Write_url, Base_Path;
    HTTPClient RTDBClient, AutenticationClient; 

    String Auth_url= "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";
    StaticJsonDocument<256> AuthDoc;
  public:
    #define WRITE     0
    #define UPDATE    1
    #define TIMESTAMP 4
    #define READ      5
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
    bool Send_Command(String path, JsonDocument *doc, uint8_t Command);
    long long  Get_Timestamp(String path,JsonDocument *response);
    void begin(String Host, String DatabaseID);
    void restart();
    void end();
};

class Firebase{
    
  public:
    Real_Time_Database RTDB;


};




#endif