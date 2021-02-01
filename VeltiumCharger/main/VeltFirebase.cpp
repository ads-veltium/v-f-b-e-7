#include "VeltFirebase.h"

/*********** Clase autenticacion ************/
void Autenticacion::begin (void) {
  AutenticationClient.setTimeout(1000);
  AutenticationClient.begin(Auth_url+FIREBASE_API_KEY);  
  AutenticationClient.addHeader("Content-Type", "application/json");
}

bool Autenticacion::LogIn(void){

    String AuthPostData;
    DynamicJsonDocument Response(4096);

    doc["email"] = email;
    doc["password"]  = pass;
    doc["returnSecureToken"] = true;

    serializeJson(doc, AuthPostData);
    
    int status = AutenticationClient.POST(AuthPostData);

    if (status <= 0) {
        Serial.printf("HTTP error: %s\n", 
        AutenticationClient.errorToString(status).c_str());
        return false;
    }

    // Read the response.
    deserializeJson(Response, AutenticationClient.getString());

    idToken = Response["idToken"].as<String>();
    localId = Response["localId"].as<String>();

    
    return true;
}

/*********** Clase RTDB ************/
void Real_Time_Database::begin(String Host){
    RTDB_url="https://";
    RTDB_url+=Host+"/";
    RTDBClient.setTimeout(500);
}

bool Real_Time_Database::Send_Command(String path, JsonDocument *doc, uint8_t Command, String auth){
    String SerializedData;
    int response;

    Write_url = RTDB_url+path+".json?auth="+auth;

    RTDBClient.begin(Write_url);

    if(Command < 3){      
        RTDBClient.addHeader("Content-Type", "application/json");
        serializeJson(*doc, SerializedData);
    }

    switch(Command){
        case WRITE:        
            response = RTDBClient.PUT(SerializedData);
            break;
        case UPDATE:
            response = RTDBClient.PATCH(SerializedData);
            break;
        case TIMESTAMP:     
            response = RTDBClient.PUT("{\".sv\": \"timestamp\"}");
            break;
        case READ:
            response = RTDBClient.GET();
            break;

        default:
            Serial.print("Accion no implementada!");
            return false;
            break;
    }

    if (response <= 0) {
        Serial.printf("HTTP error: %s\n", 
        RTDBClient.errorToString(response).c_str());
        return false;
    }

    doc->clear();
    deserializeJson(*doc, RTDBClient.getString());

    RTDBClient.end();
    return true;
}