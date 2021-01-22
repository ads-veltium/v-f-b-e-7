////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  FirebaseClient.h
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 26/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//  Edited by Joël Martínez on 05/01/2020
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FirebaseClient.h"

const char* veltiumbackend_firebase_project = "veltiumdev-default-rtdb.firebaseio.com";
const char* veltiumbackend_database_secret  = "1mJtIoJnreTFPbyTFQgdN0hj3GQRQo50SJOYE7Al";
const char* veltiumbackend_api_key          = "AIzaSyCYZpVNUOQvrXvc3qETxCqX4DPfp3Fwe3w";
const char* veltiumbackend_user             = "joelmartinez@veltium.com";
const char* veltiumbackend_password         = "Escolapios2";
const char* STORAGE_BUCKET_ID               = "veltiumdev.appspot.com";

//Define FIREBASE data object

//FirebaseData *firebaseData = (FirebaseData *) ps_malloc(sizeof(FirebaseData));
//FirebaseAuth *auth = (FirebaseAuth *) ps_malloc(sizeof(FirebaseAuth));
//FirebaseConfig *config = (FirebaseConfig *) ps_malloc(sizeof(FirebaseConfig));

static FirebaseData firebaseData EXT_RAM_ATTR;
FirebaseAuth   auth;
FirebaseConfig config;
/*static FirebaseAuth   *auth         EXT_RAM_ATTR;
static FirebaseConfig *config       EXT_RAM_ATTR;*/

String url;

//Extern variables
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Update_Status          UpdateStatus;
extern carac_Params                 Params;
extern carac_Coms                   Coms;

static FirebaseJson Control_Json              EXT_RAM_ATTR;
static FirebaseJson Status_Json               EXT_RAM_ATTR;
static FirebaseJson Params_Json               EXT_RAM_ATTR;
static FirebaseJson Comms_Json                EXT_RAM_ATTR;

static FirebaseJsonData Datos_Json            EXT_RAM_ATTR; 

//RTOS 
//Semaforo de acceso a firebase
SemaphoreHandle_t firebase_Access = NULL;

void DownloadTask(void *arg);

void FreeFirebaseHeap(){
   if(ESP.getFreeHeap()<30000){
    stopFirebaseClient();
    initFirebaseClient();
  }
}

uint16 ParseFirmwareVersion(String Texto){
  String sub = Texto.substring(6, 10); 
  return(sub.toInt());
}

/*************************
 Client control functions
*************************/
bool initFirebaseClient(){
    Serial.println("INIT Firebase Client");

    //ATENCION!!!!!! ACCeso total ala base de datos, cambiar por usuario y contraseña!!!!
    //config.signer.tokens.legacy_token = veltiumbackend_database_secret;

    config.api_key = veltiumbackend_api_key;
    config.host    = veltiumbackend_firebase_project;

    auth.user.email    = veltiumbackend_user;
    auth.user.password = veltiumbackend_password;

    Firebase.begin(&config,&auth);
    //Firebase.reconnectWiFi(false);

    //Set database read timeout to 1 minute (max 15 minutes)
    //Firebase.setReadTimeout(*firebaseData, 1000 * 60);
    //Firebase.setwriteSizeLimit(*firebaseData, "tiny");

    Serial.println("Done");
    if(firebase_Access==NULL){
      vSemaphoreCreateBinary(firebase_Access);
    }
    xSemaphoreGive(firebase_Access);

    return true;

}

bool stopFirebaseClient(){

    ConfigFirebase.FirebaseConnected=0;
    Serial.println("Stop Firebase Client");

    Firebase.RTDB.endStream(&firebaseData);
    Firebase.RTDB.removeStreamCallback(&firebaseData);
    Firebase.RTDB.end(&firebaseData);

    //Deallocate
    //delete &firebaseData;  
    return true;
}

uint8_t getfirebaseClientStatus(){
  return ConfigFirebase.FirebaseConnected;
}

