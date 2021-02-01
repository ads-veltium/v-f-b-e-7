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

//#include "FirebaseClient.h"
#include "VeltFirebase.h"

#include "control.h"
#include "tipos.h"


Firebase Database;

StaticJsonDocument<512>  Lectura        EXT_RAM_ATTR;
StaticJsonDocument<512>  Escritura      EXT_RAM_ATTR;
StaticJsonDocument<1024> Actualizacion  EXT_RAM_ATTR;
//Define FIREBASE data object

//FirebaseData *firebaseData = (FirebaseData *) ps_malloc(sizeof(FirebaseData));
//FirebaseAuth *auth = (FirebaseAuth *) ps_malloc(sizeof(FirebaseAuth));
//FirebaseConfig *config = (FirebaseConfig *) ps_malloc(sizeof(FirebaseConfig));

const char* veltiumbackend_user             = "joelmartinez@veltium.com";
const char* veltiumbackend_password         = "Escolapios2";

String url;

//Extern variables
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Update_Status          UpdateStatus;
extern carac_Params                 Params;
extern carac_Coms                   Coms;


//RTOS 
//Semaforo de acceso a firebase
SemaphoreHandle_t firebase_Access = NULL;

void DownloadTask(void *arg);

void FreeFirebaseHeap(){
   if(ESP.getFreeHeap()<30000){
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

    Database.Auth.email=veltiumbackend_user;
    Database.Auth.pass=veltiumbackend_password;

    Database.Auth.begin();
    Database.Auth.LogIn();

    Database.RTDB.begin(FIREBASE_PROJECT);

    Serial.println("Done");
    if(firebase_Access==NULL){
      vSemaphoreCreateBinary(firebase_Access);
    }
    xSemaphoreGive(firebase_Access);

    return true;

}

uint8_t getfirebaseClientStatus(){
  return ConfigFirebase.FirebaseConnected;
}

/***************************************************
  Funciones de escritura
***************************************************/
bool WriteFirebaseStatus(String Path){
  bool response = false;
  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10) )){
    Serial.println("UPDATE FIREBASE CALLED");
    Escritura.clear();
    Escritura["hpt"]              = String(Status.HPT_status).substring(0,2);
    Escritura["icp"]              = Status.ICP_status;
    Escritura["dc_leak"]          = Status.DC_Leack_status;
    Escritura["conn_lock_stat"]   = Status.Con_Lock;

    //Medidas
    Escritura["max_current_cable"]                = Status.Measures.max_current_cable;
    Escritura["measures_phase_A/inst_current"]    = Status.Measures.instant_current;
    Escritura["measures_phase_A/inst_voltage"]    = Status.Measures.instant_voltage;
    Escritura["measures_phase_A/active_power"]    = Status.Measures.active_power;
    Escritura["measures_phase_A/reactive_power"]  = Status.Measures.reactive_power;
    Escritura["measures_phase_A/apparent_power"]  = Status.Measures.apparent_power;
    Escritura["measures_phase_A/power_factor"]    = Status.Measures.power_factor;
    Escritura["measures_phase_A/home_current"]    = Status.Measures.consumo_domestico;    
    
    Escritura["error_code"] = Status.error_code;

    //Tiempos
    Escritura["connect_ts"]           = Status.Time.connect_date_time;
    Escritura["disconnect_date_time"] = Status.Time.disconnect_date_time;
    Escritura["charge_start_time"]    = Status.Time.charge_start_time;
    Escritura["charge_stop_time"]     = Status.Time.charge_stop_time;
    
    if(Database.RTDB.Send_Command(Path,&Escritura,UPDATE,Database.Auth.idToken)){     
      //Write readed Timestamp
      if(Database.RTDB.Send_Command(Path+"ts_dev_ack/",&Lectura,TIMESTAMP,Database.Auth.idToken)){
        response = true;
      }
    }
    xSemaphoreGive(firebase_Access);
  }
  return false;
}


