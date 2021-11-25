#include "VeltFirebase.h"
#ifdef CONNECTED
#include "base64.h"

#include "libb64/cdecode.h"

Real_Time_Database *Database = new Real_Time_Database();

DynamicJsonDocument Lectura(2048);

//Extern variables
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Update_Status          UpdateStatus;
extern carac_Params                 Params;
extern carac_Coms                   Coms;
extern carac_Schedule               Schedule;

extern uint8 user_index;

#ifdef USE_GROUPS
extern carac_group                  ChargingGroup;
extern carac_circuito               Circuitos[MAX_GROUP_SIZE];
extern carac_charger                charger_table[ MAX_GROUP_SIZE];
#endif

extern uint8_t ConnectionState;

//Declaracion de funciones locales
void DownloadTask(void *arg);
void store_group_in_mem(carac_charger* group, uint8_t size);
void coap_put( char* Topic, char* Message);
bool add_to_group(const char* ID, IPAddress IP, carac_charger* group, uint8_t *size);
uint8_t check_in_group(const char* ID, carac_charger* group, uint8_t size);
uint8_t CreateMatrix(uint16_t* inBuff, uint8_t* outBufff);
uint8_t hex2int(char ch);
uint16_t hexChar_To_uint16_t(const uint8_t num);
void Hex_Array_To_Uint16A_Array(const char* inBuff, uint16_t* outBuff);

bool askForPermission = false ;
bool givePermission = false;

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
  DynamicJsonDocument Escritura(2048);
  Escritura.clear();
  if(memcmp(&Status.HPT_status[0],"F",1) && memcmp(&Status.HPT_status[0],"E",1) ){
    Escritura["hpt"]              = String(Status.HPT_status).substring(0,2);
  }
  else{
    Escritura["hpt"]              = String(Status.HPT_status).substring(0,1);
  }
  Escritura["icp"]              = Status.ICP_status;
  Escritura["dc_leak"]          = Status.DC_Leack_status;
  Escritura["conn_lock_stat"]   = Status.Con_Lock;
  Escritura["real_current_limit"]   = Status.current_limit;

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
    Escritura["measures_phase_B/active_energy"]   = Status.MeasuresB.active_energy; 

    Escritura["measures_phase_C/inst_current"]    = Status.MeasuresC.instant_current;
    Escritura["measures_phase_C/inst_voltage"]    = Status.MeasuresC.instant_voltage;
    Escritura["measures_phase_C/active_power"]    = Status.MeasuresC.active_power;
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
  DynamicJsonDocument Escritura(2048);
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
  DynamicJsonDocument Escritura(2048);
  Serial.println("Write Params CALLED");
  Escritura.clear();
  Escritura["auth_mode"]           = String(Params.autentication_mode).substring(0,2);
  Escritura["inst_curr_limit"]      = Params.inst_current_limit;
  Escritura["contract_power1"]      = Params.potencia_contratada1;
  Escritura["contract_power2"]      = Params.potencia_contratada2;
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
  DynamicJsonDocument Escritura(2048);
  Escritura.clear();
  Escritura["wifi/on"]     = Coms.Wifi.ON;
  Escritura["wifi/ssid"]   = Coms.Wifi.AP;
  Escritura["eth/on"]      = Coms.ETH.ON;
  Escritura["eth/auto"]    = Coms.ETH.Auto;

  if(Coms.ETH.Auto){
    Escritura["eth/ip"]       = ip4addr_ntoa(&Coms.ETH.IP);
    Escritura["eth/gateway"]  = ip4addr_ntoa(&Coms.ETH.Gateway);
    Escritura["eth/mask"]     = ip4addr_ntoa(&Coms.ETH.Mask);
  }

  #ifdef USE_GSM
    Comms_Json.set("modem/apn",Coms.GSM.APN);
    Comms_Json.set("modem/pass",Coms.GSM.Pass);
  #endif
  
  if(Database->Send_Command(Path,&Escritura,UPDATE)){     
    if(Database->Send_Command(Path+"/ts_dev_ack",&Escritura,TIMESTAMP)){
      return true;
    }
  }

  return false;
}