/*************************
 Comprobar Actualizaciones
*************************/
bool CheckForUpdate(){
  String ESP_Ver;
  String PSOC5_Ver;
  url="";

  /*********************/
  UpdateStatus.BetaPermission=1;
  memcpy(Params.Fw_Update_mode,"AA",2);
  /*********************/

  if(UpdateStatus.BetaPermission){
    //Check PSOC5 firmware updates
    Firebase.RTDB.getString(&firebaseData, "/prod/fw/beta/VELT2/verstr", &PSOC5_Ver);
    uint16 Psoc_int_Version=ParseFirmwareVersion(PSOC5_Ver);

    if(UpdateStatus.PSOC5_Act_Ver<Psoc_int_Version){
      UpdateStatus.PSOC5_UpdateAvailable=1;
      Firebase.RTDB.getString(&firebaseData, "/prod/fw/beta/VELT2/url", &url);
      Serial.println("Updating PSOC5 with Beta firmware");
      return true;
    }

    //Check ESP32 firmware updates
    else{
      Firebase.RTDB.getString(&firebaseData, "/prod/fw/beta/VBLE2/verstr", &ESP_Ver);
      uint16 ESP_int_Version=ParseFirmwareVersion(ESP_Ver);

      if(UpdateStatus.ESP_Act_Ver<ESP_int_Version){
        UpdateStatus.ESP_UpdateAvailable=1;
        Serial.println("Updating ESP32 with Beta firmware");
        Firebase.RTDB.getString(&firebaseData, "/prod/fw/beta/VBLE2/url", &url);
        return true;
      }
    }  
  }
  else{
    Firebase.RTDB.getString(&firebaseData, "/prod/fw/prod/VELT2/verstr", &PSOC5_Ver);
    Firebase.RTDB.getString(&firebaseData, "/prod/fw/prod/VBLE2/verstr", &ESP_Ver);
  }
  return false;
}

/***************************************************
  Funciones de escritura
***************************************************/
bool WriteFirebaseStatus(String Path){

  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10) )){
    Serial.println("UPDATE FIREBASE CALLED");

    Status_Json.set("HPT_Status",String(Status.HPT_status).substring(0,2));
    Status_Json.set("ICP_Status",String(Status.ICP_status).substring(0,2));
    Status_Json.set("DC_Leackage_Status", String(Status.DC_Leack_status).substring(0,2));
    Status_Json.set("Con_Lock", String(Status.Con_Lock).substring(0,2));
    Status_Json.set("Max_Current_Cable", (int)Status.Measures.max_current_cable);
    Status_Json.set("Medidas_Fase_A/power_factor", (int)Status.Measures.power_factor);
    Status_Json.set("Medidas_Fase_A/instant_current",(int) Status.Measures.instant_current);
    Status_Json.set("Medidas_Fase_A/instant_voltage", (int)Status.Measures.instant_voltage);
    Status_Json.set("Medidas_Fase_A/active_power", (int)Status.Measures.active_power);
    Status_Json.set("Medidas_Fase_A/reactive_power", (int)Status.Measures.reactive_power);
    Status_Json.set("Medidas_Fase_A/active_energy", (int)Status.Measures.active_energy);
    Status_Json.set("Medidas_Fase_A/reactive_energy", (int)Status.Measures.reactive_energy);
    Status_Json.set("Medidas_Fase_A/apparent_power", (int)Status.Measures.apparent_power);
    Status_Json.set("Medidas_Fase_A/consumo_domestico", (int)Status.Measures.consumo_domestico);
    Status_Json.set("Codigo_Error",(int)Status.error_code);
    
    //Params_Json.set("Time");

    if(!Firebase.RTDB.updateNode(&firebaseData, Path.c_str(), &Status_Json)){
      return 0;
      xSemaphoreGive(firebase_Access);
    }
    xSemaphoreGive(firebase_Access);
  }
  return 1;
}