bool WriteFirebaseParams(String Path){

  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10) )){
    Serial.println("Write Params CALLED");
    Escritura.clear();
    Escritura["Ubic_sensor"]            = Params.Ubicacion_Sensor;
    Escritura["Inst_current_limit"]     = Params.inst_current_limit;
    Escritura["Potencia_Contratada"]    = Params.potencia_contratada;
    Escritura["CDP_On"]                 = Params.CDP_On;
    Escritura["Autentication_mode"]     = String(Params.autentication_mode).substring(0,2);
    Escritura["Fw_Update_mode"]         = String(Params.Fw_Update_mode).substring(0,2);
    
    serializeJson(Escritura,Serial);
    if(Database.RTDB.Send_Command(Path,&Escritura,UPDATE,Database.Auth.idToken)){     
      Path += "/Timestamp";
      if(Database.RTDB.Send_Command(Path,&Escritura,TIMESTAMP,Database.Auth.idToken)){
        xSemaphoreGive(firebase_Access);
        return true;
      }
      xSemaphoreGive(firebase_Access);
      return false;
    }
    xSemaphoreGive(firebase_Access);
    return false;
  }
  return false;
}

bool WriteFirebaseComs(String Path){

  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10) )){
    Serial.println("Write Coms CALLED");
    Escritura.clear();
    #ifdef USE_WIFI
      Escritura["WIFI/ON"]        = Coms.Wifi.ON;
      Escritura["WIFI/AP"]        = Coms.Wifi.AP;
      Escritura["WIFI/Password"]  = Coms.Wifi.Pass;
    #endif

    #ifdef USE_ETH
      Escritura["ETH/ON"]   = Coms.ETH.ON;
      Escritura["ETH/AUTO"] = Coms.ETH.Auto;
      if(!Coms.ETH.Auto){
        Escritura["ETH/IP1"]   = Coms.ETH.IP1;
        Escritura["ETH/IP2"]   = Coms.ETH.IP2;
        Escritura["ETH/Gateway"]   = Coms.ETH.Gateway;
        Escritura["ETH/Mask"]   = Coms.ETH.Mask;
        Escritura["ETH/AP"]   = Coms.ETH.AP;
        Escritura["ETH/Pass"]   = Coms.ETH.Pass;
      }
    #endif

    #ifdef USE_GSM
      Comms_Json.set("GSM/ON",Coms.GSM.ON);
      Comms_Json.set("GSM/APN",Coms.GSM.APN);
      Comms_Json.set("GSM/Pass",Coms.GSM.Pass);
    #endif
   
    if(Database.RTDB.Send_Command(Path,&Escritura,UPDATE,Database.Auth.idToken)){     
      Path += "/Timestamp";
      if(Database.RTDB.Send_Command(Path,&Escritura,TIMESTAMP,Database.Auth.idToken)){
        xSemaphoreGive(firebase_Access);
        return true;
      }
      xSemaphoreGive(firebase_Access);
      return false;
    }
    xSemaphoreGive(firebase_Access);
    return false;
  }
  return false;
}

/***************************************************
  Funciones de lectura
***************************************************/
bool ReadFirebaseControl(String Path){
  Lectura.clear();
  bool response = false;
  if(xSemaphoreTake(firebase_Access,  pdMS_TO_TICKS(10))){
    if(Database.RTDB.Send_Command(Path,&Lectura, READ, Database.Auth.idToken)){
      Comands.start           = Lectura["start"]     ? true : Comands.start;
      Comands.stop            = Lectura["stop"]      ? true : Comands.stop;
      Comands.reset           = Lectura["reset"]     ? true : Comands.reset;
      Comands.desired_current = Lectura["desired_current"];     
      Comands.fw_update       = Lectura["fw_update"] ? true : Comands.fw_update;
      Comands.conn_lock       = Lectura["conn_lock"] ? true : Comands.conn_lock;

      response = true;
      if(Lectura["start"] || Lectura["stop"] || Lectura["reset"] || Lectura["fw_update"]){
        Lectura["fw_update"]=false;
        Lectura["reset"]=false;
        Lectura["start"]=false;
        Lectura["stop"]=false;
        serializeJson(Lectura,Serial);
        response = true;
        if(!Database.RTDB.Send_Command(Path,&Lectura,UPDATE,Database.Auth.idToken)){
          response = false;
        } 
      }
      //Write readed Timestamp
      Database.RTDB.Send_Command(Path+"ts_dev_ack/",&Lectura,TIMESTAMP,Database.Auth.idToken);
    }
    xSemaphoreGive(firebase_Access);
  } 
  return response;
}

/*****************************************************
 *              Sistema de Actualización
 *****************************************************/
