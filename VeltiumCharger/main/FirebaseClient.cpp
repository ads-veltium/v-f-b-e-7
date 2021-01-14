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

FirebaseData *firebaseData = (FirebaseData *) ps_malloc(sizeof(FirebaseData));
FirebaseAuth *auth = (FirebaseAuth *) ps_malloc(sizeof(FirebaseAuth));

String url;

//Extern variables
extern uint8 version_firmware[11];
extern uint8 PSOC5_version_firmware[11];	 
extern carac_Auto_Update AutoUpdate;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Firebase_Control ControlFirebase;
extern carac_Firebase_Status StatusFirebase;

TaskHandle_t xHandleDownloadTask = NULL;
TaskHandle_t xHandleUpdateTask=NULL;

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
      xTaskCreate(UpdateFirebaseControl_Task, "Firebase Update", 4096*2, NULL, 5, &xHandleUpdateTask); 
    }
    
    ConfigFirebase.FirebaseConnected=1;
    //Check permisions stored in firebase
}

void stopFirebaseClient(){
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

/*************************
 Database acces control
*************************/
void CheckForUpdate(){
  FreeFirebaseHeap();
  ConfigFirebase.FirebaseConnected=0;
  String ESP_Ver;
  String PSOC5_Ver;
  url="";

  if(AutoUpdate.BetaPermission){
    //Check PSOC5 firmware updates
    Firebase.getString(*firebaseData, "/prod/fw/beta/VELT2/verstr", PSOC5_Ver);
    uint16 Psoc_int_Version=ParseFirmwareVersion(PSOC5_Ver);

    if(AutoUpdate.PSOC5_Act_Ver<Psoc_int_Version){
      Firebase.getString(*firebaseData, "/prod/fw/beta/VELT2/url", url);
      AutoUpdate.PSOC5_UpdateAvailable=1;
      Serial.println("Updating PSOC5 with Beta firmware");
    }

    //Check ESP32 firmware updates
    else{
      Firebase.getString(*firebaseData, "/prod/fw/beta/VBLE2/verstr", ESP_Ver);
      uint16 ESP_int_Version=ParseFirmwareVersion(ESP_Ver);

      if(AutoUpdate.ESP_Act_Ver<ESP_int_Version){
        Serial.println("Updating ESP32 with Beta firmware");
        AutoUpdate.ESP_UpdateAvailable=1;
        Firebase.getString(*firebaseData, "/prod/fw/beta/VBLE2/url", url);
      }
    }  
  }
  else{
    Firebase.getString(*firebaseData, "/prod/fw/prod/VELT2/verstr", PSOC5_Ver);
    Firebase.getString(*firebaseData, "/prod/fw/prod/VBLE2/verstr", ESP_Ver);
  }
  if(AutoUpdate.ESP_UpdateAvailable || AutoUpdate.PSOC5_UpdateAvailable) {
    Serial.println("Updating firmware");
    xTaskCreate(DownloadTask,"TASK DOWNLOAD",4096*2,NULL,1,&xHandleDownloadTask); 
    stopFirebaseClient();
  }
  else{
    ConfigFirebase.FirebaseConnected=1;
  }
  FreeFirebaseHeap();
}

void UpdateFirebaseStatus(){
  if(xSemaphoreTake(firebase_Access,  portMAX_DELAY )){
    FreeFirebaseHeap();
  
    Serial.println("UPDATE FIREBASE CALLED");
    String Path="/prod/devices/";
    Path=Path + (char *)ConfigFirebase.Device_Db_ID + "/status/" ;

    Firebase.setString(*firebaseData,Path+"HP",StatusFirebase.HPT_status);
    Firebase.setInt(*firebaseData,Path+"current",StatusFirebase.inst_current);
    xSemaphoreGive(firebase_Access);
  }
}

void UpdateFirebaseControl_Task(void *arg){
  String Base_Path="/prod/devices/";
  Base_Path=Base_Path + (char *)ConfigFirebase.Device_Db_ID;
  
  bool Ret;

  while(1){
    if(xSemaphoreTake(firebase_Access,  100/configTICK_RATE_HZ)){
      Ret=false;
      Firebase.getBool(*firebaseData,Base_Path+"/control/start",Ret);
      
      if(Ret){
        ControlFirebase.start=1;
        Firebase.setBool(*firebaseData,Base_Path+"/control/start",false);
      }
      else{
        Firebase.getBool(*firebaseData,Base_Path+"/control/stop",Ret);
        if(Ret){
          ControlFirebase.stop=1;
          Firebase.setBool(*firebaseData,Base_Path+"/control/stop",false);
        }
      }
      
      if(!Ret){

        Firebase.getBool(*firebaseData,Base_Path +"/fw/StartUpdate",Ret);
        if(Ret){
          Firebase.setBool(*firebaseData,Base_Path+"/fw/StartUpdate",false);
          CheckForUpdate();
        }
      }
      xSemaphoreGive(firebase_Access);
    } 
    vTaskDelay(750/configTICK_RATE_HZ); 
  }
}

void DownloadTask(void *arg){
  Serial.println("Empezando descarga del Fichero");
  AutoUpdate.DescargandoArchivo=1;

  HTTPClient http;
  WiFiClient* stream = new WiFiClient();

  TaskHandle_t HandleLeds;

  xTaskCreate(LedUpdateDownload_Task, "Led Control", configMINIMAL_STACK_SIZE, (void*)2 , 1, &HandleLeds); 

  SPIFFS.remove("/Archivo.txt");
  SPIFFS.remove("/FreeRTOS_V6.cyacd");
  SPIFFS.remove("/firmware.bin");
  
  File UpdateFile = SPIFFS.open("/Archivo.txt", "w");
  Serial.println(url);
  http.begin(url);
  int httpCode = http.GET();  
  int contentLen=0;
  unsigned long StartTime = millis();
  if (httpCode > 0) {

    // Obtener tamaño del archivo
    contentLen = http.getSize();  

    if(http.connected()) {

      stream = http.getStreamPtr();     
      uint8_t buff[512] = { 0 };

      //Leer datos del servidor
      while (http.connected() && (contentLen > 0 || contentLen == -1)){

        size_t size = stream->available();
        if (size){
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          UpdateFile.write(buff, c);

          if (contentLen > 0){
            contentLen -= c;
          }         
        }
        vTaskDelay(5/configTICK_RATE_HZ);
      }
    }
  }
  unsigned long CurrentTime = millis();
  Serial.println("Elapsed time");
  Serial.print(CurrentTime - StartTime);

  //Liberar memoria
  stream->stop();
  http.end();

  delete stream;

  Serial.println("Fichero descargado");
  AutoUpdate.DescargandoArchivo=0;

  if(AutoUpdate.Auto_Act && UpdateFile.size()==contentLen){
    UpdateFile.close();
    if(AutoUpdate.ESP_UpdateAvailable){
      SPIFFS.rename("/Archivo.txt", "/firmware.bin");
      UpdateESP();
    }
    else if(AutoUpdate.PSOC5_UpdateAvailable){
      SPIFFS.rename("/Archivo.txt", "/FreeRTOS_V6.cyacd");
      setMainFwUpdateActive(1);
    }
  }
  
  //Eliminar la tarea
  vTaskDelete(HandleLeds);
  vTaskDelete(xHandleDownloadTask);
}