#include "VeltFirebase.h"

Real_Time_Database *Database = new Real_Time_Database();

StaticJsonDocument<2048>  Lectura        EXT_RAM_ATTR;
StaticJsonDocument<2048>  Escritura      EXT_RAM_ATTR;

//Extern variables
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Update_Status          UpdateStatus;
extern carac_Params                 Params;
extern carac_Coms                   Coms;
extern carac_group                  ChargingGroup;

extern uint8_t ConnectionState;


void DownloadTask(void *arg);
void store_group_in_mem(carac_chargers* group);
void coap_put( char* Topic, char* Message);
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);


uint16 ParseFirmwareVersion(String Texto){
  String sub = Texto.substring(6, 10); 
  return(sub.toInt());
}

/*************************
 Client control functions
*************************/
bool initFirebaseClient(){

    Database->deviceID = ConfigFirebase.Device_Id;
    if(!Database->LogIn()){
      return false;
    }
    #ifdef DEVELOPMENT
    String project = FIREBASE_DEV_PROJECT;
    #else
    String project = FIREBASE_PROJECT;
    #endif
    project += "/";
    Database->begin(project, ConfigFirebase.Device_Db_ID);
    
    return true;

}

uint8_t getfirebaseClientStatus(){
  return ConfigFirebase.FirebaseConnected;
}

/***************************************************
  Funciones de escritura
***************************************************/
bool WriteFirebaseStatus(String Path){

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
  Escritura["measures_phase_A/home_current"]    = Status.Measures.consumo_domestico;    
  Escritura["measures_phase_A/active_energy"]   = Status.Measures.active_energy;    

  if(Status.Trifasico){
    Escritura["measures_phase_B/inst_current"]    = Status.MeasuresB.instant_current;
    Escritura["measures_phase_B/inst_voltage"]    = Status.MeasuresB.instant_voltage;
    Escritura["measures_phase_B/active_power"]    = Status.MeasuresB.active_power;
    Escritura["measures_phase_B/home_current"]    = Status.MeasuresB.consumo_domestico; 
    Escritura["measures_phase_B/active_energy"]   = Status.MeasuresB.active_energy; 

    Escritura["measures_phase_C/inst_current"]    = Status.MeasuresC.instant_current;
    Escritura["measures_phase_C/inst_voltage"]    = Status.MeasuresC.instant_voltage;
    Escritura["measures_phase_C/active_power"]    = Status.MeasuresC.active_power;
    Escritura["measures_phase_C/home_current"]    = Status.MeasuresC.consumo_domestico;  
    Escritura["measures_phase_C/active_energy"]   = Status.MeasuresC.active_energy;   
  }
  
  Escritura["error_code"] = Status.error_code;

  if(Database->Send_Command(Path,&Escritura,UPDATE)){     
    if(Database->Send_Command(Path+"/ts_dev_ack",&Lectura,TIMESTAMP)){
      return true;
    }
  }
  return false;
}

bool WriteFirebaseTimes(String Path){

  Escritura.clear();

  //Tiempos
  Escritura["connect_ts"]         = Status.Time.connect_date_time * 1000;
  Escritura["disconnect_ts"]      = Status.Time.disconnect_date_time * 1000;
  Escritura["charge_start_ts"]    = Status.Time.charge_start_time * 1000;
  Escritura["charge_stop_ts"]     = Status.Time.charge_stop_time * 1000;

  if(Database->Send_Command(Path,&Escritura,UPDATE)){     
    return true;
  }
  return false;
}

