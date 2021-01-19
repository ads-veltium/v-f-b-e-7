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
const char* veltiumbackend_database_secret = "1mJtIoJnreTFPbyTFQgdN0hj3GQRQo50SJOYE7Al";
const char* veltiumbackend_user="joelmartinez@veltium.com";
const char* veltiumbackend_password="Escolapios2";

//Define FirebaseESP32 data object

//FirebaseData *firebaseData = (FirebaseData *) ps_malloc(sizeof(FirebaseData));
//FirebaseAuth *auth = (FirebaseAuth *) ps_malloc(sizeof(FirebaseAuth));

static FirebaseData *firebaseData EXT_RAM_ATTR;
static FirebaseAuth *auth EXT_RAM_ATTR;

String url;

//Extern variables
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Auto_Update            AutoUpdate;

TaskHandle_t xHandleUpdateTask=NULL;

StaticTask_t xFirebaseBuffer;
static StackType_t xFirebaseStack[STACK_SIZE] EXT_RAM_ATTR;
static FirebaseJson Lectura_Json              EXT_RAM_ATTR;
static FirebaseJson Escritura_Json            EXT_RAM_ATTR;
static FirebaseJsonData Datos_Json            EXT_RAM_ATTR; 

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

void initFirebaseClient(){
    Serial.println("INIT Firebase Client");

    firebaseData = new FirebaseData();
    Firebase.begin(veltiumbackend_firebase_project,veltiumbackend_database_secret);

    //Firebase.reconnectWiFi(true);
    //Set database read timeout to 1 minute (max 15 minutes)
    Firebase.setReadTimeout(*firebaseData, 1000 * 60);
    Firebase.setwriteSizeLimit(*firebaseData, "tiny");

    if(firebase_Access==NULL){
      vSemaphoreCreateBinary(firebase_Access);
    }
    xSemaphoreGive(firebase_Access);

    if(xHandleUpdateTask!=NULL){
      vTaskResume(xHandleUpdateTask);
    }
    else{
      xHandleUpdateTask = xTaskCreateStatic(UpdateFirebaseControl_Task,"Firebase Update",STACK_SIZE,NULL,PRIORIDAD_FIREBASE,xFirebaseStack,&xFirebaseBuffer );
    }

    #ifndef USE_DRACO_BLE
      
      memcpy(&ConfigFirebase.Device_Db_ID,"CD012012140000049461C051E014",29);
      AutoUpdate.BetaPermission=1;

    #endif

    ConfigFirebase.FirebaseConnected=1;

}

void stopFirebaseClient(){

    if(xHandleUpdateTask!=NULL){
      vTaskSuspend(xHandleUpdateTask);
    }

    ConfigFirebase.FirebaseConnected=0;
    Serial.println("Stop Firebase Client");

    Firebase.endStream(*firebaseData);
    Firebase.removeStreamCallback(*firebaseData);
    Firebase.end(*firebaseData);

    //Deallocate
    delete firebaseData;
    firebaseData = nullptr;   

}

void pauseFirebaseClient(){
  if(xHandleUpdateTask!=NULL){
    vTaskSuspend(xHandleUpdateTask);
  }
}

void resumeFirebaseClient(){
  if(xHandleUpdateTask!=NULL && firebase_Access!=NULL){
    xSemaphoreGive(firebase_Access);
    vTaskResume(xHandleUpdateTask);
  }
  else{
    initFirebaseClient();
  }
}

uint8_t getfirebaseClientStatus(){
  return ConfigFirebase.FirebaseConnected;
}

/*************************
 Comprobar Actualizaciones
*************************/
void CheckForUpdate(){

  ConfigFirebase.FirebaseConnected=0;
  String ESP_Ver;
  String PSOC5_Ver;
  url="";
  uint8 tipo=0;
  //AutoUpdate.BetaPermission=1;
  Serial.println("Updating firmware");
  if(1/*AutoUpdate.BetaPermission*/){
    //Check PSOC5 firmware updates
    Firebase.getString(*firebaseData, "/prod/fw/beta/VELT2/verstr", PSOC5_Ver);
    uint16 Psoc_int_Version=ParseFirmwareVersion(PSOC5_Ver);

    if(AutoUpdate.PSOC5_Act_Ver<Psoc_int_Version){
      Firebase.getString(*firebaseData, "/prod/fw/beta/VELT2/url", url);
      tipo=1;
      Serial.println("Updating PSOC5 with Beta firmware");
    }

    //Check ESP32 firmware updates
    else{
      Firebase.getString(*firebaseData, "/prod/fw/beta/VBLE2/verstr", ESP_Ver);
      uint16 ESP_int_Version=ParseFirmwareVersion(ESP_Ver);

      if(AutoUpdate.ESP_Act_Ver<ESP_int_Version){
        Serial.println("Updating ESP32 with Beta firmware");
        tipo=2;
        Firebase.getString(*firebaseData, "/prod/fw/beta/VBLE2/url", url);
      }
    }  
  }
  else{
    Firebase.getString(*firebaseData, "/prod/fw/prod/VELT2/verstr", PSOC5_Ver);
    Firebase.getString(*firebaseData, "/prod/fw/prod/VBLE2/verstr", ESP_Ver);
  }
  if(tipo!=0) {
    xTaskCreate(DownloadTask,"TASK DOWNLOAD",4096*2,(void*) tipo,PRIORIDAD_DESCARGA, NULL);
    stopFirebaseClient();
  }
  else{
    ConfigFirebase.FirebaseConnected=1;
  }
}

