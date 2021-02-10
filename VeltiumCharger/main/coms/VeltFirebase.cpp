#include "VeltFirebase.h"

/*********** Clase autenticacion ************/
void Real_Time_Database::beginAuth (void) {
  AutenticationClient.setTimeout(1000);
  AutenticationClient.begin(Auth_url+FIREBASE_API_KEY);
  AutenticationClient.addHeader("Content-Type", "application/json");
}

bool Real_Time_Database::LogIn(void){
    beginAuth();
    String AuthPostData;
    DynamicJsonDocument Response(4096);

    AuthDoc["email"] = email;
    AuthDoc["password"]  = pass;
    AuthDoc["returnSecureToken"] = true;

    serializeJson(AuthDoc, AuthPostData);

    int status = AutenticationClient.POST(AuthPostData);
    
    if (status != 200) {
        Serial.println(status);
        Serial.printf("HTTP error: %s\n", 
        AutenticationClient.errorToString(status).c_str());
        return false;
    }

    // Read the response.
    deserializeJson(Response, AutenticationClient.getString());

    idToken = Response["idToken"].as<String>();
    localId = Response["localId"].as<String>();

    endAuth();
    return true;
}

void Real_Time_Database::endAuth(void){
    AutenticationClient.end();
}

bool Real_Time_Database::checkPermisions(void){

    DynamicJsonDocument Response(1024);
    //Get User permisions.
    RTDBClient.setURL(Base_Path+"/prod/users/"+localId+"/perms.json?auth="+idToken);
    int response = RTDBClient.GET(); 
    if (response < -0) {
        Serial.println(response);
        Serial.printf("HTTP error: %s\n", 
        RTDBClient.errorToString(response).c_str());
    }

    deserializeJson(Response, RTDBClient.getString());   

    return Response["fw_beta"];
}

/*********** Clase RTDB ************/
void Real_Time_Database::begin(String Host, String DatabaseID){
    RTDB_url="https://";
    RTDB_url+=Host+("prod/devices/"+DatabaseID);
    Base_Path="https://";
    Base_Path+=Host;

    Write_url = RTDB_url+"/status/ts_app_req.json?auth="+idToken+"&timeout=2500ms";

    RTDBClient.begin(Write_url);
    RTDBClient.addHeader("Content-Type", "application/json"); 
    RTDBClient.setTimeout(2000);
    RTDBClient.setConnectTimeout(2000);
    RTDBClient.setReuse(true);
}

void Real_Time_Database::end(){
    RTDBClient.end();
}

void Real_Time_Database::restart(){
    Write_url = RTDB_url+"/status/ts_app_req.json?auth="+idToken+"&timeout=2500ms";

    RTDBClient.begin(Write_url);
    RTDBClient.setTimeout(2000);
    RTDBClient.setConnectTimeout(2000);
    RTDBClient.setReuse(true);
    RTDBClient.addHeader("Content-Type", "application/json"); 
}

bool Real_Time_Database::Send_Command(String path, JsonDocument *doc, uint8_t Command){   
    String SerializedData;
    int response;

    Write_url = RTDB_url+path+".json?auth="+idToken;

    if(Command < 3){      
        Write_url += +"&timeout=2500ms";  
          
        serializeJson(*doc, SerializedData);
    }

    
    RTDBClient.setURL(Write_url);

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
        case READ_FW:
            RTDBClient.setURL(Base_Path+path+".json?auth="+idToken);
            response = RTDBClient.GET();
            break;

        default:
            Serial.print("Accion no implementada!");
            return false;
            break;
    }
    if (response < -0) {
        Serial.println(response);
        Serial.printf("HTTP error: %s\n", 
        RTDBClient.errorToString(response).c_str());
    }

    doc->clear();
    deserializeJson(*doc, RTDBClient.getString());

    return true;
}

long long  Real_Time_Database::Get_Timestamp(String path, JsonDocument *respuesta){
    String SerializedData;
    int response;

    Write_url = RTDB_url+path+".json?auth="+idToken+"&timeout=2000ms";

    RTDBClient.setURL(Write_url);

    response = RTDBClient.GET();

    if (response < -11) {
        Serial.println(response);
        Serial.printf("HTTP error: %s\n", 
        RTDBClient.errorToString(response).c_str());
    }

    String payload = RTDBClient.getString();
    long long data1 = atoll(payload.c_str());
    if(data1<1){
        respuesta->clear();
        deserializeJson(*respuesta,payload);
    }

    //conection refused
    if(response == -1){
        Serial.println("Conection refused");
        return -1;
    }
    return data1;

}