bool WriteFirebaseParams(String Path){

  Serial.println("Write Params CALLED");
  Escritura.clear();
  Escritura["auth_mode"]           = String(Params.autentication_mode).substring(0,2);
  Escritura["inst_curr_limit"]      = Params.inst_current_limit;
  Escritura["contract_power"]      = Params.potencia_contratada;
  //Escritura["dps_placement"]       = Params.Ubicacion_Sensor;
  //Escritura["dpc_on"]              = Params.CDP_On;
  Escritura["dpc" ]                = Params.CDP;
  Escritura["fw_auto"]             = String(Params.Fw_Update_mode).substring(0,2);
  
  if(Database->Send_Command(Path,&Escritura,UPDATE)){     
    //Write readed Timestamp
    if(Database->Send_Command(Path+"/ts_dev_ack",&Escritura,TIMESTAMP)){
      return true;
    }
  }

  return false;
}

bool WriteFirebaseComs(String Path){

  Escritura.clear();
      Escritura["wifi/on"]     = Coms.Wifi.ON;
      Escritura["wifi/ssid"]   = Coms.Wifi.AP;
      Escritura["eth/on"]      = Coms.ETH.ON;
      Escritura["eth/auto"]    = Coms.ETH.Auto;

    if(Coms.ETH.Auto){
      Escritura["eth/ip"]      = ip4addr_ntoa(&Coms.ETH.IP);
      Escritura["eth/gateway"]  = ip4addr_ntoa(&Coms.ETH.Gateway);
      Escritura["eth/mask"]     = ip4addr_ntoa(&Coms.ETH.Mask);
    }

  #ifdef USE_GSM
    Comms_Json.set("modem/apn",Coms.GSM.APN);
    Comms_Json.set("modem/passwd",Coms.GSM.Pass);
  #endif
  
  if(Database->Send_Command(Path,&Escritura,UPDATE)){     
    if(Database->Send_Command(Path+"/ts_dev_ack",&Escritura,TIMESTAMP)){
      return true;
    }
  }

  return false;
}

bool WriteFirebaseControl(String Path){
  Escritura.clear();

  Escritura["fw_update"] = false;
  Escritura["reset"]     = false;
  Escritura["start"]     = false;
  Escritura["stop"]      = false;
  if(Database->Send_Command(Path,&Escritura,UPDATE)){
    return true;
  }

  return false;
}

bool WriteFirebaseFW(String Path){
  Escritura.clear();

  if(UpdateStatus.ESP_Act_Ver < 1000){
    Escritura["VBLE2"] = "VBLE2_0"+String(UpdateStatus.ESP_Act_Ver);
    Escritura["VELT2"] = "VELT2_0"+String(UpdateStatus.PSOC5_Act_Ver);
  }
  else{
    Escritura["VBLE2"] = "VBLE2_"+String(UpdateStatus.ESP_Act_Ver);
    Escritura["VELT2"] = "VELT2_"+String(UpdateStatus.PSOC5_Act_Ver);
  }


  if(Database->Send_Command(Path,&Escritura,UPDATE)){
    return true;
  }

  return false;
}

bool WriteFirebasegroups(String Path){
    Escritura.clear();

    Escritura["delete"] = false;

    if(Database->Send_Command(Path+"/params",&Escritura,UPDATE)){
      return true;
    }

    return false;
}