/***************************************************
 Acutalizar la base de datos con el estado actual
***************************************************/
void UpdateFirebaseStatus(){

  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10) )){
    Serial.println("UPDATE FIREBASE CALLED");
    
    String Path="/prod/devices/";
    Path=Path + (char *)ConfigFirebase.Device_Db_ID + "/status/" ;
    Serial.println(Status.HPT_status);
    Escritura_Json.set("HPT_Status",String(Status.HPT_status));
    Escritura_Json.set("ICP_Status",String(Status.ICP_status));
    Escritura_Json.set("DC_Leackage_Status", String(Status.DC_Leack_status));
    Escritura_Json.set("Con_Lock", String(Status.Con_Lock));
    Escritura_Json.set("Max_Current_Cable", Status.Measures.max_current_cable);
    Escritura_Json.set("Medidas_Fase_A/power_factor", Status.Measures.power_factor);
    Escritura_Json.set("Medidas_Fase_A/instant_current", Status.Measures.instant_current);
    Escritura_Json.set("Medidas_Fase_A/instant_voltage", Status.Measures.instant_voltage);
    Escritura_Json.set("Medidas_Fase_A/active_power", Status.Measures.active_power);
    Escritura_Json.set("Medidas_Fase_A/reactive_power", Status.Measures.reactive_power);
    Escritura_Json.set("Medidas_Fase_A/active_energy", Status.Measures.active_energy);
    Escritura_Json.set("Medidas_Fase_A/reactive_energy", Status.Measures.reactive_energy);
    Escritura_Json.set("Medidas_Fase_A/apparent_power", Status.Measures.apparent_power);
    Escritura_Json.set("Medidas_Fase_A/consumo_domestico", Status.Measures.consumo_domestico);
    Escritura_Json.set("Codigo_Error",Status.error_code);
    
    //Escritura_Json.set("Time");

    Firebase.updateNode(*firebaseData, Path, Escritura_Json);
    xSemaphoreGive(firebase_Access);
  }
}

/***************************************************
 Leer la base de datos de manera recurrente 
***************************************************/
void UpdateFirebaseControl_Task(void *arg){
  
  String Base_Path="/prod/devices/";
  Base_Path=Base_Path + (char *)ConfigFirebase.Device_Db_ID+"/control/";

  while(1){
    if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(100))){

      Firebase.get(*firebaseData,Base_Path);
      Lectura_Json = firebaseData->jsonObject();

      Lectura_Json.get(Datos_Json, "start");
      Comands.start = ((Datos_Json.boolValue) ? Datos_Json.boolValue : Comands.start);

      Lectura_Json.get(Datos_Json, "stop");
      Comands.stop = ((Datos_Json.boolValue) ? Datos_Json.boolValue : Comands.stop);

      Lectura_Json.get(Datos_Json, "reset");
      Comands.Reset = ((Datos_Json.boolValue) ? Datos_Json.boolValue : Comands.Reset);

      Lectura_Json.get(Datos_Json, "Fw_Update");
      Comands.Fw_Update = ((Datos_Json.boolValue) ? Datos_Json.boolValue : Comands.Fw_Update);

      Lectura_Json.get(Datos_Json, "Lim_Corriente");
      Comands.Limite_Corriente = ((Datos_Json.intValue) ? Datos_Json.intValue : Comands.Limite_Corriente);

      if(Comands.start || Comands.stop || Comands.Reset || Comands.Fw_Update){
        Lectura_Json.set("start", false);
        Lectura_Json.set("stop", false);
        Lectura_Json.set("reset", false);
        Lectura_Json.set("Fw_Update", false);
        Firebase.updateNode(*firebaseData, Base_Path, Lectura_Json);
      }
      xSemaphoreGive(firebase_Access);
    } 
    vTaskDelay(pdMS_TO_TICKS(1000)); 
  }
}

/***************************************************
 Descargar archivos de actualizacion
***************************************************/
void DownloadTask(void *arg){

  File UpdateFile;
  String FileName;

  //Si descargamos actualizacion para el PSOC5, debemos crear el archivo en el SPIFFS
  if((int) arg ==1){
    SPIFFS.begin(1,"/spiffs",1,"PSOC5");
    FileName="/FreeRTOS_V6.cyacd";
    if(SPIFFS.exists(FileName)){
      vTaskDelay(50/configTICK_RATE_HZ);
      SPIFFS.format();
    }
    vTaskDelay(50/configTICK_RATE_HZ);
    UpdateFile = SPIFFS.open(FileName, FILE_WRITE);
  }

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
      

      if((int) arg ==2){
        if(!Update.begin(contentLen)){
          return;
        };
      }

      //Leer datos del servidor
      while (http.connected() && (contentLen > 0 || contentLen == -1)){

        size_t size = stream->available();
        if (size){
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

          if((int) arg ==1){
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
        vTaskDelay(1/configTICK_RATE_HZ);
      }
    }
  }
  Serial.println("No escrito");
  Serial.printf("%i vs %i\n",  suma, UpdateFile.size());
  
  Serial.println("Archivo descargado!");
  if((int) arg ==2){
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

  //Eliminar la tarea
  //AutoUpdate.DescargandoArchivo=0;
  vTaskDelete(NULL);
}