bool CheckForUpdate(){

  if(Database.RTDB.Send_Command("/prod/fw/beta/",&Actualizacion, READ, Database.Auth.idToken)){
    String ESP_Ver   = Actualizacion["VBLE2"]["verstr"];
    Serial.println(ESP_Ver);
    uint16 ESP_int_Version=ParseFirmwareVersion(ESP_Ver);

    if(ESP_int_Version>UpdateStatus.ESP_Act_Ver){
      Serial.println("Updating ESP!");
      UpdateStatus.ESP_UpdateAvailable= true;
      url = Actualizacion["VBLE2"]["url"].as<String>();
      return true;
    }

    String PSOC5_Ver   = Actualizacion["VELT2"]["verstr"];
    uint16 VELT_int_Version=ParseFirmwareVersion(PSOC5_Ver);

    if(VELT_int_Version>UpdateStatus.PSOC5_Act_Ver){
      Serial.println("Updating PSOC5!");
      UpdateStatus.PSOC5_UpdateAvailable= true;
      url = Actualizacion["VELT2"]["url"].as<String>();
      return true;
    }    
    return false;
  }

  return false;
}

void DownloadFileTask(void *args){
  UpdateStatus.DescargandoArchivo=true;

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


  HTTPClient DownloadClient;
  DownloadClient.begin(url);
  WiFiClient *stream = new WiFiClient();
  int total = 0;
  if(DownloadClient.GET()>0){
    Serial.println(url);
    total = DownloadClient.getSize();
    int len = total;
    int written=0;
    stream = DownloadClient.getStreamPtr();
    uint8_t buff[1024] = { 0 };

    if(UpdateStatus.ESP_UpdateAvailable){
      if(!Update.begin(total)){
        Serial.println("Error with size");
      };
    }


    while (DownloadClient.connected() &&(len > 0 || len == -1)){
      size_t size = stream->available();
      if (size){

        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

        if(UpdateStatus.PSOC5_UpdateAvailable){
          int escritos = UpdateFile.write(buff, c);
        }
        else{
          Update.write(buff,c);
        } 
        
        if (len > 0){
          written +=c;
          len -= c;
        }
        Serial.printf("%i vs %i \r", written, total);
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  if(UpdateStatus.ESP_UpdateAvailable){
    if(Update.end()){	
      Serial.println("Micro Actualizado!"); 
      //reboot
      MAIN_RESET_Write(0);
      ESP.restart();
    }
  }

  stream->stop();
  DownloadClient.end(); 
  delete stream;

  Serial.println(UpdateFile.size());
  Serial.println(total);
  if(UpdateFile.size()==total){
    UpdateFile.close();
    setMainFwUpdateActive(1);
  }

  UpdateStatus.DescargandoArchivo=false;
  vTaskDelete(NULL);
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
      if(ConfigFirebase.InternetConection && !ConfigFirebase.StopSistem){
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
      if(!ConfigFirebase.InternetConection || Error_Count>10 || ConfigFirebase.StopSistem){
        ConnectionState=DISCONNECTING;
        break;
      }

      //Si está conectado por ble no hace nada
      else if(serverbleGetConnected()){
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
      else if(Comands.fw_update){
        Serial.println("Starting update sistem");
        Comands.fw_update=0;
        ConnectionState = UPDATING;
        break;
      }


      ConnectionState=READING_CONTROL;
      break;

    /*********************** WRITTING states **********************/
    case WRITTING_CONTROL:
      //Error_Count+=!WriteFirebaseControl(Base_Path+"/control/");
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
      Error_Count+=WriteFirebaseParams(Base_Path+"/params/");
      ConnectionState=IDLE;
      break;
    /*********************** READING states **********************/
    case READING_CONTROL:

      Error_Count+=!ReadFirebaseControl(Base_Path+"/control/");
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
          xTaskCreate(DownloadFileTask,"DOWNLOAD FILE", 4096*2, NULL, 1,NULL);
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
      if(!UpdateStatus.DescargandoArchivo){
        ConnectionState=INSTALLING;
      }  
      break;

    case INSTALLING:
      //Do nothing
      break;

    /*********************** DISCONNECT states **********************/
    case DISCONNECTING:

      ConnectionState=DISCONNECTED;
      
      break;
    default:
      while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
        Serial.printf("Error en la maquina d estados de firebase: %i \n", ConnectionState);
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}