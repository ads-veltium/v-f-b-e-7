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
FirebaseData *firebaseData;
static FirebaseAuth auth;

String url;

//Extern variables
extern uint8 version_firmware[11];
extern uint8 PSOC5_version_firmware[11];	 
extern carac_Auto_Update AutoUpdate;

TaskHandle_t xHandle2 = NULL;

void DownloadTask(void *arg);

int ParseFirmwareVersion(String Texto){
  String sub = Texto.substring(6, 10); 
  return(sub.toInt());
}

void CheckForUpdate(){
  String ESP_Ver;
  String PSOC5_Ver;
  AutoUpdate.BetaPermission=1;

  if(AutoUpdate.BetaPermission){
    Firebase.getString(*firebaseData, "/prod/fw/beta/VELT2/verstr", PSOC5_Ver);
    Firebase.getString(*firebaseData, "/prod/fw/beta/VBLE2/verstr", ESP_Ver);
  }
  else{
    Firebase.getString(*firebaseData, "/prod/fw/prod/VELT2/verstr", PSOC5_Ver);
    Firebase.getString(*firebaseData, "/prod/fw/prod/VBLE2/verstr", ESP_Ver);
  }

  int Psoc_int_Version=ParseFirmwareVersion(PSOC5_Ver);
  int ESP_int_Version=ParseFirmwareVersion(ESP_Ver);

  int ESP_Act_int_Version = ParseFirmwareVersion((char *)(version_firmware));
  int PSOC5_Act_int_Version = ParseFirmwareVersion((char *)(PSOC5_version_firmware));

  
  if(PSOC5_Act_int_Version<Psoc_int_Version){
    AutoUpdate.PSOC5_UpdateAvailable=1;
  }
  
  if(ESP_Act_int_Version<ESP_int_Version){
    url="";
    Firebase.getString(*firebaseData, "/prod/fw/beta/VBLE2/url", url);

    stopFirebaseClient();
    xTaskCreate(DownloadTask,"TASK DOWNLOAD",4096,NULL,5,&xHandle2);
    AutoUpdate.ESP_UpdateAvailable=1;
  }
}


void initFirebaseClient(){
    Serial.println("INIT Firebase Client");

    firebaseData=new FirebaseData();
    Firebase.begin(veltiumbackend_firebase_project,veltiumbackend_database_secret);

    //Firebase.reconnectWiFi(true);
    //Set database read timeout to 1 minute (max 15 minutes)
    Firebase.setReadTimeout(*firebaseData, 1000 * 60);
    Firebase.setwriteSizeLimit(*firebaseData, "tiny");
}

void stopFirebaseClient(){
    Serial.println("Stop Firebase Client");

    Firebase.endStream(*firebaseData);
    Firebase.removeStreamCallback(*firebaseData);
    Firebase.end(*firebaseData);

    //Deallocate
    delete firebaseData;
    firebaseData = nullptr;   
}

void WriteFireBaseData(){
  Firebase.setDouble(*firebaseData, "/test/esp32test/doubletest_5", 3.141592);

  Firebase.setTimestamp(*firebaseData, "/test/esp32test/ts");

  Serial.println("Written DOUBLE value to /test/esp32test/doubletest_5");
}

void DownloadTask(void *arg){

  Serial.println("Empezando descarga del Fichero");
  AutoUpdate.DescargandoArchivo=1;

  HTTPClient http;
  WiFiClient* stream;
  uint8 LedPointer=0;
	uint8 Luminosidad=100;
  uint8 Timeout=0;

  SPIFFS.remove("/Archivo.txt");
  
  File UpdateFile = SPIFFS.open("/Archivo.txt", "w");

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {

    // Obtener tamaño del archivo
    int contentLen = http.getSize();

    if(http.connected()) {

      stream = http.getStreamPtr();     
      uint8_t buff[256] = { 0 };

      //Leer datos del servidor
      while (http.connected() && (contentLen > 0 || contentLen == -1)){

        //Control de los leds
        if(++Timeout>3){
          changeOne(Luminosidad,20,80,50,LedPointer);
          if(++LedPointer>=8){
            if(Luminosidad==100){
              Luminosidad=0;
            }
            else{
              Luminosidad=100;
            }
            LedPointer=0;
          }
          Timeout=0;
        }

        size_t size = stream->available();
        if (size){
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          UpdateFile.write(buff, c);

          if (contentLen > 0){
            contentLen -= c;
          }
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
      }
      Serial.print("File size: ");
      Serial.println(UpdateFile.size());
    }
  }

  //Liberar memoria
  UpdateFile.close();
  stream->stop();
  http.end();

  delete stream;

  initFirebaseClient();

  Serial.println("Fichero descargado");
  AutoUpdate.DescargandoArchivo=0;

  //Eliminar la tarea
  vTaskDelete(xHandle2);
}