bool WriteFirebaseParams(String Path){

  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10) )){
    Serial.println("Write Params CALLED");

    Params_Json.set("Ubic_sensor",Params.Ubicacion_Sensor);
    Params_Json.set("Inst_current_limit",(int)Params.inst_current_limit);
    Params_Json.set("Potencia_Contratada",(int)Params.potencia_contratada);
    Params_Json.set("CDP_On",Params.CDP_On);
    Params_Json.set("Autentication_mode",String(Params.autentication_mode).substring(0,2));
    Params_Json.set("Fw_Update_mode",String(Params.Fw_Update_mode).substring(0,2));    
    //Params_Json.set("Time");

    if(!Firebase.RTDB.updateNode(&firebaseData, Path.c_str(), &Params_Json)){
      return 0;
      xSemaphoreGive(firebase_Access);
    }
    xSemaphoreGive(firebase_Access);
  }
  return 1;
}

bool WriteFirebaseComs(String Path){

  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10) )){
    Serial.println("Write Coms CALLED");

    #ifdef USE_WIFI
      Comms_Json.set("WIFI/ON",Coms.Wifi.ON);
      Comms_Json.set("WIFI/AP",Coms.Wifi.AP);
      Comms_Json.set("WIFI/Password",Coms.Wifi.Pass);
    #endif

    #ifdef USE_ETH
      Comms_Json.set("ETH/ON",Coms.ETH.ON);
      Comms_Json.set("ETH/Auto",Coms.ETH.Auto);
      if(!Coms.ETH.Auto){
        Comms_Json.set("ETH/IP1",Coms.ETH.IP1);
        Comms_Json.set("ETH/IP2",Coms.ETH.IP2);
        Comms_Json.set("ETH/AP",Coms.ETH.AP);
        Comms_Json.set("ETH/Gateway",Coms.ETH.Gateway);
        Comms_Json.set("ETH/Mask",Coms.ETH.Mask);
        Comms_Json.set("ETH/Pass",Coms.ETH.Pass);
      }
    #endif

    #ifdef USE_GSM
      Comms_Json.set("GSM/ON",Coms.GSM.ON);
      Comms_Json.set("GSM/APN",Coms.GSM.APN);
      Comms_Json.set("GSM/Pass",Coms.GSM.Pass);
    #endif
   
    //Params_Json.set("Time");

    if(!Firebase.RTDB.updateNode(&firebaseData, Path.c_str(), &Comms_Json)){
      return 0;
      xSemaphoreGive(firebase_Access);
    }
    xSemaphoreGive(firebase_Access);
  }
  return 1;
}

bool WriteFirebaseControl(String Path){
  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10))){
    if(!Firebase.RTDB.updateNode(&firebaseData, Path.c_str(), &Control_Json)){
        xSemaphoreGive(firebase_Access);
        return false;
      }
  xSemaphoreGive(firebase_Access);
  } 
  return true;
}


/***************************************************
  Funciones de lectura
***************************************************/
bool ReadFirebaseControl(String Path){

  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10))){
    
    QueryFilter query;
    if(!Firebase.RTDB.get(&firebaseData,Path.c_str())){  
        xSemaphoreGive(firebase_Access);
        return false;
    }
    Control_Json = firebaseData.jsonObject();

    Control_Json.get(Datos_Json, "start");
    Comands.start = ((Datos_Json.boolValue) ? Datos_Json.boolValue : Comands.start);

    Control_Json.get(Datos_Json, "stop");
    Comands.stop = ((Datos_Json.boolValue) ? Datos_Json.boolValue : Comands.stop);

    Control_Json.get(Datos_Json, "reset");
    Comands.Reset = ((Datos_Json.boolValue) ? Datos_Json.boolValue : Comands.Reset);

    Control_Json.get(Datos_Json, "Fw_Update");
    Comands.Fw_Update = ((Datos_Json.boolValue) ? Datos_Json.boolValue : Comands.Fw_Update);

    Control_Json.get(Datos_Json, "Lim_Corriente");
    Comands.Limite_Corriente = ((Datos_Json.intValue) ? Datos_Json.intValue : Comands.Limite_Corriente);

    xSemaphoreGive(firebase_Access);
  } 
  return true;
}