bool WriteFirebaseControl(String Path){
  DynamicJsonDocument Escritura(2048);
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
  DynamicJsonDocument Escritura(2048);
  Escritura.clear();

  if(Configuracion.data.Firmware < 1000){
    Escritura["VBLE2"] = "VBLE2_0"+String(Configuracion.data.Firmware);
    Escritura["VELT2"] = "VELT2_0"+String(Configuracion.data.FirmwarePSOC);
  }
  else{
    Escritura["VBLE2"] = "VBLE2_"+String(Configuracion.data.Firmware);
    Escritura["VELT2"] = "VELT2_"+String(Configuracion.data.FirmwarePSOC);
  }


  if(Database->Send_Command(Path,&Escritura,UPDATE)){
    return true;
  }

  return false;
}

bool WriteFirebasegroups(String Path){
    DynamicJsonDocument Escritura(2048);
    Escritura.clear();

    Escritura["delete"] = false;

    if(Database->Send_Command(Path+"/params",&Escritura,UPDATE)){
      return true;
    }

    return false;
}

bool ReiniciarMultiusuario(){
    DynamicJsonDocument Escritura(2048);
    //Indicar que el cargador esta libre en firebase
    printf("Reiniciando usuario!\n");
    Escritura.clear();
    Escritura["charg_user_id"] = 255;
    Escritura["user_index"] = 255;
    Escritura["user_type"] = 255;
    Database->Send_Command("/params/user",&Escritura,UPDATE);
    return true;
}

bool EscribirMultiusuario(){
    DynamicJsonDocument Escritura(2048);
    //Indicar que el cargador esta libre en firebase
     printf("Escribiendo usuario!\n");
    Escritura.clear();
    Escritura["charg_user_id"] = user_index;
    Database->Send_Command("/params/user",&Escritura,UPDATE);
    return true;
}

bool WriteFirebaseHistoric(char* buffer){
    DynamicJsonDocument Escritura(2048);
    struct tm t = {0};  // Initalize to all 0's
    t.tm_year = (buffer[4]!=0)?buffer[4]+100:0;  // This is year-1900, so 112 = 2012
    t.tm_mon  = (buffer[3]!=0)?buffer[3]-1:0;
    t.tm_mday = buffer[2];
    t.tm_hour = buffer[5];
    t.tm_min  = buffer[6];
    t.tm_sec  = buffer[7];
    int ConectionTS = mktime(&t);

    if(ConectionTS> Status.Time.actual_time || ConectionTS < 1417305600){
      printf("Hay algun error con las horas %i %lli \n", ConectionTS, Status.Time.actual_time);
      return true;
    }

    t = {0};  // Initalize to all 0's
    t.tm_year = (buffer[10]!=0)?buffer[10]+100:0;  // This is year-1900, so 112 = 2012
    t.tm_mon  = (buffer[9]!=0)?buffer[9]-1:0;
    t.tm_mday = buffer[8];
    t.tm_hour = buffer[11];
    t.tm_min  = buffer[12];
    t.tm_sec  = buffer[13];
    int StartTs = mktime(&t);

    if(StartTs> Status.Time.actual_time || StartTs < 1417305600){
      printf("Hay algun error con las horas 2\n");
      return true;
    }

    t = {0};  // Initalize to all 0's
    t.tm_year = (buffer[16]!=0)?buffer[16]+100:0;  // This is year-1900, so 112 = 2012
    t.tm_mon  = (buffer[15]!=0)?buffer[15]-1:0;
    t.tm_mday = buffer[14];
    t.tm_hour = buffer[17];
    t.tm_min  = buffer[18];
    t.tm_sec  = buffer[19];
    int DisconTs = mktime(&t);

    if(DisconTs < StartTs || DisconTs < 1417305600){
      printf("Hay algun error con las horas 3\n");
      return true;
    }

    int j = 20;
    int size =0;
    bool end = false;
    uint8_t* record_buffer = (uint8_t*) malloc(252);

    for(int i = 0;i<252;i+=2){		
      if(j<252){
        if(buffer[j]==255 && buffer[j+1]==255){
          end = true;
          break;
        }
        uint16_t PotLeida=buffer[j]*0x100+buffer[j+1];
        if(PotLeida > 0 && PotLeida < 5500 && !end){
          record_buffer[i]   = buffer[j];
          record_buffer[i+1]   = buffer[j+1];

          size+=2;
        }
        else{
          record_buffer[i]   = 0;
          size+=2;
        }
        j+=2;
      }
      else{
        record_buffer[i]   = 0;
        size+=2;
      }				
    }
    
  
    String Encoded = base64::encode(record_buffer,(size_t)size);

    free(record_buffer);

    int count = 0;
    askForPermission = true;
    
    while(ConnectionState != IDLE && !givePermission){
      delay(10);
      if(++count > 1000){
        return true;
      }
    }
    
    uint8 Lastconn = ConnectionState;
    ConnectionState = 11;
  
    String Path = "/records/";
    Escritura.clear();

    Escritura["act"] = Encoded;
    Escritura["con"] = ConectionTS;
    Escritura["dis"] = DisconTs;
    Escritura["rea"] = "";
    Escritura["sta"] = StartTs;
    Escritura["usr"] = buffer[1] + buffer[0]*0x100;

    char buf[10];
    ltoa(ConectionTS, buf, 10); 
    
    
    //escribir el historico nuevo
    if(Database->Send_Command(Path+buf,&Escritura,UPDATE)){
      ConnectionState = Lastconn;
      askForPermission = false;
      givePermission = false;
      return true;
    }
    else{
      printf("Fallo en el envio del registro!\n");
      Database->Send_Command(Path+buf,&Escritura,UPDATE);
      ConnectionState = Lastconn;
      askForPermission = false;
      givePermission = false;
      return false;
    }
    askForPermission = false;
    givePermission = false;
    ConnectionState = Lastconn;


  return true;
}