bool WriteFirebaseHistoric(char* buffer){
  if(ConnectionState == IDLE){
    String Path = "/records/";
    Escritura.clear();
    long conTime=123456789; 

    Escritura["act"] = "123";
    Escritura["con"] = "123";
    Escritura["dis"] = "123";
    Escritura["rea"] = "123";
    Escritura["sta"] = "123";
    Escritura["usr"] = "123";

    char buf[10];
    ltoa(conTime, buf, 10); 
    printf("Escribiendo historico\n");
    if(Database->Send_Command(Path+buf,&Escritura,UPDATE)){
      return true;
    }
  }
  
  return false;
}
/***************************************************
  Funciones de lectura
***************************************************/
bool ReadFirebaseGroups(String Path){

  long long ts_app_req=Database->Get_Timestamp(Path+"/ts_app_req",&Lectura, true);
  if(ts_app_req > ChargingGroup.last_ts_app_req){
    Lectura.clear();
    //Leer los parametros del grupo
    if(Database->Send_Command(Path+"/params",&Lectura, LEER, true)){
      ChargingGroup.last_ts_app_req=ts_app_req;
    
      ChargingGroup.Params.GroupActive = Lectura["active"].as<uint8_t>();
      ChargingGroup.Params.inst_max = Lectura["install_limit"].as<uint8_t>();
      ChargingGroup.Params.CDP = Lectura["cdp"].as<uint8_t>();
      ChargingGroup.Params.ContractPower = Lectura["p_con"].as<uint8_t>();
      ChargingGroup.Params.potencia_max = Lectura["p_max"].as<uint8_t>();
      ChargingGroup.DeleteOrder  = Lectura["delete"].as<bool>();

      ChargingGroup.SendNewParams = true;
      delay(250);
      
      if(ChargingGroup.Params.GroupActive){
        //Leer los equipos del grupo
        Lectura.clear();
        if(Database->Send_Command(Path+"/devices",&Lectura, LEER, true)){ 
          JsonObject root = Lectura.as<JsonObject>();
          ChargingGroup.group_chargers.size = 0;
          //int index =0;
          for (JsonPair kv : root) {
            add_to_group(kv.key().c_str(),IPADDR_ANY, &ChargingGroup.group_chargers);
            ChargingGroup.group_chargers.charger_table[ChargingGroup.group_chargers.size-1].Fase = Lectura[kv.key().c_str()]["phase"].as<uint8_t>();
            /*memcpy(ChargingGroup.group_chargers.charger_table[index].name, kv.key().c_str(),8);
            ChargingGroup.group_chargers.charger_table[index].Fase = Lectura[kv.key().c_str()]["phase"].as<uint8_t>();
            index ++;*/
          }
          /*ChargingGroup.group_chargers.size = index;*/
          store_group_in_mem(&ChargingGroup.group_chargers);

          //ChargingGroup.SendNewGroup = true;
        }
        print_table(ChargingGroup.group_chargers, "Grupo desde firebase");
      }    

      else{
          if(ChargingGroup.Conected){
            char buffer[20];
            memcpy(buffer,"Pause",5);
            coap_put("CONTROL", buffer);
          }
      }
      if(ChargingGroup.DeleteOrder){
          WriteFirebasegroups(Path);
          if(ChargingGroup.Conected){
            char buffer[20];
            memcpy(buffer,"Delete",6);
            coap_put("CONTROL", buffer);
         }
      }
      
      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,TIMESTAMP, true)){
          return false;
      } 
    }
  }
  return true;

}

bool ReadFirebaseComs(String Path){

  long long ts_app_req=Database->Get_Timestamp(Path+"/ts_app_req",&Lectura);
  if(ts_app_req > Coms.last_ts_app_req){
    Lectura.clear();
    if(Database->Send_Command(Path,&Lectura, LEER)){
      Coms.last_ts_app_req=ts_app_req;

      Coms.Wifi.ON   = Lectura["wifi"]["on"];

      Coms.ETH.ON   = Lectura["eth"]["on"];
      Coms.ETH.Auto = Lectura["eth"]["auto"];
      
      #ifdef USE_GSM
        Coms.GSM.ON    = Lectura["modem"]["on"];
        Coms.GSM.APN   = Lectura["modem"]["apn"];
        Coms.GSM.Pass  = Lectura["modem"]["passwd"];
      #endif
      //Store coms in psoc5 flash memory
      //SendToPSOC5(COMS_CONFIGURATION_CHAR_HANDLE);

      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,TIMESTAMP)){
          return false;
      } 
    }
  }
  return true;
}