/***************************************************
 Descargar archivos de actualizacion
***************************************************/
void DownloadFile(){
  UpdateStatus.DescargandoArchivo=1;

  File UpdateFile;
  String FileName;

  //Si descargamos actualizacion para el PSOC5, debemos crear el archivo en el SPIFFS
  if(UpdateStatus.PSOC5_UpdateAvailable){
    SPIFFS.begin(0,"/spiffs",1,"ESP32");
    FileName="/FreeRTOS_V6.cyacd";
    if(SPIFFS.exists(FileName)){
      vTaskDelay(50/configTICK_RATE_HZ);
      SPIFFS.format();
    }
    vTaskDelay(50/configTICK_RATE_HZ);
    UpdateFile = SPIFFS.open(FileName, FILE_WRITE);
  }

  //Firebase.Storage.download(&fbdo, STORAGE_BUCKET_ID, "path/to/fie/filename.png", "/path/to/save/filename.png", mem_storage_type_flash)

  //Abrir la conexion con el servidor
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  // Obtener tamaño del archivo
  int contentLen = http.getSize(); 
  int total=contentLen;
  int suma =0;
  WiFiClient* stream = NULL;
  
  Serial.println(contentLen);
  if (httpCode > 0) {
    if(http.connected()) {

      stream = http.getStreamPtr();     
      uint8_t buff[128] = { 0 };
      

      if(UpdateStatus.ESP_UpdateAvailable){
        if(!Update.begin(contentLen)){
          return;
        };
      }
      Serial.println("Entrando en el bucle");
      //Leer datos del servidor
      while (http.connected() && (contentLen > 0 || contentLen == -1)){

        size_t size = stream->available();
        if (size){
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

          if(UpdateStatus.PSOC5_UpdateAvailable){
            int escritos = UpdateFile.write(buff, c);
            if(escritos!=c){
              Serial.printf("\nError %i vs %i \n",escritos, c);
            }
          }
          else{
            Update.write(buff,c);
          }        

          if (contentLen > 0){
            contentLen -= c;
            suma += c;
          }         
          
        }      
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }
  Serial.println("No escrito");
  Serial.printf("%i vs %i\n",  suma, UpdateFile.size());
  
  Serial.println("Archivo descargado!");
  if(UpdateStatus.ESP_UpdateAvailable){
    if(Update.end()){	
      Serial.println("Micro Actualizado!"); 
      //reboot
      MAIN_RESET_Write(0);
      ESP.restart();
    }
  }

  //Aqui solo llega si está actualizando el psoc5
  //Liberar memoria
  
  stream->stop();
  http.end(); 
  delete stream;

  Serial.println(UpdateFile.size());
  Serial.println(total);
  if(UpdateFile.size()==total){
    UpdateFile.close();
    setMainFwUpdateActive(1);
  }

  UpdateStatus.DescargandoArchivo=0;
}

/***************************************************
 Tarea de control de firebase
***************************************************/
void Firebase_Conn_Task(void *args){
  uint8_t ConnectionState =  DISCONNECTED;
  uint8_t Error_Count;
  String Base_Path;

  while(1){
    switch (ConnectionState){

    //Comprobar estado de la red
    case DISCONNECTED:
      if(ConfigFirebase.InternetConection){
        Error_Count=0;
        Base_Path="/prod/devices/";
        ConnectionState=CONNECTING;
      }
      break;

    case CONNECTING:
      if(initFirebaseClient()){
        ConfigFirebase.FirebaseConnected=1;
        Base_Path += (char *)ConfigFirebase.Device_Db_ID;
        ConnectionState=IDLE;
      }
      break;

    case IDLE:
      //No connection == Disconnect
      //Error_count > 10 == Disconnect
      if(!ConfigFirebase.InternetConection || Error_Count>10){
        ConnectionState=DISCONNECTING;
        break;
      }

      //Las ordenes de conrtrol.cpp siempre tienen mas prioridad
      else if(ConfigFirebase.WriteStatus){
        ConfigFirebase.WriteStatus=false;
        ConnectionState = WRITTING_STATUS;
        break;
      }

      else if(ConfigFirebase.WriteParams){
        ConfigFirebase.WriteParams=false;
        ConnectionState = WRITTING_PARAMS;
        break;
      }

      else if(ConfigFirebase.WriteComs){
        ConfigFirebase.WriteComs=false;
        ConnectionState = WRITTING_COMS;
        break;
      }

      //Comprobar actualizaciones
      else if(Comands.Fw_Update){
        Comands.Fw_Update=0;
        ConnectionState = UPDATING;
        break;
      }


      ConnectionState=READING_CONTROL;
      break;

    /*********************** WRITTING states **********************/
    case WRITTING_CONTROL:
      Error_Count+=!WriteFirebaseControl(Base_Path+"/control/");
      ConnectionState=WRITTING_STATUS;
      break;

    case WRITTING_STATUS:
      Error_Count+=!WriteFirebaseStatus(Base_Path+"/status/");
      ConnectionState=IDLE;
      break;

    case WRITTING_COMS:
      Error_Count+=!WriteFirebaseComs(Base_Path+"/coms/");
      ConnectionState=IDLE;
      break;
    
    case WRITTING_PARAMS:
      Error_Count+=!WriteFirebaseParams(Base_Path+"/params/");
      ConnectionState=IDLE;
      break;
    /*********************** READING states **********************/
    case READING_CONTROL:

      Error_Count+=!ReadFirebaseControl(Base_Path+"/control/");
      if(Comands.start || Comands.stop || Comands.Reset || Comands.Fw_Update){
        Control_Json.set("start", false);
        Control_Json.set("stop", false);
        Control_Json.set("reset", false);
        Control_Json.set("Fw_Update", false);
        ConnectionState=WRITTING_CONTROL;
        break;
      }

      ConnectionState=IDLE;
      break;

    case READING_PARAMS:
      //Error_Count+=!ReadFirebaseParams(Base_Path+"/params/");
      ConnectionState=IDLE;
      break;
    
    case READING_COMS:
      //Error_Count+=!ReadFirebaseComsBase_Path+"/coms/");
      ConnectionState=IDLE;
      break;

    /*********************** UPDATING states **********************/
    case UPDATING:
      if(CheckForUpdate()){
        if(!memcmp(Params.Fw_Update_mode, "AA",2)){
          ConnectionState=DOWNLOADING;
        }
        else{
          ConnectionState=IDLE;
        }
      }
      else{
        ConnectionState=IDLE;
      }
      break;

    case DOWNLOADING:
      //DownloadFile();
      Serial.println("Descargando...");
      firebaseData.setResponseSize(1024);
      SPIFFS.begin(0,"/spiffs",1,"ESP32");
      if(!Firebase.Storage.download(&firebaseData, STORAGE_BUCKET_ID, "fw/VBLE2_0501","/firmware.bin", mem_storage_type_flash))
      {
        Serial.println("FAILED");
        Serial.println("REASON: " + firebaseData.errorReason());
        Serial.println("------------------------------------");
        Serial.println();
      }
      else{
        Serial.println("SUcceSS");
      }
      
      
      ConnectionState=INSTALLING;
      break;

    case INSTALLING:
      //Do nothing
      break;

    /*********************** DISCONNECT states **********************/
    case DISCONNECTING:
      if(stopFirebaseClient()){
        ConnectionState=DISCONNECTED;
      }
      break;
    default:
      while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
        Serial.printf("Error en la maquina d estados de firebase: %i \n", ConnectionState);

      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}