/***************************************************
  Funciones de lectura
***************************************************/
#ifdef NO_EJECUTAR
bool ReadFirebaseGroups(String Path){

  long long ts_app_req=Database->Get_Timestamp(Path+"/ts_app_req",&Lectura, true);
  if(ts_app_req > ChargingGroup.last_ts_app_req){
    Lectura.clear();
    //Leer los parametros del grupo
    if(Database->Send_Command(Path+"/params",&Lectura, LEER, true)){
      ChargingGroup.last_ts_app_req=ts_app_req;
    
      ChargingGroup.Params.GroupActive = Lectura["active"].as<uint8_t>();
      ChargingGroup.Params.CDP = Lectura["cdp"].as<uint8_t>();
      ChargingGroup.Params.ContractPower = Lectura["p_con"].as<uint16_t>();
      ChargingGroup.Params.potencia_max = Lectura["p_max"].as<uint16_t>();
      ChargingGroup.DeleteOrder  = Lectura["delete"].as<bool>();

      ChargingGroup.SendNewParams = true;
      delay(250);

      //Leer los circuitos del grupo
      Lectura.clear();
      if(Database->Send_Command(Path+"/circuits",&Lectura, LEER, true)){ 
        JsonObject root = Lectura.as<JsonObject>();
        ChargingGroup.Circuit_number = 0;
        for (JsonPair kv : root) {  
          Circuitos[ChargingGroup.Circuit_number].numero = atoi(kv.key().c_str());
          Circuitos[ChargingGroup.Circuit_number].limite_corriente = Lectura[kv.key().c_str()]["i_max"].as<uint8_t>();
        }
      }

      
      if(ChargingGroup.Params.GroupActive){
        //Leer los equipos del grupo
        Lectura.clear();
        if(Database->Send_Command(Path+"/devices",&Lectura, LEER, true)){ 
          JsonObject root = Lectura.as<JsonObject>();

          //crear una copia temporal para almacenar los que están cargando
          carac_charger temp_chargers[MAX_GROUP_SIZE];
          uint8_t temp_chargers_size =0;

          for(uint8_t i=0; i<ChargingGroup.Charger_number;i++){    
            if(!memcmp(charger_table[i].HPT, "C2",2)){
                memcpy(temp_chargers[temp_chargers_size].name,charger_table[i].name,9);
                temp_chargers[temp_chargers_size].Current = charger_table[i].Current;
                temp_chargers[temp_chargers_size].CurrentB = charger_table[i].CurrentB;
                temp_chargers[temp_chargers_size].CurrentC = charger_table[i].CurrentC;

                temp_chargers[temp_chargers_size].Delta = charger_table[i].Delta;
                temp_chargers[temp_chargers_size].Consigna = charger_table[i].Consigna;
                temp_chargers[temp_chargers_size].Delta_timer = charger_table[i].Delta_timer;
                temp_chargers_size ++;
            }
          }


          ChargingGroup.Charger_number = 0;
          //int index =0;
          for (JsonPair kv : root) {
            add_to_group(kv.key().c_str(),IPADDR_ANY, charger_table, &ChargingGroup.Charger_number);
            charger_table[ChargingGroup.Charger_number-1].Fase = Lectura[kv.key().c_str()]["phase"].as<uint8_t>();
            charger_table[ChargingGroup.Charger_number-1].Circuito = Lectura[kv.key().c_str()]["circuit"].as<uint8_t>();

            uint8_t index =check_in_group(kv.key().c_str(), temp_chargers, temp_chargers_size);
            if(index != 255){
              memcpy(charger_table[ChargingGroup.Charger_number-1].HPT,"C2",2);
              charger_table[ChargingGroup.Charger_number-1].Current = temp_chargers[index].Current;
              charger_table[ChargingGroup.Charger_number-1].CurrentB = temp_chargers[index].CurrentB;
              charger_table[ChargingGroup.Charger_number-1].CurrentC = temp_chargers[index].CurrentC;
              charger_table[ChargingGroup.Charger_number-1].Delta = temp_chargers[index].Delta;
              charger_table[ChargingGroup.Charger_number-1].Consigna = temp_chargers[index].Consigna;
              charger_table[ChargingGroup.Charger_number-1].Delta_timer = temp_chargers[index].Delta_timer;
            }
          }
          /*ChargingGroup.Charger_number = index;*/
          store_group_in_mem(charger_table, ChargingGroup.Charger_number);

          ChargingGroup.SendNewGroup = true;
        }
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
#endif

//Leer las programaciones que haya disponibles en firebase
bool ReadFirebaseShedule(String Path){

  long long ts_app_req=Database->Get_Timestamp(Path+"/ts_app_req",&Lectura);
  if(ts_app_req > Schedule.last_ts_app_req){
    Lectura.clear();

    if(Database->Send_Command(Path,&Lectura, LEER)){

      Schedule.num_shedules = 0;
      String programaciones[Schedule.num_shedules];

      //Obtener el numero de programaciones que hay en firebase
      for(int i = 0;i< 100;i++){
        String key = i<10? "P0":"P";
        key+=String(i);
        if(!(Lectura["programs"][key])){
          break;
        }
        programaciones[i] = Lectura["programs"][key].as<String>();
        Schedule.num_shedules++;
      }

      uint8_t plainMatrix[168]={0};
      uint8_t alguna_activa = 0;

      //Decodificar las programaciones
      for(String x : programaciones){
          char dst[6] = {0};
          uint8_t decoded = base64_decode_chars(x.c_str(),8,dst);

          //Menos de 6 bits como resultado = error
          if(decoded != 6){
              continue;
          }
          uint16_t decodedBuf[6] = {0};
          
          Hex_Array_To_Uint16A_Array(dst, decodedBuf);

          //Si nos devuelve algo es que la matriz no es valida
          if(CreateMatrix(decodedBuf, plainMatrix) == 0){
              alguna_activa = 1;
          }
      }

      SendToPSOC5((uint8_t)alguna_activa, CHARGING_INSTANT_DELAYED_CHAR_HANDLE);
      SendToPSOC5(plainMatrix, 168, SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);

      Schedule.last_ts_app_req=ts_app_req;

      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,TIMESTAMP)){
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

      if(Lectura["auth_mode"].as<String>()!="null"){
        if(memcmp(Params.autentication_mode,Lectura["auth_mode"].as<String>().c_str(),2)){
          memcpy(Params.autentication_mode, Lectura["auth_mode"].as<String>().c_str(),2);
          SendToPSOC5(Params.autentication_mode,2,CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
        }     
        
        if(memcmp(Params.Fw_Update_mode, Lectura["fw_auto"].as<String>().c_str(),2)!=0){
          memcpy(Params.Fw_Update_mode, Lectura["fw_auto"].as<String>().c_str(),2);
          SendToPSOC5(Params.Fw_Update_mode, 2, COMS_FW_UPDATEMODE_CHAR_HANDLE);
        }
      }


      
      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,TIMESTAMP)){
          return false;
      } 
    }
    else return false;  
  }

  return true;
}

bool ReadFirebasePath(String Path){
  
    Lectura.clear();
    if(Database->Send_Command(Path,&Lectura, LEER)){
      if(Lectura != NULL){
        serializeJson(Lectura,Serial);
      }
      else{
        return false;
      }
      
    }
    else return false;  

  return true;
}

bool ReadFirebaseUser(){

  Lectura.clear();
  if(Database->Send_Command("/params/user",&Lectura, LEER)){
    uint8 index = Lectura["user_index"].as<uint8>();
    uint8 type = Lectura["user_type"].as<uint8>();
    if(index!=255){
      SendToPSOC5(index,VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE);
    }
    
    if(type!=255){
      SendToPSOC5(type,VCD_NAME_USERS_USER_TYPE_CHAR_HANDLE);
    }

  }
  else return false;  


  return true;
}

bool ReadFirebaseControl(String Path){
  static long long last_ts_app_req = 0;
  long long ts_app_req=Database->Get_Timestamp(Path+"/ts_app_req",&Lectura);
  if(ts_app_req> last_ts_app_req){
    Lectura.clear();
    if(Database->Send_Command(Path,&Lectura, LEER)){

      if( Lectura["desired_current"]!=0){
        last_ts_app_req = ts_app_req;
        Comands.start           = Lectura["start"]     ? true : Comands.start;
        Comands.stop            = Lectura["stop"]      ? true : Comands.stop;
        Comands.reset           = Lectura["reset"]     ? true : Comands.reset;

        if(Comands.desired_current != Lectura["desired_current"] && !Comands.Newdata){   
          Comands.desired_current = Lectura["desired_current"];
          Comands.Newdata = true;
        }

        ConfigFirebase.ClientAuthenticated = Comands.start == true;
              
        Comands.fw_update       = Lectura["fw_update"] ? true : Comands.fw_update;
        Comands.conn_lock       = Lectura["conn_lock"] ? true : Comands.conn_lock;

        WriteFirebaseControl("/control");
        if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,TIMESTAMP)){
            return false;
        } 
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
  bool update = false;

#ifdef DEVELOPMENT
  //Check Permisions
  UpdateStatus.BetaPermission = true;//Database->checkPermisions();
  //Check Beta Firmware

  if(Database->Send_Command("/prod/fw/beta/",&Lectura, READ_FW)){
    String PSOC5_Ver   = Lectura["VELT2"]["verstr"].as<String>();
    uint16 VELT_int_Version=ParseFirmwareVersion(PSOC5_Ver);

    Serial.println(PSOC5_Ver);
    Serial.println(VELT_int_Version);

    if(VELT_int_Version>Configuracion.data.FirmwarePSOC){
      UpdateStatus.PSOC5_UpdateAvailable= true;
      UpdateStatus.PSOC_url = Lectura["VELT2"]["url"].as<String>();
      update = true;
    }    

    String ESP_Ver   = Lectura["VBLE2"]["verstr"].as<String>();
    uint16 ESP_int_Version=ParseFirmwareVersion(ESP_Ver);

    if(ESP_int_Version>Configuracion.data.Firmware){
      UpdateStatus.ESP_UpdateAvailable= true;
      UpdateStatus.ESP_url = Lectura["VBLE2"]["url"].as<String>();
      update = true;
    }  
  }
  
#endif
  
  //Check Normal Firmware
  if(Database->Send_Command("/prod/fw/prod/",&Lectura, READ_FW)){
    String PSOC5_Ver   = Lectura["VELT2"]["verstr"].as<String>();
    uint16 VELT_int_Version=ParseFirmwareVersion(PSOC5_Ver);

    if(VELT_int_Version>Configuracion.data.FirmwarePSOC){
      UpdateStatus.PSOC5_UpdateAvailable= true;
      UpdateStatus.PSOC_url = Lectura["VELT2"]["url"].as<String>();
      update = true;
    }    

    String ESP_Ver   = Lectura["VBLE2"]["verstr"].as<String>();
    uint16 ESP_int_Version=ParseFirmwareVersion(ESP_Ver);

    if(ESP_int_Version>Configuracion.data.Firmware){
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
    SPIFFS.end();
    if(!SPIFFS.begin(1,"/spiffs",1,"PSOC5")){
      SPIFFS.end();					
      SPIFFS.begin(1,"/spiffs",1,"PSOC5");
    }
    if(SPIFFS.exists("/FreeRTOS_V6.cyacd")){
      vTaskDelay(pdMS_TO_TICKS(50));
      SPIFFS.format();
    }

    FileName="/FreeRTOS_V6.cyacd";

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
      //reboot
      MAIN_RESET_Write(0);
      ESP.restart();
    }
  }
  //Aqui solo lleg asi se está actualizando el PSOC5

  stream->stop();
  DownloadClient.end(); 
  delete stream;

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
  uint8_t Error_Count = 0, null_count=0, bloquedByBLE=0;
  //timeouts para leer parametros y coms (Para leer menos a menudo)
  uint8 Params_Coms_Timeout =0;
  String Base_Path;
  long long ts_app_req =0;
  TickType_t xStarted = 0, xElapsed = 0; 
  int UpdateCheckTimeout=0;
  int  delayeando = 0;
  while(1){
    delayeando = 0;
    if(!ConfigFirebase.InternetConection && ConnectionState!= DISCONNECTED){
      ConnectionState=DISCONNECTING;
    }

    //Si estamos conectados a través de gsm y tenemos muchos errores, significa que hemos perdido la cobertura
    //Sino esque nos han desconectado el cable o hemos perdido el wifi
    //Y debemos reiniciar el modem
    if(Error_Count >= 20){
      if(Coms.GSM.ON && Coms.GSM.Internet && Error_Count >= 20){
        Coms.GSM.reboot = true;
        Coms.GSM.Internet = false;
        Error_Count = 0;
        if(ConnectionState > CONNECTING){
          ConnectionState=DISCONNECTING;
        }
        else{
          ConnectionState=DISCONNECTED;
          delayeando = 10000;
        }
      }
      else{
        Coms.ETH.Internet = false;
        Coms.Wifi.Internet = false;
        ConfigFirebase.InternetConection = false;
        if(ConnectionState > CONNECTING){
          ConnectionState=DISCONNECTING;
        }
        else{
          ConnectionState=DISCONNECTED;
          delayeando = 10000;
        }
      }
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
        delayeando = 10;
      }
      break;
    }

    case CONNECTING:
      if(initFirebaseClient()){
        ConfigFirebase.FirebaseConnected=1;
        Base_Path += (char *)ConfigFirebase.Device_Db_ID;
        ConnectionState=CONECTADO;
        delayeando = 10;
      }
      else{
        Error_Count++;
      }
      break;
    
    case CONECTADO:
      delayeando = 10;
      //Inicializar los timeouts
      Params.last_ts_app_req   = Database->Get_Timestamp("/params/ts_app_req",&Lectura);
      Params.last_ts_app_req   = Database->Get_Timestamp("/params/ts_app_req",&Lectura);
      Comands.last_ts_app_req  = Database->Get_Timestamp("/control/ts_app_req",&Lectura);
      Coms.last_ts_app_req     = Database->Get_Timestamp("/coms/ts_app_req",&Lectura);
      Status.last_ts_app_req   = Database->Get_Timestamp("/status/ts_app_req",&Lectura);
      Schedule.last_ts_app_req = Database->Get_Timestamp("/schedule/ts_app_req",&Lectura);
      
      Error_Count+=!WriteFirebaseFW("/fw/current");
      Error_Count+=!WriteFirebaseComs("/coms"); 

      if(ChargingGroup.Params.GroupActive){
        //comprobar si han borrado el grupo mientras estabamos desconectados
        if(!ReadFirebasePath("/groupId")){
          #ifdef DEBUG_GROUPS
          printf("Han borrado el grupo mientras estaba descoenctado!\n");
          #endif
          ChargingGroup.Params.GroupActive = false;
          ChargingGroup.Params.GroupMaster = false;
        }
      }


      //Comprobar si hay firmware nuevo
#ifndef DEVELOPMENT
        if(!memcmp(Params.Fw_Update_mode, "AA",2)){
          ConnectionState = UPDATING;
          break;
        }
#endif
      
      ConnectionState=IDLE;
      break;

    case IDLE:
      ConfigFirebase.ClientConnected = false;
      ConfigFirebase.ClientAuthenticated = false;
      //No connection == Disconnect
      //Error_count > 10 == Disconnect

      if(!ConfigFirebase.InternetConection || Error_Count>10){
        ConnectionState=DISCONNECTING;
        break;
      }
      else if(ConfigFirebase.ResetUser){
        if(!serverbleGetConnected()){
          ReiniciarMultiusuario();
        }
        ConfigFirebase.ResetUser = 0;
        ConfigFirebase.UserReseted = 1;
      }
      /*else if(ConfigFirebase.WriteUser){
        EscribirMultiusuario();
        ConfigFirebase.WriteUser = 0;
      }*/
      //Si está conectado por ble no hace nada
      else if(serverbleGetConnected()){
        bloquedByBLE = 1;
        ConfigFirebase.ClientConnected = false;
        ConfigFirebase.ClientAuthenticated = false;
        break;
      }
      else if(!serverbleGetConnected() && bloquedByBLE){
        bloquedByBLE = 0;
        Status.last_ts_app_req = Database->Get_Timestamp("/status/ts_app_req",&Lectura) + 10000;
      }

      //comprobar si hay usuarios observando:    
      ts_app_req=Database->Get_Timestamp("/status/ts_app_req",&Lectura);
      if(ts_app_req < 1){//connection refused o autenticacion terminada, comprobar respuesta
        String ResponseString = Lectura["error"];
        if(strcmp(ResponseString.c_str(),"Auth token is expired") == 0){
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
        Error_Count=0;
      }

      //Comprobar actualizaciones automaticas
      if(++UpdateCheckTimeout>8575){ // Unas 12 horas
        if(!memcmp(Status.HPT_status, "A1",2) || !memcmp(Status.HPT_status, "0V",2)){
          UpdateCheckTimeout=0;
          if(!memcmp(Params.Fw_Update_mode, "AA",2)){
            ConnectionState = UPDATING;
            break;
          }
        }
      }
      
      if(ts_app_req > Status.last_ts_app_req && !serverbleGetConnected()){
        Status.last_ts_app_req= ts_app_req;
        if(GetStateTime(Status.LastConn)> 5000){
          xStarted = xTaskGetTickCount();
          Error_Count+=!WriteFirebaseStatus("/status");
          if(!ConfigFirebase.ClientConnected){  
            Error_Count+=!WriteFirebaseComs("/coms"); 
            ReadFirebaseUser();  
            delayeando = 10;   
            ConnectionState = USER_CONNECTED;
            ConfigFirebase.ClientConnected  = true;
            Error_Count=0;
          }
        }
      }
      
      break;

    /*********************** Usuario conectado **********************/
    case USER_CONNECTED:
      if(askForPermission){
        givePermission = true;
        break;
      }

      if(serverbleGetConnected()){
        ConfigFirebase.ClientConnected = false;
        ConfigFirebase.ClientAuthenticated = false;
        ConnectionState = IDLE;
        break;
      }

      //Comprobar actualizaciones manualmente
      else if(Comands.fw_update){
        if(!memcmp(Status.HPT_status, "A1",2) || !memcmp(Status.HPT_status, "0V",2) ){
          UpdateCheckTimeout=0;
          Comands.fw_update=0;
          ConnectionState = UPDATING;
          break;
        }
      }
      
       //comprobar si hay usuarios observando:    
      ts_app_req=Database->Get_Timestamp("/status/ts_app_req",&Lectura);
      if(ts_app_req < 1){//connection refused o autenticacion terminada, comprobar respuesta
        String ResponseString = Lectura["error"];
        if(strcmp(ResponseString.c_str(),"Auth token is expired") == 0){
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

      if(ts_app_req > Status.last_ts_app_req){
        Status.last_ts_app_req= ts_app_req;
        xStarted = xTaskGetTickCount();
        Error_Count+=!WriteFirebaseStatus("/status");
        ConfigFirebase.ClientConnected  = true;
      }

      //Si pasan 35 segundos sin actualizar el status, lo damos por desconectado
      xElapsed=xTaskGetTickCount()-xStarted;
      if(pdTICKS_TO_MS(xElapsed)>45000){
        ConfigFirebase.ClientConnected  = false;
        ConnectionState = IDLE;
        break;
      }

      
      if(ConfigFirebase.WriteStatus){
        ConfigFirebase.WriteStatus=false;
        Error_Count+=!WriteFirebaseStatus("/status");
      }

      if(ConfigFirebase.WriteParams){
        ConfigFirebase.WriteParams=false;
        Error_Count+=!WriteFirebaseParams("/params");
        
      }

      if(ConfigFirebase.WriteControl){
        ConfigFirebase.WriteControl=0;
        Error_Count+=!WriteFirebaseControl("/control");
        
      }
      if(ConfigFirebase.WriteTime){
        ConfigFirebase.WriteTime=0;
        Error_Count+=!WriteFirebaseTimes("/status/times");
        
      }
      if(++Params_Coms_Timeout>=5){
        Error_Count+=!ReadFirebaseParams("/params");
        Error_Count+=!ReadFirebaseShedule("/schedule");
        Params_Coms_Timeout = 0;

         
      }
      if(ConfigFirebase.ResetUser){
        ReiniciarMultiusuario();
        ConfigFirebase.ResetUser = 0;
        ConfigFirebase.UserReseted = 1;
        
      }
        

      //Comprobar actualizaciones manuales
      if(Comands.fw_update){
        if(!memcmp(Status.HPT_status, "A1",2) || !memcmp(Status.HPT_status, "0V",2) ){
          UpdateCheckTimeout=0;
          Comands.fw_update=0;
          ConnectionState = UPDATING;
          break;
        }
      }
      

      //Mientras hay un cliente conectado y nada que hacer, miramos control
      Error_Count+=!ReadFirebaseControl("/control");

      break;

    /*********************** WRITTING states **********************/
    case WRITTING_CONTROL:
      Error_Count+=!WriteFirebaseControl("/control");
      ConnectionState=IDLE;
      break;

    case WRITTING_STATUS:
      Error_Count+=!WriteFirebaseStatus("/status");
      if(askForPermission){
        askForPermission = false;
        ConnectionState = IDLE;
        break;
      }
      ConnectionState = NextState ==0 ? READING_CONTROL : NextState;
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
      ConnectionState = NextState == 0 ? IDLE : NextState;
      break;

    case READING_PARAMS:
      Error_Count+=!ReadFirebaseParams("/params");
      ConnectionState=IDLE;
      break;
    
    //Está preparado pero nunca lee las comunicaciones de firebase
    case READING_COMS: 
      Error_Count+=!ReadFirebaseComs("/coms");
      ConnectionState=IDLE;
      NextState=READING_PARAMS;
      break;

#ifdef NO_EJECUTAR
    case READING_GROUP:
      Error_Count+=!ReadFirebaseGroups("123456789");
      ConnectionState=IDLE;
      NextState=WRITTING_STATUS;
      break;
#endif

    /*********************** UPDATING states **********************/
    case UPDATING:
      delayeando = 10;
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
      delayeando = 10;
      break;

    case 11:
      break;
    default:
      while(1){
        delay(1000);
        Serial.printf("Error en la maquina d estados de firebase: %i \n", ConnectionState);
      }
      break;
    }
    if(ConnectionState!=DISCONNECTING && delayeando == 0){
      delay(ConfigFirebase.ClientConnected ? 150:5000);
    }
    else{
      delay(delayeando);
    }
    

    #ifdef DEBUG
    if(LastStatus!= ConnectionState){
      Serial.printf("Maquina de estados de Firebase de % i a %i \n", LastStatus, ConnectionState);
      LastStatus= ConnectionState;
    }

    //chivatos de la ram
    if(ESP.getFreePsram() < 2000000 || ESP.getFreeHeap() < 20000){
        Serial.println(ESP.getFreePsram());
        Serial.println(ESP.getFreeHeap());
    }
    #endif
  }
}

//Helpers para la matriz de carga
uint8_t CreateMatrix(uint16_t* inBuff, uint8_t* outBufff){
    uint8_t matrix[7][24] = {0};

    memcpy(matrix, outBufff,168);

    //Si la programacion no está activa, volvemos
    if(inBuff[0]!= 1) return 1;

    //Obtenemos la potencia programada para estas horas
    uint8_t power = (inBuff[4]*0x100 + inBuff[5])/100;

    //Obtenemos las horas de inicio y final
    uint8_t init = inBuff[2];
    uint8_t fin  = inBuff[3];

    //Obtener los dias de la semana
    uint8_t dias = inBuff[1];

    //Iterar por los dias de la semana
    for(uint8_t dia=0; dia < 7;dia++){

        //Si el dia está habilitado
        if ((dias>>dia)&1){
            //Iteramos por las horas activas
            for(uint8_t hora = init;hora <= fin;hora++){
                matrix[dia][hora]=power;
            }
        }
    }

    memcpy(outBufff,matrix,168);

    return 0;
}

uint8_t hex2int(char ch){
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}

uint16_t hexChar_To_uint16_t(const uint8_t num){
    char hexCar[3];
    sprintf(hexCar, "%02X", num);
    uint16_t valor = hex2int(hexCar[0]) *0x10 + hex2int(hexCar[1]);
    return valor;
}

void Hex_Array_To_Uint16A_Array(const char* inBuff, uint16_t* outBuff) {
    for(int i=0; i<strlen(inBuff); i++){
        outBuff[i] = hexChar_To_uint16_t((uint8_t)inBuff[i]);
    }
}

#endif