bool ReadFirebaseParams(String Path){
  
  long long ts_app_req=Database->Get_Timestamp(Path+"/ts_app_req",&Lectura);
  if(ts_app_req > Params.last_ts_app_req){
    Lectura.clear();
    if(Database->Send_Command(Path,&Lectura, LEER)){
      
      Params.last_ts_app_req=ts_app_req;

      if(memcmp(Params.autentication_mode,Lectura["auth_mode"].as<String>().c_str(),2)){
        memcpy(Params.autentication_mode, Lectura["auth_mode"].as<String>().c_str(),2);
        SendToPSOC5(Params.autentication_mode,2,CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
      }     
      
      if(memcmp(Params.Fw_Update_mode, Lectura["fw_auto"].as<String>().c_str(),2)!=0){
        Serial.println("Nuevo modo de autenticacion detectado!");
        memcpy(Params.Fw_Update_mode, Lectura["fw_auto"].as<String>().c_str(),2);
        SendToPSOC5(Params.Fw_Update_mode, 2, COMS_FW_UPDATEMODE_CHAR_HANDLE);
      }

      
      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,TIMESTAMP)){
          return false;
      } 
    }
    else return false;  
  }

  return true;
}

bool ReadFirebaseControl(String Path){
  
  long long ts_app_req=Database->Get_Timestamp(Path+"/ts_app_req",&Lectura);
  if(ts_app_req> Comands.last_ts_app_req){
    Lectura.clear();
    if(Database->Send_Command(Path,&Lectura, LEER)){
    
      Comands.last_ts_app_req = ts_app_req;
      Comands.start           = Lectura["start"]     ? true : Comands.start;
      Comands.stop            = Lectura["stop"]      ? true : Comands.stop;
      Comands.reset           = Lectura["reset"]     ? true : Comands.reset;

      if(Comands.desired_current != Lectura["desired_current"] && !Comands.Newdata){   
        Comands.desired_current = Lectura["desired_current"];
        Comands.Newdata = true;
      }
            
      Comands.fw_update       = Lectura["fw_update"] ? true : Comands.fw_update;
      Comands.conn_lock       = Lectura["conn_lock"] ? true : Comands.conn_lock;

      WriteFirebaseControl("/control");
      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,TIMESTAMP)){
          return false;
      } 
    }
    else return false;  
  }
   
  return true;
}

/*****************************************************
 *              Sistema de Actualización
 *****************************************************/
bool CheckForUpdate(){
  bool update= false;
  //Check Permisions
  UpdateStatus.BetaPermission = Database->checkPermisions();
  //Check Beta Firmware
  if(UpdateStatus.BetaPermission){
    if(Database->Send_Command("/prod/fw/beta/",&Lectura, READ_FW)){
      String PSOC5_Ver   = Lectura["VELT2"]["verstr"].as<String>();
      uint16 VELT_int_Version=ParseFirmwareVersion(PSOC5_Ver);

      if(VELT_int_Version>UpdateStatus.PSOC5_Act_Ver){
        Serial.print("Actualizando PSOC5 a version beta");
        Serial.println(PSOC5_Ver);
        UpdateStatus.PSOC5_UpdateAvailable= true;
        UpdateStatus.PSOC_url = Lectura["VELT2"]["url"].as<String>();
        update = true;
      }    

      String ESP_Ver   = Lectura["VBLE2"]["verstr"].as<String>();
      uint16 ESP_int_Version=ParseFirmwareVersion(ESP_Ver);

      if(ESP_int_Version>UpdateStatus.ESP_Act_Ver){
        Serial.print("Actualizando ESP32 a version beta");
        Serial.println(ESP_Ver);
        UpdateStatus.ESP_UpdateAvailable= true;
        UpdateStatus.ESP_url = Lectura["VBLE2"]["url"].as<String>();
        update = true;
      }  
    }
  }
  

  //Check Normal Firmware
  if(Database->Send_Command("/prod/fw/prod/",&Lectura, READ_FW)){
    String PSOC5_Ver   = Lectura["VELT2"]["verstr"].as<String>();
    Serial.println(PSOC5_Ver);
    Serial.println(UpdateStatus.PSOC5_Act_Ver);
    uint16 VELT_int_Version=ParseFirmwareVersion(PSOC5_Ver);

    if(VELT_int_Version>UpdateStatus.PSOC5_Act_Ver){
      Serial.print("Actualizando PSOC5 a version ");
      Serial.println(PSOC5_Ver);
      UpdateStatus.PSOC5_UpdateAvailable= true;
      UpdateStatus.PSOC_url = Lectura["VELT2"]["url"].as<String>();
      update = true;
    }    

    String ESP_Ver   = Lectura["VBLE2"]["verstr"].as<String>();
    uint16 ESP_int_Version=ParseFirmwareVersion(ESP_Ver);

    if(ESP_int_Version>UpdateStatus.ESP_Act_Ver){
      Serial.print("Actualizando ESP32 a version ");
      Serial.println(ESP_Ver);
      UpdateStatus.ESP_UpdateAvailable= true;
      UpdateStatus.ESP_url = Lectura["VBLE2"]["url"].as<String>();
      update = true;
    }  
  }
  return update;
}

void DownloadFileTask(void *args){
  UpdateStatus.DescargandoArchivo=true;

  File UpdateFile;
  String FileName;
  String url;
  //Si descargamos actualizacion para el PSOC5, debemos crear el archivo en el SPIFFS
  if(UpdateStatus.PSOC5_UpdateAvailable){
    url=UpdateStatus.PSOC_url;
    SPIFFS.begin(0,"/spiffs",1,"PSOC5");
    FileName="/FreeRTOS_V6.cyacd";
    if(SPIFFS.exists(FileName)){
      vTaskDelay(pdMS_TO_TICKS(50));
      SPIFFS.format();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    UpdateFile = SPIFFS.open(FileName, FILE_WRITE);
  }
  else{
    url=UpdateStatus.ESP_url;
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

    if(UpdateStatus.ESP_UpdateAvailable && !UpdateStatus.PSOC5_UpdateAvailable){
      if(!Update.begin(total)){
        Serial.println("Error with size");
      };
    }

    Serial.println("Descargando firmware...");
    while (DownloadClient.connected() &&(len > 0 || len == -1)){
      size_t size = stream->available();
      if (size){

        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

        if(UpdateStatus.PSOC5_UpdateAvailable){
          UpdateFile.write(buff, c);
        }
        else{
          Update.write(buff,c);
        } 
        
        if (len > 0){
          written +=c;
          len -= c;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  if(UpdateStatus.ESP_UpdateAvailable && !UpdateStatus.PSOC5_UpdateAvailable){
    if(Update.end()){	
      Serial.println("Micro Actualizado!"); 
      //reboot
      MAIN_RESET_Write(0);
      ESP.restart();
    }
  }
  //Aqui solo lleg asi se está actualizando el PSOC5

  stream->stop();
  DownloadClient.end(); 
  delete stream;

  Serial.println("Firmware de PSOC5 descargado!");
  UpdateFile.close();
  setMainFwUpdateActive(1);

  UpdateStatus.DescargandoArchivo=false;
  vTaskDelete(NULL);
}

/***************************************************
 Tarea de control de firebase
***************************************************/

void Firebase_Conn_Task(void *args){
  ConnectionState =  DISCONNECTED;
  uint8_t  LastStatus = DISCONNECTED, NextState = 0;
  uint8_t Error_Count = 0, null_count=0;
  //timeouts para leer parametros y coms (Para leer menos a menudo)
  uint8 Params_Coms_Timeout =0;
  String Base_Path;
  long long ts_app_req =0;
  TickType_t xStarted = 0, xElapsed = 0; 
  int UpdateCheckTimeout=0;

  while(1){
    if(!ConfigFirebase.InternetConection && ConnectionState!= DISCONNECTED){
      ConnectionState=DISCONNECTING;
    }
    switch (ConnectionState){
    //Comprobar estado de la red
    case DISCONNECTED:{
      delete Database;
      Database = new Real_Time_Database();

      if(ConfigFirebase.InternetConection){
        Error_Count=0;
        Base_Path="prod/devices/";
        ConnectionState=CONNECTING;
      }
      break;
    }

    case CONNECTING:
      if(initFirebaseClient()){
        ConfigFirebase.FirebaseConnected=1;
        Base_Path += (char *)ConfigFirebase.Device_Db_ID;
        ConnectionState=CONECTADO;
      }
      break;
    
    case CONECTADO:
      //Inicializar los timeouts
      ChargingGroup.last_ts_app_req = Database->Get_Timestamp("123456789/ts_app_req",&Lectura, true);
      ChargingGroup.last_ts_app_req = Database->Get_Timestamp("123456789/ts_app_req",&Lectura, true);
      Params.last_ts_app_req  = Database->Get_Timestamp("/params/ts_app_req",&Lectura);
      Comands.last_ts_app_req = Database->Get_Timestamp("/control/ts_app_req",&Lectura);
      Coms.last_ts_app_req    = Database->Get_Timestamp("/coms/ts_app_req",&Lectura);
      Status.last_ts_app_req  = Database->Get_Timestamp("/status/ts_app_req",&Lectura);
      
      Error_Count+=!WriteFirebaseFW("/fw/current");
      ConnectionState=IDLE;
      break;

    case IDLE:
    
      //No connection == Disconnect
      //Error_count > 10 == Disconnect
      if(!ConfigFirebase.InternetConection || Error_Count>10){
        ConnectionState=DISCONNECTING;
        break;
      }

      //Si está conectado por ble no hace nada
      else if(serverbleGetConnected()){
        ConfigFirebase.ClientConnected = false;
        break;
      }

      //comprobar si hay usuarios observando:    
      ts_app_req=Database->Get_Timestamp("/status/ts_app_req",&Lectura);
      if(ts_app_req < 1){//connection refused o autenticacion terminada, comprobar respuesta
        String ResponseString = Lectura["error"];
        if(strcmp(ResponseString.c_str(),"Auth token is expired") == 0){
            Serial.println("Autorizacion expirada, solicitando una nueva");
            if(Database->LogIn()){
              break;
            }
            ConnectionState=DISCONNECTING;
        }
        else{
          if(++null_count>=3){
              ConnectionState=DISCONNECTING;
          }
        }
        break; 
      }
      else{
        null_count=0;
      }

      //Comprobar actualizaciones automaticas
      if(++UpdateCheckTimeout>8575){ // Unas 12 horas
        if(!memcmp(Status.HPT_status, "A1",2) || !memcmp(Status.HPT_status, "0V",2)){
          UpdateCheckTimeout=0;
          if(!memcmp(Params.Fw_Update_mode, "AA",2)){
            Serial.println("Comprobando firmware nuevo");
            ConnectionState = UPDATING;
            break;
          }
        }
      }
      
      if(ts_app_req > Status.last_ts_app_req){
        Status.last_ts_app_req= ts_app_req;
        ConfigFirebase.ClientConnected  = true;
        xStarted = xTaskGetTickCount();
        Serial.println("User connected!");
        ConnectionState = WRITTING_STATUS;
        NextState = WRITTING_COMS;
        break;
      }
 
      if(ConfigFirebase.ClientConnected){
        xElapsed=xTaskGetTickCount()-xStarted;
        if(pdTICKS_TO_MS(xElapsed)>60000){
          ConfigFirebase.ClientConnected  = false;
        }
        if(NextState!=0){
          ConnectionState = NextState;
          NextState = 0;
          break;
        }

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

        else if(ConfigFirebase.WriteControl){
          ConfigFirebase.WriteControl=0;
          ConnectionState = WRITTING_CONTROL;
          break;
        }
        else if(ConfigFirebase.WriteTime){
          ConfigFirebase.WriteTime=0;
          ConnectionState = WRITTING_TIMES;
          break;
        }
        else if(++Params_Coms_Timeout>=5){
          ConnectionState = READING_PARAMS;
          Params_Coms_Timeout = 0;
          break; 
        }

        //Comprobar actualizaciones
        else if(Comands.fw_update){
          if(!memcmp(Status.HPT_status, "A1",2) || !memcmp(Status.HPT_status, "0V",2) ){
            UpdateCheckTimeout=0;
            Comands.fw_update=0;
            ConnectionState = UPDATING;
            break;
          }
        }
        

        //Mientras hay un cliente conectado y nada que hacer, miramos control
        ConnectionState=READING_CONTROL;
      }
      
      break;

    /*********************** WRITTING states **********************/
    case WRITTING_CONTROL:
      Error_Count+=!WriteFirebaseControl("/control");
      ConnectionState=IDLE;
      break;

    case WRITTING_STATUS:
      Error_Count+=!WriteFirebaseStatus("/status");
      ConnectionState=IDLE;
      break;

    case WRITTING_COMS:
      Error_Count+=!WriteFirebaseComs("/coms");
      ConnectionState=IDLE;
      break;
    
    case WRITTING_PARAMS:
      Error_Count+=!WriteFirebaseParams("/params");
      ConnectionState=IDLE;
      break;

    case WRITTING_TIMES:
      Error_Count+=!WriteFirebaseTimes("/status/times");
      ConnectionState=IDLE;
      break;
    /*********************** READING states **********************/
    case READING_CONTROL:
      Error_Count+=!ReadFirebaseControl("/control");
      ConnectionState=IDLE;
      break;

    case READING_PARAMS:
      Error_Count+=!ReadFirebaseParams("/params");
      ConnectionState=IDLE;
      NextState=READING_GROUP;
      break;
    
    //Está preparado pero nunca lee las comunicaciones de firebase
    case READING_COMS: 
      Error_Count+=!ReadFirebaseComs("/coms");
      ConnectionState=IDLE;
      NextState=READING_PARAMS;
      break;

    case READING_GROUP:
      Error_Count+=!ReadFirebaseGroups("123456789");
      ConnectionState=IDLE;
      NextState=WRITTING_STATUS;
      break;

    /*********************** UPDATING states **********************/
    case UPDATING:
      Serial.println("Comprobando firmware nuevo");
      if(CheckForUpdate()){
        xTaskCreate(DownloadFileTask,"DOWNLOAD FILE", 4096*2, NULL, 1,NULL);
        ConnectionState=DOWNLOADING;
      }
      else{
        Serial.println("No hay software nuevo disponible");
        ConnectionState=IDLE;
      }
      break;

    case DOWNLOADING:
      if(!UpdateStatus.DescargandoArchivo){
        ConnectionState=INSTALLING;
      }  
      break;

    case INSTALLING:
      if(UpdateStatus.DobleUpdate){
        xTaskCreate(DownloadFileTask,"DOWNLOAD FILE", 4096*2, NULL, 1,NULL);
        ConnectionState=DOWNLOADING;
      }
      //Do nothing
      break;

    /*********************** DISCONNECT states **********************/
    case DISCONNECTING:
      Database->end();
      ConnectionState=DISCONNECTED;
      break;
    default:
      while(1){
        delay(1000);
        Serial.printf("Error en la maquina d estados de firebase: %i \n", ConnectionState);
      }
      break;
    }
    if(LastStatus!= ConnectionState){
      Serial.printf("Maquina de estados de Firebase de % i a %i \n", LastStatus, ConnectionState);
      LastStatus= ConnectionState;
    }
    
    if(ConnectionState!=DISCONNECTING){
      delay(ConfigFirebase.ClientConnected ? 500:5000);
    }

    //chivatos de la ram
    if(ESP.getFreePsram() < 3800000 || ESP.getFreeHeap() < 20000){
        Serial.println(ESP.getFreePsram());
        Serial.println(ESP.getFreeHeap());
    }
  }
}

