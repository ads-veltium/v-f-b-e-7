#include "VeltFirebase.h"
#ifdef CONNECTED
#include "base64.h"
#include <esp_err.h>
#include "libb64/cdecode.h"

static const char* TAG = "FirebaseClient";

Real_Time_Database *Database = new Real_Time_Database();

DynamicJsonDocument Lectura(4096);

//Extern variables
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Comands                Comands;
extern carac_Status                 Status;
extern carac_Update_Status          UpdateStatus;
extern carac_Params                 Params;
extern carac_Coms                   Coms;
extern carac_Schedule               Schedule;

extern uint8 user_index;
extern uint8 PSOC_version_firmware[11];
extern uint8 ESP_version_firmware[11];

#ifdef USE_GROUPS
extern carac_group                  ChargingGroup;
extern carac_circuito               Circuitos[MAX_GROUP_SIZE];
extern carac_charger                charger_table[MAX_GROUP_SIZE];
#endif

// Variables para escritura de registros de carga
extern uint8_t last_record_in_mem;
extern uint8_t last_record_lap;
extern uint8_t record_index;
extern uint8_t record_lap;
extern boolean record_pending_for_write;
extern uint8 record_received_buffer_for_fb[550];
uint8_t older_record_in_fb;
boolean write_records_trigger = false;
extern boolean records_trigger;
extern boolean ask_for_new_record;
boolean retry_enable =true;

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

String FIREBASE_HOST_IP, HOST_FOR_DATABASE;
String FIREBASE_AUTH_HOST_IP, HOST_FOR_AUTH;

#ifndef DEVELOPMENT
String FIREBASE_PROJECT = FIREBASE_PROD_PROJECT;
#else
#ifdef DEMO_MODE_BACKEND
String FIREBASE_PROJECT = FIREBASE_PROD_PROJECT;
#else
String FIREBASE_PROJECT = FIREBASE_DEV_PROJECT;
#endif
#endif

unsigned long previousMillis = 0;

/*************************
 Client control functions
*************************/
bool initFirebaseClient(){

  /* // Modificaciones para convetir en direccion IP las direciones de los servidores de Firebase
  if (FIREBASE_HOST_IP.isEmpty()){
    obtener_direccion_IP(project, FIREBASE_HOST_IP);
  }
  HOST_FOR_DATABASE = FIREBASE_HOST_IP.isEmpty() ? project : FIREBASE_HOST_IP;

  if (FIREBASE_AUTH_HOST_IP.isEmpty()){
    obtener_direccion_IP(FIREBASE_AUTH_HOST, FIREBASE_AUTH_HOST_IP);
  }
  HOST_FOR_AUTH = FIREBASE_AUTH_HOST_IP.isEmpty() ? FIREBASE_AUTH_HOST : FIREBASE_AUTH_HOST_IP;
  */

  HOST_FOR_DATABASE = FIREBASE_PROJECT; 
  HOST_FOR_AUTH = FIREBASE_AUTH_HOST;

  ESP_LOGI(TAG, "HOST_FOR_DATABASE=%s HOST_FOR_AUTH=%s", HOST_FOR_DATABASE.c_str(), HOST_FOR_AUTH.c_str());

  Database->deviceID = ConfigFirebase.Device_Id;
  if (!Database->LogIn(HOST_FOR_AUTH)){
    return false;
  }
  if (!Database->begin(HOST_FOR_DATABASE, ConfigFirebase.Device_Db_ID)){
    return false;
  }
  return true;
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
  
    Escritura["ext_meter/net_power"]    = Status.net_power;
    Escritura["ext_meter/total_power"]    = Status.total_power;
  
  Escritura["error_code"] = Status.error_code;
  
  if(Database->Send_Command(Path,&Escritura,FB_UPDATE)){     
    if(Database->Send_Command(Path+"/ts_dev_ack",&Lectura,FB_TIMESTAMP)){
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

  if(Database->Send_Command(Path,&Escritura,FB_UPDATE)){     
    return true;
  }
  return false;
}

bool WriteFirebaseParams(String Path){
  DynamicJsonDocument Escritura(2048);
  Serial.println("Write Params CALLED");
  Escritura.clear();
  Escritura["auth_mode"]           = String(Params.autentication_mode).substring(0,2);
  Escritura["inst_curr_limit"]     = Params.install_current_limit;
  Escritura["contract_power1"]     = Params.potencia_contratada1;
  Escritura["contract_power2"]     = Params.potencia_contratada2;
  Escritura["dpc" ]                = Params.CDP;
  Escritura["fw_auto"]             = String(Params.Fw_Update_mode).substring(0,2);
  
  if(Database->Send_Command(Path,&Escritura,FB_UPDATE)){     
    //Write readed Timestamp
    if(Database->Send_Command(Path+"/ts_dev_ack",&Escritura,FB_TIMESTAMP)){
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

  if(Coms.ETH.Auto & Coms.ETH.ON){
    Escritura["eth/ip"]       = ip4addr_ntoa(&Coms.ETH.IP);
    Escritura["eth/gateway"]  = ip4addr_ntoa(&Coms.ETH.Gateway);
    Escritura["eth/mask"]     = ip4addr_ntoa(&Coms.ETH.Mask);
  }
  else{
    Escritura["eth/ip"]       = "";
    Escritura["eth/gateway"]  = "";
    Escritura["eth/mask"]     = "";

  }
  if(Coms.Wifi.ON){
    Escritura["wifi/ip"]      = ip4addr_ntoa(&Coms.Wifi.IP);
  }
  else{
    Escritura["wifi/ip"]      = "";
  }
  
  if(Database->Send_Command(Path,&Escritura,FB_UPDATE)){     
    if(Database->Send_Command(Path+"/ts_dev_ack",&Escritura,FB_TIMESTAMP)){
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
  Escritura["desired_current"] = Comands.desired_current;
  if(Database->Send_Command(Path,&Escritura,FB_UPDATE)){
    return true;
  }

  return false;
}

bool WriteFirebaseFW(String Path){
  DynamicJsonDocument Escritura(2048);
  Escritura.clear();

  if(Configuracion.data.FirmwareESP < 1000){
    Escritura[ParseFirmwareModel((char *)(ESP_version_firmware))] = ParseFirmwareModel((char *)(ESP_version_firmware)) +"_0" + String(Configuracion.data.FirmwareESP);
    Escritura[ParseFirmwareModel((char *)(PSOC_version_firmware))] = PSOC_version_firmware;
  }
  else{
    Escritura["VBLE2"] = "VBLE2_"+String(Configuracion.data.FirmwareESP);
    Escritura[ParseFirmwareModel((char *)(PSOC_version_firmware))] = PSOC_version_firmware;
  }


  if(Database->Send_Command(Path,&Escritura,FB_UPDATE)){
    return true;
  }

  return false;
}

bool WriteInitTimestamp(String Path){
  DynamicJsonDocument Escritura(2048);
  Escritura.clear();

  if(Database->Send_Command(Path,&Escritura,FB_UPDATE)){     
    if(Database->Send_Command(Path+"/ts_dev_ack",&Escritura,FB_TIMESTAMP)){
      return true;
    }
  }

  return false;
}

bool WriteFirebasegroups(String Path){
    DynamicJsonDocument Escritura(2048);
    Escritura.clear();

    Escritura["delete"] = false;

    if(Database->Send_Command(Path+"/params",&Escritura,FB_UPDATE)){
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
    Database->Send_Command("/params/user",&Escritura,FB_UPDATE);
    return true;
}

bool EscribirMultiusuario(){
    DynamicJsonDocument Escritura(2048);
    //Indicar que el cargador esta libre en firebase
     printf("Escribiendo usuario!\n");
    Escritura.clear();
    Escritura["charg_user_id"] = user_index;
    Database->Send_Command("/params/user",&Escritura,FB_UPDATE);
    return true;
}

bool WriteFirebaseHistoric(char* buffer){
    DynamicJsonDocument Escritura(2048);
    struct tm t = {0};  // Initalize to all 0's
    t.tm_year = (2000+buffer[4]-1900);  // This is year-1900, so 112 = 2012
    t.tm_mon  = (buffer[3]!=0)?buffer[3]-1:0;
    t.tm_mday = buffer[2];
    t.tm_hour = buffer[5];
    t.tm_min  = buffer[6];
    t.tm_sec  = buffer[7];
    int ConectionTS = mktime(&t);

    ESP_LOGI(TAG,"Wrtinging record. Connection Time - Year= %i, Month= %i, Day= %i, Hour= %i, Min= %i, Sec= %i - TimeStamp=[%i]",t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec,ConectionTS);

    if(ConectionTS> Status.Time.actual_time || ConectionTS < MIN_RECORD_TIMESTAMP){
      ESP_LOGW(TAG,"Error in Connection Time=%i Actual Time =%lli", ConectionTS, Status.Time.actual_time);
      ESP_LOGW(TAG,"Year=%i, Month=%i, Day=%i, Hour=%i, Min=%i, Sec=%i",t.tm_year,t.tm_mon,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec);
      // return true; // No se bloquea el almacenamiento de recargas con horas incorrectas.
    }

    //Bloqueo de escritura de registros recibidos con TS del 1/1/2000 00:00:00 o 3/3/2003 03:03:03
    if (ConectionTS==946598400 || ConectionTS==1046660583){ 
      ESP_LOGE(TAG,"Record with Connection Time=%i not written to Firebase",ConectionTS);
      return false;
    }

    t = {0};  // Initalize to all 0's
    t.tm_year = (2000+buffer[10]-1900);  // This is year-1900, so 112 = 2012
    t.tm_mon  = (buffer[9]!=0)?buffer[9]-1:0;
    t.tm_mday = buffer[8];
    t.tm_hour = buffer[11];
    t.tm_min  = buffer[12];
    t.tm_sec  = buffer[13];
    int StartTs = mktime(&t);

    if(StartTs> Status.Time.actual_time || StartTs < MIN_RECORD_TIMESTAMP){
      ESP_LOGW(TAG,"Error in Start Time= %i Actual Time = %lli", StartTs, Status.Time.actual_time);
      // return true; // No se bloquea el almacenamiento de recargas con horas incorrectas
    }

    t = {0};  // Initalize to all 0's
    t.tm_year = (2000+buffer[16]-1900);  // This is year-1900, so 112 = 2012
    t.tm_mon  = (buffer[15]!=0)?buffer[15]-1:0;
    t.tm_mday = buffer[14];
    t.tm_hour = buffer[17];
    t.tm_min  = buffer[18];
    t.tm_sec  = buffer[19];
    int DisconTs = mktime(&t);

    if(DisconTs < StartTs || DisconTs < MIN_RECORD_TIMESTAMP){
      ESP_LOGW(TAG,"Error en Dicconnect Time= %i Actual Time = %lli", DisconTs, Status.Time.actual_time);
      // return true; // No se bloquea el almacenamiento de recargas con horas incorrectas
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
    
    while(ConfigFirebase.FirebaseConnState != IDLE && !givePermission){
      delay(10);
      if(++count > 1000){
        return true;
      }
    }
    
    uint8 ReturnToLastconn = ConfigFirebase.FirebaseConnState;
    ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
    ConfigFirebase.FirebaseConnState = WRITING_RECORD;
    ESP_LOGI(TAG, "Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);

    String Path = "/records/";
    Escritura.clear();

    Escritura["act"] = Encoded;
    Escritura["con"] = ConectionTS;
    Escritura["dis"] = DisconTs;
    Escritura["rea"] = "";
    Escritura["sta"] = StartTs;
    Escritura["usr"] = buffer[1] + buffer[0]*0x100;
    Escritura["index"] = record_index;
    Escritura["lap"] = record_lap;

    char buf[10];
    ltoa(ConectionTS, buf, 10); 
    
    //escribir el historico nuevo
    if(Database->Send_Command(Path+buf,&Escritura,FB_UPDATE)){
      ConfigFirebase.FirebaseConnState = ReturnToLastconn;
      askForPermission = false;
      givePermission = false;
      return true;
    }
    else{
      printf("Fallo en el envio del registro!\n");
      Database->Send_Command(Path + buf, &Escritura, FB_UPDATE);
      ConfigFirebase.FirebaseConnState = ReturnToLastconn;
      askForPermission = false;
      givePermission = false;
      return false;
    }
    askForPermission = false;
    givePermission = false;
    ConfigFirebase.FirebaseConnState = ReturnToLastconn;
    return true;
}

bool WriteFirebaseLastRecordSynced(uint8_t last_rec_in_mem, uint8_t rec_index, uint8_t rec_lap){
  DynamicJsonDocument Escritura(2048);
#ifdef DEBUG
  ESP_LOGI(TAG,"Last record in mem=%i - Writing index=%i - Writinglap=%i",last_rec_in_mem, rec_index, rec_lap);
#endif
  Escritura.clear();
  Escritura["last_record_in_mem"] = last_rec_in_mem;
  Escritura["writing_record_index"] = rec_index;
  Escritura["last_record_lap"] = rec_lap;
  if (Database->Send_Command("/records_sync", &Escritura, FB_UPDATE)){
    if (Database->Send_Command("/records_sync/writing_ts", &Escritura, FB_TIMESTAMP)){
      return true;
    }
    else
      return false;
  }
  else
    return false;
}

bool WriteFirebaseOlderSyncRecord (uint8_t rec_index) {
  DynamicJsonDocument Escritura(2048);
#ifdef DEBUG
  ESP_LOGI(TAG,"Older registro escrito: Index %i", rec_index);
#endif
  Escritura.clear();
  Escritura["older_record_in_fb"] = rec_index;
  if (Database->Send_Command("/records_sync", &Escritura, FB_UPDATE)){
    if (Database->Send_Command("/records_sync/writing_ts", &Escritura, FB_TIMESTAMP)){
      return true;
    }
    else
      return false;
  }
  else
    return false;
}

bool WriteFirebaseGroupData(String Path){
  DynamicJsonDocument Escritura(2048);
  Escritura.clear();

  Escritura["params/master"] = (ChargingGroup.Params.GroupMaster == 1) ? true : false;
  Escritura["params/active"] = (ChargingGroup.Params.GroupActive == 1) ? true : false;
  Escritura["params/cdp"] = ChargingGroup.Params.CDP;
  Escritura["status/chargers"] = ChargingGroup.Charger_number;
  Escritura["status/circuits"] = ChargingGroup.Circuit_number;
  Escritura["status/connected"] = (ChargingGroup.Conected == 1) ? true : false;

    Escritura["params/max_dynamic_power"] = ChargingGroup.Params.ContractPower;
    Escritura["params/max_static_power"] = ChargingGroup.Params.potencia_max;

  if (ChargingGroup.Params.GroupMaster){
    //Escritura["params/max_dynamic_power"] = ChargingGroup.Params.ContractPower;
    //Escritura["params/max_static_power"] = ChargingGroup.Params.potencia_max;
    JsonObject estructura = Escritura.createNestedObject("struct");
    // Recorrer charger_table desde el primero hasta el valor 'n'
    for (int i = 0; i < ChargingGroup.Charger_number; i++){
      // Crear un campo para cada cargador dentro de "estructura"
      JsonObject chargerObj = estructura.createNestedObject(charger_table[i].name);
      // Añadir "circuito" y "fase" para cada cargador
      chargerObj["circuit"] = charger_table[i].Circuito;
      chargerObj["phase"] = charger_table[i].Fase;
      chargerObj["setpoint"] = charger_table[i].Consigna;
      chargerObj["hpt"] = charger_table[i].HPT;
      chargerObj["current"] = charger_table[i].Current;
    }
  }
  else {
    JsonObject estructura = Escritura.createNestedObject("struct");
    //JsonObject estructura = Escritura.createNestedObject("params/max_dynamic_power");
    //JsonObject estructura = Escritura.createNestedObject("params/max_static_power");
  }

  ip4_addr_t addr;
  addr.addr = ChargingGroup.MasterIP;
  Escritura["status/masterIP"] = ip4addr_ntoa(&addr);
  //Escritura["status/masterIP"]     = ip4addr_ntoa(&ChargingGroup.MasterIP);
  
  if(Database->Send_Command(Path,&Escritura,FB_UPDATE)){     
    if(Database->Send_Command(Path+"/ts_dev_ack",&Escritura,FB_TIMESTAMP)){
      return true;
    }
  }

  return false;
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
    if(Database->Send_Command(Path+"/params",&Lectura, FB_READ, true)){
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
      if(Database->Send_Command(Path+"/circuits",&Lectura, FB_READ, true)){ 
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
        if(Database->Send_Command(Path+"/devices",&Lectura, FB_READ, true)){ 
          JsonObject root = Lectura.as<JsonObject>();

          //crear una copia temporal para almacenar los que están cargando
          carac_charger temp_chargers[MAX_GROUP_SIZE];
          uint8_t temp_chargers_size =0;

          for(uint8_t i=0; i<ChargingGroup.Charger_number;i++){    
            if(!memcmp(charger_table[i].HPT, "C2",3)){
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
              memcpy(charger_table[ChargingGroup.Charger_number-1].HPT,"C2",3);
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
      
      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,FB_TIMESTAMP, true)){
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

    if(Database->Send_Command(Path,&Lectura, FB_READ)){

      Schedule.num_shedules = 100;
      String programaciones[Schedule.num_shedules];

      //Obtener el numero de programaciones que hay en firebase
      for(int i = 0;i< 100;i++){
        String key = i<10? "P0":"P";
        key+=String(i);
        if(!(Lectura["programs_utc"][key])){
          break;
        }
        programaciones[i] = Lectura["programs_utc"][key].as<String>();
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
      SendScheduleMatrixToPSOC5(plainMatrix);

      Schedule.last_ts_app_req=ts_app_req;

      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,FB_TIMESTAMP)){
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
    if(Database->Send_Command(Path,&Lectura, FB_READ)){
      Coms.last_ts_app_req=ts_app_req;

      Coms.Wifi.ON   = Lectura["wifi"]["on"];

      Coms.ETH.ON   = Lectura["eth"]["on"];
      Coms.ETH.Auto = Lectura["eth"]["auto"];
      
            //Store coms in psoc5 flash memory
      //SendToPSOC5(COMS_CONFIGURATION_CHAR_HANDLE);

      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,FB_TIMESTAMP)){
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
    if (Database->Send_Command(Path, &Lectura, FB_READ)){
      ESP_LOGI(TAG,"ReadFirebaseParams. Lectura = %s",Lectura.as<String>().c_str());

      Params.last_ts_app_req = ts_app_req;

      if (Lectura["auth_mode"].as<String>() != "null"){
        if (memcmp(Params.autentication_mode, Lectura["auth_mode"].as<String>().c_str(), 2)){
          memcpy(Params.autentication_mode, Lectura["auth_mode"].as<String>().c_str(), 2);
          Params.autentication_mode[2] = '\0';
          ESP_LOGI(TAG,"ReadFirebaseParams - auth_mode = %s",Params.autentication_mode);
          SendToPSOC5(Params.autentication_mode, 2, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
        }
    
        if (memcmp(Params.Fw_Update_mode, Lectura["fw_auto"].as<String>().c_str(), 2) != 0){
          memcpy(Params.Fw_Update_mode, Lectura["fw_auto"].as<String>().c_str(), 2);
          Params.Fw_Update_mode[2] = '\0';
          ESP_LOGI(TAG,"ReadFirebaseParams - Fw_Update_mode = %s",Params.Fw_Update_mode);
          SendToPSOC5(Params.Fw_Update_mode, 2, COMS_FW_UPDATEMODE_CHAR_HANDLE);
        }

        if (Params.potencia_contratada1 != Lectura["contract_power"].as<uint16>()){
          Params.potencia_contratada1 = Lectura["contract_power"].as<uint16>();
          uint8 potencia_contr[2];
          potencia_contr[0] = Params.potencia_contratada1;
          potencia_contr[1] = Params.potencia_contratada1 >> 8;
          SendToPSOC5(potencia_contr, 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P1_CHAR_HANDLE);
        }

        if (Params.potencia_contratada2 != Lectura["contract_power_P2"].as<uint16>()){
          Params.potencia_contratada2 = Lectura["contract_power_P2"].as<uint16>();
          uint8 potencia_contr_2[2];
          potencia_contr_2[0] = Params.potencia_contratada2;
          potencia_contr_2[1] = Params.potencia_contratada2 >> 8;
          SendToPSOC5(potencia_contr_2, 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P2_CHAR_HANDLE);
        }

        if(Params.CDP != Lectura["dpc"].as<uint8>()){
          Params.CDP=Lectura["dpc"].as<uint8>();
          SendToPSOC5(Params.CDP,DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
        }

        if(Params.install_current_limit != Lectura["inst_curr_limit"].as<uint8>()){
          Params.install_current_limit=Lectura["inst_curr_limit"].as<uint8>();
          SendToPSOC5(Params.install_current_limit,MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);
        }
      }

      if(!Database->Send_Command(Path+"/ts_dev_ack",&Lectura,FB_TIMESTAMP)){
          return false;
      }
    }
    else return false;  
  }

  return true;
}

bool ReadFirebasePath(String Path){
  
    Lectura.clear();
    if(Database->Send_Command(Path,&Lectura, FB_READ)){
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
  if(Database->Send_Command("/params/user",&Lectura, FB_READ)){
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
  long long ts_app_req = Database->Get_Timestamp(Path + "/ts_app_req", &Lectura);
  if (ts_app_req > last_ts_app_req){
    Lectura.clear();
    if (Database->Send_Command(Path, &Lectura, FB_READ)){
      last_ts_app_req = ts_app_req;
      Comands.start = Lectura["start"] ? true : Comands.start;
      ConfigFirebase.ClientAuthenticated = Comands.start == true;
      Comands.stop = Lectura["stop"] ? true : Comands.stop;
      Comands.reset = Lectura["reset"] ? true : Comands.reset;
      Comands.fw_update = Lectura["fw_update"] ? true : Comands.fw_update;
      Comands.conn_lock = Lectura["conn_lock"] ? true : Comands.conn_lock;

      if ((Comands.desired_current != Lectura["desired_current"] && !Comands.Newdata) && Lectura["desired_current"] > 6){
        if (!ChargingGroup.Params.GroupActive){
          Comands.desired_current = Lectura["desired_current"];
          Comands.Newdata = true;
        }
      }

      WriteFirebaseControl("/control");
      if (!Database->Send_Command(Path + "/ts_dev_ack", &Lectura, FB_TIMESTAMP)){
        return false;
      }
    }
    else
      return false;
  }
  return true;
}

bool ReadFirebaseReadingPeriod(){
  Lectura.clear();
  if(Database->Send_Command("/prod/global_vars/coms_period",&Lectura, FB_READ_ROOT)){
    ConfigFirebase.PeriodoLectura = Lectura.as<uint8>() * 1000;
    ESP_LOGI(TAG,"ConfigFirebase.PeriodoLectura=%i",ConfigFirebase.PeriodoLectura);
    return true;
  }
  else 
    return false;
}

bool ReadFirebaseUpdatePeriod(){
  Lectura.clear();
  if(Database->Send_Command("/prod/global_vars/update_period",&Lectura, FB_READ_ROOT)){
    ConfigFirebase.PeriodoFWUpdate = Lectura.as<uint16>();
    ESP_LOGI(TAG,"ConfigFirebase.PeriodoFWUpdate=%i",ConfigFirebase.PeriodoFWUpdate);
    return true;
  }
  else 
    return false;
}

bool ReadFirebaseGroupsDebug(){
  Lectura.clear();
  if(Database->Send_Command("/prod/global_vars/groups_debug",&Lectura, FB_READ_ROOT)){
    ConfigFirebase.GroupsDebug = Lectura.as<uint8>() * 1000;
    ESP_LOGI(TAG,"ConfigFirebase.GroupsDebug=%i",ConfigFirebase.GroupsDebug);
    return true;
  }
  else 
    return false;
}

bool ReadFirebaseOlderSyncRecord(){
  Lectura.clear();
  if (Database->Send_Command("/records_sync", &Lectura, FB_READ)){
    if (Lectura.containsKey("older_record_in_fb")){
      older_record_in_fb = Lectura["older_record_in_fb"].as<uint8>();
      ESP_LOGI(TAG, "older_record_in_fb read = %i", older_record_in_fb);
      return true;
    }
    else{
      ESP_LOGW(TAG, "older_record_in_fb not found");
      older_record_in_fb = 0;
      WriteFirebaseOlderSyncRecord(older_record_in_fb);
      return true;
    }
  }
  else{
    ESP_LOGW(TAG, "Failed to read /records_sync");
    return false;
  }
}

bool ReadFirebaseRecordsTrigger(){
  Lectura.clear();
  if (Database->Send_Command("/records_sync", &Lectura, FB_READ)){
    if (Lectura.containsKey("write_records_trigger")){
      write_records_trigger = Lectura["write_records_trigger"].as<bool>();
      if (write_records_trigger){
        ESP_LOGI(TAG, "write_records_trigger read = %i", write_records_trigger);
      }
      return true;
    }
    else{
      ESP_LOGW(TAG, "write_records_trigger not found");
      return false;
    }
  }
  else{
    ESP_LOGW(TAG, "Failed to read /records_sync");
    return false;
  }
}

bool WriteFirebaseRecordsTrigger(bool trigger_value){
  DynamicJsonDocument Escritura(2048);
  Escritura.clear();
  Escritura["write_records_trigger"] = trigger_value;
  if (Database->Send_Command("/records_sync", &Escritura, FB_UPDATE)){
    return true;
  }
  else
    return false;
}

/*****************************************************
 *              Sistema de Actualización
 *****************************************************/

bool ReadFirebaseBranch(){
  Lectura.clear();
  if (Database->Send_Command("/fw", &Lectura, FB_READ)){
    UpdateStatus.Alt_available = Lectura["alt"]["available"];
    UpdateStatus.Group = Lectura["perms"]["group"].as<String>();
    if (UpdateStatus.Alt_available){
      UpdateStatus.Branch = Lectura["alt"]["branch"].as<String>();
      return true;
    }
    else{
      return false;
    }
  }
  return false;
}

//Checkear compatibilidad del grupo del cargador con el grupo de los FW
bool CheckFwPermissions(String fw_group){
  if ((!fw_group.compareTo(UpdateStatus.Group)) || (!fw_group.compareTo("all"))){
    if (fw_group.compareTo("none")){
      return true;
    }
    else{
      return false;
    }
  }
  else{
    return false;
  }
}

bool CheckForUpdate(){
  bool update = false;
  String branch = "prod";
  String PSOC_string;
  String ESP_string;

  //Leemos la rama del cargador y vemos si está habilitado
  if(ReadFirebaseBranch()){
    branch = UpdateStatus.Branch;
  }  
  else{
    branch = "prod";
  }

  ESP_LOGI(TAG,"Branch de FW=%s",branch.c_str());
  PSOC_string = ParseFirmwareModel((char *)(PSOC_version_firmware));
  ESP_string = ParseFirmwareModel((char *)(ESP_version_firmware));
  ESP_LOGI(TAG,"PSOC_string=%s - ESP_string=%s",PSOC_string.c_str(),ESP_string.c_str());

  //Check Firmware PSoC
  if (Database->Send_Command("/prod/fw/" + branch + "/" + PSOC_string, &Lectura, FB_READ_ROOT)){
    String PSOC_Ver = Lectura["verstr"].as<String>();
    String fw_psoc_group = Lectura["group"].as<String>();
    uint16 PSOC_int_Version = ParseFirmwareVersion(PSOC_Ver);
    ESP_LOGI(TAG, "PSoC: Version instalada:%i - Version publicada:%i",Configuracion.data.FirmwarePSOC,PSOC_int_Version);
    if (PSOC_int_Version != Configuracion.data.FirmwarePSOC){
      if (CheckFwPermissions(fw_psoc_group)){
        UpdateStatus.PSOC_UpdateAvailable = true;
        UpdateStatus.PSOC_url = Lectura["url"].as<String>();
        update = true;
        ESP_LOGI(TAG, "PSoC: Actualizando a %i",PSOC_int_Version);
      }
      else{
        ESP_LOGI(TAG, "group=%s. FW group=%s. PSoC NO se actualiza",UpdateStatus.Group.c_str(),fw_psoc_group.c_str());
      }
    }
  }

  // Check Firmware ESP
  if (Database->Send_Command("/prod/fw/" + branch + "/" + ESP_string, &Lectura, FB_READ_ROOT)){
    String ESP_Ver = Lectura["verstr"].as<String>();
    String fw_esp_group = Lectura["group"].as<String>();
    uint16 ESP_int_Version = ParseFirmwareVersion(ESP_Ver);
    ESP_LOGI(TAG, "ESP: Version instalada:%i - Version publicada:%i",Configuracion.data.FirmwareESP,ESP_int_Version);
    if (ESP_int_Version != Configuracion.data.FirmwareESP){
      if (CheckFwPermissions(fw_esp_group)){
        UpdateStatus.ESP_UpdateAvailable = true;
        UpdateStatus.ESP_url = Lectura["url"].as<String>();
        update = true;
        ESP_LOGI(TAG, "ESP32: Actualizando a %i", ESP_int_Version);
      }
      else{
        ESP_LOGI(TAG, "group=%s. FW group=%s. ESP NO se actualiza", UpdateStatus.Group.c_str(), fw_esp_group.c_str());
      }
    }
  }
  return update;
  //return false;
}

void DownloadFileTask(void *args){
  UpdateStatus.DescargandoArchivo=true;

  String url;
  const uint8 MAX_RETRIES = 5; // Número máximo de intentos
  bool successPSOC = false;
  bool successESP = false;

  if(UpdateStatus.PSOC_UpdateAvailable) url=UpdateStatus.PSOC_url;
  else{
    url=UpdateStatus.ESP_url;
  }

  for(int attempt = 1; ((attempt <= MAX_RETRIES) && !(successPSOC || successESP)); attempt++)
  {
    ESP_LOGI(TAG, "Intento de descarga %d de %d...", attempt, MAX_RETRIES);
    //Si descargamos actualizacion para el PSOC5, debemos crear el archivo en el SPIFFS
    File UpdateFile;
    if(UpdateStatus.PSOC_UpdateAvailable){
      ESP_LOGI(TAG, "FW for PSoC Available at URL: %s", url.c_str());
      SPIFFS.end();
      vTaskDelay(pdMS_TO_TICKS(20));
      if (!SPIFFS.begin(1, "/spiffs", 2, "PSOC5")) {
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_LOGI(TAG, "Fallo al iniciar SPIFFS. Reintentando...");
        SPIFFS.end();
        vTaskDelay(pdMS_TO_TICKS(20));
        if (!SPIFFS.begin(1, "/spiffs", 2, "PSOC5")) {
          Serial.println("Fallo crítico al iniciar SPIFFS. Abortando...");
          UpdateStatus.DescargandoArchivo = false;
          vTaskDelete(NULL);
        }
      } 
      if (SPIFFS.exists(PSOC_UPDATE_FILE)) {
        ESP_LOGI(TAG, "Existe fichero previo - formateando SPIFFS");
        SPIFFS.format();
        delay(200);
      }
      vTaskDelay(pdMS_TO_TICKS(50));
      UpdateFile = SPIFFS.open(PSOC_UPDATE_FILE, FILE_WRITE);
    }
    
    HTTPClient DownloadClient;
    ESP_LOGI(TAG, "Downloading: %s", url.c_str());
    DownloadClient.begin(url);
    WiFiClient *stream = nullptr;

    int total = 0;
    if(DownloadClient.GET()>0){
      total = DownloadClient.getSize();
      ESP_LOGI(TAG,"Tamaño total del fichero de actualizacion: %d\n", total);
      int len = total;
      int written=0;
      stream = DownloadClient.getStreamPtr();
      uint8_t buff[2048] = { 0 };

      if(UpdateStatus.ESP_UpdateAvailable && !UpdateStatus.PSOC_UpdateAvailable)
      {
        if(!Update.begin(total)) ESP_LOGI(TAG, "Error with size en descarga del ESP");
        else ESP_LOGI(TAG,"Correcta descarga del ESP");
      }
      const int MAX_EMPTY_CYCLES = 500;
      int emptyCycles = 0;
      while (DownloadClient.connected() &&(len > 0 || len == -1))
      {
        size_t size = stream->available();
        if (size > 0)
        {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

          if(UpdateStatus.PSOC_UpdateAvailable)
          {
            if (UpdateFile.write(buff, c)!=c)
            {
              ESP_LOGI(TAG, "Error escribiendo en el archivo de actualización PSoC");
              break;
            }
          } 
          else
          {
            if (Update.write(buff, c) != c) {
              ESP_LOGI(TAG, "Error escribiendo en la actualización del ESP");
              break;
            }
          } 
          
          if (len > 0)
          {
            written +=c;
            len -= c;
          }
        }
        else 
        {
          emptyCycles++;
          if (emptyCycles >= MAX_EMPTY_CYCLES) {
            ESP_LOGW(TAG, "Error: demasiados ciclos sin datos. Abortando...");
            break;
          }
          vTaskDelay(10 / portTICK_PERIOD_MS);
        }
      }
      UpdateFile.close();
      if (written == total) {
        ESP_LOGI(TAG, "Archivo descargado correctamente.Esperado: %d, Recibido: %d\n", total, written);
        if(UpdateStatus.PSOC_UpdateAvailable)
        {
          UpdateFile = SPIFFS.open(PSOC_UPDATE_FILE);
          ESP_LOGI(TAG,"Revisión del tamaño del fichero: %d\n", UpdateFile.size());
          if(UpdateFile.size() == total) successPSOC = true;
          UpdateFile.close();
        }
        else successESP = true;
      }
      else ESP_LOGI(TAG, "Error descargando archivo. Esperado: %d, Recibido: %d\n", total, written);

    }

    if(UpdateStatus.ESP_UpdateAvailable && !UpdateStatus.PSOC_UpdateAvailable){
      if(Update.end()){	
        //reboot
        Serial.println("Update finalizado - Reboot");
        MAIN_RESET_Write(0);
        ESP.restart();
      }
    }
    // Aqui solo llega asi se está actualizando el PSOC
    if (stream) stream->stop();
    DownloadClient.end();
    if(!successESP && !successPSOC && attempt==MAX_RETRIES)
    {
      ESP_LOGW(TAG, "ERROR. NO SE HA PODIDO DESCARGAR EL FICHERO DE ACTUALIZACION. Reiniciando...");
      Status.error_code = 0x60;
      vTaskDelay(pdMS_TO_TICKS(5000));

      MAIN_RESET_Write(0);
      delay(500);
      ESP.restart();
    }
  }
  
  setMainFwUpdateActive(1);
  UpdateStatus.DescargandoArchivo=false;
  vTaskDelete(NULL);
}




/***************************************************
 Tarea de control de firebase
***************************************************/

void Firebase_Conn_Task(void *args){
  ConfigFirebase.FirebaseConnState =  DISCONNECTED;
  ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
  uint8_t NextState = 0;
  uint8_t Error_Count = 0;
  uint8_t null_count=0;
  uint8_t bloquedByBLE=0;
  //timeouts para leer parametros y coms (Para leer menos a menudo)
  uint8 Params_Coms_Timeout =0;
  // String Base_Path;
  long long ts_app_req =0;
  TickType_t xStarted = 0;
  TickType_t xElapsed = 0; 
  uint32_t UpdateCheckTimeout=0;
  int  delayeando = 0;

  while(1){
    delayeando = 0;
    if(!ConfigFirebase.InternetConection && ConfigFirebase.FirebaseConnState!= DISCONNECTED){
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState=DISCONNECTING;
      ESP_LOGI(TAG, "GENERAL - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
    }
    
    switch (ConfigFirebase.FirebaseConnState){
    case DISCONNECTED:{

      if (ConfigFirebase.InternetConection){
        if (Database != NULL)
        {
          ESP_LOGI (TAG, "DISCONNECTED - Borrando Database");
          delete Database;
        }
        ESP_LOGI (TAG, "DISCONNECTED - Creando Database");
        Database = new Real_Time_Database();

        Error_Count = 0;
        // Base_Path = "/prod/devices/"; 
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = CONNECTING;
        ESP_LOGI(TAG, "DISCONNECTED - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
        delayeando = 10;
      }
      break;
    }

    case CONNECTING:
      bool conex_err;
      conex_err = initFirebaseClient();
      if(conex_err && (Database->idToken != NULL)){
        // Base_Path += (char *)ConfigFirebase.Device_Db_ID;
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = CONECTADO;
        ESP_LOGI(TAG, "CONNECTING - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
        delayeando = 10;
      }
      else{
        Error_Count++;
      }
      break;
    
    case CONECTADO:
      //Inicializar los timeouts
      Params.last_ts_app_req   = Database->Get_Timestamp("/params/ts_app_req",&Lectura);
      ESP_LOGI(TAG,"params/ts_app_req= %lld",Params.last_ts_app_req);
      Comands.last_ts_app_req  = Database->Get_Timestamp("/control/ts_app_req",&Lectura);
      ESP_LOGI(TAG,"control/ts_app_req= %lld",Comands.last_ts_app_req);
      Coms.last_ts_app_req     = Database->Get_Timestamp("/coms/ts_app_req",&Lectura);
      ESP_LOGI(TAG,"coms/ts_app_req= %lld",Coms.last_ts_app_req);
      Status.last_ts_app_req   = Database->Get_Timestamp("/status/ts_app_req",&Lectura);
      ESP_LOGI(TAG,"status/ts_app_req= %lld",Status.last_ts_app_req);
      Schedule.last_ts_app_req = Database->Get_Timestamp("/schedule/ts_app_req",&Lectura);
      ESP_LOGI(TAG,"schedule/ts_app_req= %lld",Schedule.last_ts_app_req);
      
      Error_Count+=!WriteFirebaseFW("/fw/current");
      Error_Count+=!WriteFirebaseComs("/coms");

      if(Status.ts_inicio){
        Error_Count+=!WriteInitTimestamp("/ts_last_init");
        Status.ts_inicio = false;
      }

      //Comprobar periodo de lectura
      if(!ReadFirebaseReadingPeriod()){
        ConfigFirebase.PeriodoLectura = FB_DEFAULT_READING_PERIOD;
      }
      //Comprobar periodo de fw auto update 
      if(!ReadFirebaseUpdatePeriod()){
        ConfigFirebase.PeriodoFWUpdate = FB_DEFAULT_CHECK_UPDATES_PERIOD;
      }

      if(!ReadFirebaseGroupsDebug()){
        ConfigFirebase.GroupsDebug = 0;
      }

      if (!ReadFirebaseRecordsTrigger()){
        WriteFirebaseRecordsTrigger(true);
      }
      else{
        records_trigger = write_records_trigger;
        if (ReadFirebaseOlderSyncRecord()){
          bool trigger_value = (last_record_in_mem != older_record_in_fb);
          WriteFirebaseRecordsTrigger(trigger_value);
        }
      }

#ifdef USE_GROUPS
      if(ChargingGroup.Params.GroupActive){
        //comprobar si han borrado el grupo mientras estabamos desconectados
        if(!ReadFirebasePath("/groupId")){
#ifdef DEBUG_GROUPS
          printf("FirebaseClient - Firebase_Conn_Task: CONECTADO - groupId borrado de Firebase\n");
#endif
          //  ADS - 2 lineas comentadas - Por ahora no hacemos nada ni nos salimos del grupo
          // ChargingGroup.Params.GroupActive = false;
          // ChargingGroup.Params.GroupMaster = false;
        }
      }
#endif

      //Comprobar si hay firmware nuevo
/*#ifndef DEVELOPMENT*/
      if(!memcmp(Params.Fw_Update_mode, "AA", 2)){
        ESP_LOGI(TAG, "Fw_Update_mode = %s",Params.Fw_Update_mode);
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = UPDATING;
        ESP_LOGI(TAG, "CONECTADO - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
        break;
      }
/*#endif*/
      
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = IDLE;
      ESP_LOGI(TAG, "CONECTADO - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      break;

    case IDLE:
      ConfigFirebase.ClientConnected = false;
      ConfigFirebase.ClientAuthenticated = false;
      //No connection == Disconnect
      //Error_count > 10 == Disconnect

      if(!ConfigFirebase.InternetConection || Error_Count>10){
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = DISCONNECTING;
        ESP_LOGI(TAG, "IDLE - No ha conexión a Internet - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
        break;
      }
      else if(ConfigFirebase.ResetUser){
        if(!serverbleGetConnected()){
          ESP_LOGI(TAG,"A ReiniciarMultiusiario");
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
      }
      //comprobar si hay usuarios observando:   
      ts_app_req = Database->Get_Timestamp("/status/ts_app_req",&Lectura);
      ESP_LOGI(TAG,"ts_app_req: %lld", ts_app_req);
      if (ts_app_req < 1){ // connection refused o autenticacion terminada, comprobar respuesta
        String ResponseString = Lectura["error"];
        if (strcmp(ResponseString.c_str(), "Auth token is expired") == 0){
          if (Database->LogIn(HOST_FOR_AUTH)){
            break;
          }
          ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
          ConfigFirebase.FirebaseConnState = DISCONNECTING;
          ESP_LOGI(TAG, "IDLE - Auth token expirado - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      }
        else{
          if (++null_count >= 3){
            ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
            ConfigFirebase.FirebaseConnState = DISCONNECTING;
            ESP_LOGI(TAG, "IDLE - Error de lectura de /status/ts_app_req - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
          }
        }
        break;
      }
      else{
        null_count=0;
        Error_Count=0;
      }

      //Comprobar actualizaciones automaticas cada FB_CHECK_UPDATES_PERIOD horas
      if(++UpdateCheckTimeout > (ConfigFirebase.PeriodoFWUpdate*60*60)/(ConfigFirebase.PeriodoLectura/1000)){
        ESP_LOGI(TAG, "UpdateCheckTimeout");
        if(!memcmp(Status.HPT_status, "A1", 2) || !memcmp(Status.HPT_status, "0V", 2)){
          UpdateCheckTimeout = 0;
          if(!memcmp(Params.Fw_Update_mode, "AA", 2)){
            ESP_LOGI(TAG, "Fw_Update_mode = %s",Params.Fw_Update_mode);
            ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
            ConfigFirebase.FirebaseConnState = UPDATING;
            ESP_LOGI(TAG, "IDLE - Fw_Update - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
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
            ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
            ConfigFirebase.FirebaseConnState = USER_CONNECTED;
            ESP_LOGI(TAG, "IDLE - Usuario conectado - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
            ConfigFirebase.ClientConnected  = true;
            Error_Count=0;
            break;
          }
        }
      }

      if (ConfigFirebase.GroupsDebug){
        unsigned long currentMillis = pdTICKS_TO_MS(xTaskGetTickCount());
        if (currentMillis - previousMillis >= GROUPS_DEBUG_INTERVAL){
          previousMillis = currentMillis;
          Error_Count += !WriteFirebaseGroupData("/group_data");
        }
      }

      if(ReadFirebaseRecordsTrigger()){
        records_trigger = write_records_trigger;
        WriteFirebaseRecordsTrigger(false);
      }

      if (record_pending_for_write){
        delayeando = 1;
        record_pending_for_write = false;      
        if (WriteFirebaseHistoric((char*)record_received_buffer_for_fb)) {
          retry_enable = true;
          if (WriteFirebaseLastRecordSynced(last_record_in_mem, record_index, last_record_lap)) {
            if (ReadFirebaseOlderSyncRecord()) {
              if (record_index == older_record_in_fb + 1) {
  #ifdef DEBUG
                ESP_LOGI(TAG, "record_index = %i - older_record_in_fb = %i", record_index, older_record_in_fb);
  #endif
                WriteFirebaseOlderSyncRecord(last_record_in_mem);
              } 
              else {
                if (record_index > 0) {
                  record_index--;
                } 
                else if (last_record_lap >= 1) {
                  record_index = MAX_RECORDS_IN_MEMORY - 1;
                  record_lap = last_record_lap - 1;
                }
                else{
                  ask_for_new_record = false;
  #ifdef DEBUG
                  ESP_LOGI(TAG, "record_index = %i - last_record_in_mem = %i", record_index, last_record_in_mem);
  #endif
                  WriteFirebaseOlderSyncRecord(last_record_in_mem);
                  break;
                }

                if (record_index<last_record_in_mem || (record_index/8 > last_record_in_mem/8)){
                    ask_for_new_record = true;
                } 
                else {
  #ifdef DEBUG
                  ESP_LOGI(TAG, "record_index = %i - last_record_in_mem = %i", record_index, last_record_in_mem);
  #endif
                  WriteFirebaseOlderSyncRecord(last_record_in_mem);
                }
              }
            } 
            else {
              ESP_LOGE(TAG, "No puede leer older_record_in_fb");
              WriteFirebaseOlderSyncRecord(record_index);
            }
          }
        }
        else if(retry_enable==true){
          retry_enable = false;
          ask_for_new_record = true;
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
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = IDLE;
        ESP_LOGI(TAG, "USER_CONNECTED - Conexión BLE - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
        break;
      }
      //comprobar si hay usuarios observando:    
      ts_app_req=Database->Get_Timestamp("/status/ts_app_req",&Lectura);
      if(ts_app_req < 1){//connection refused o autenticacion terminada, comprobar respuesta
        String ResponseString = Lectura["error"];
        if(strcmp(ResponseString.c_str(),"Auth token is expired") == 0){
            if(Database->LogIn(HOST_FOR_AUTH)){
              break;
            }
            ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
            ConfigFirebase.FirebaseConnState = DISCONNECTING;
            ESP_LOGI(TAG, "USER_CONNECTED - Auth token expirado - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
        }
        else{
          if(++null_count>=3){
            ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
            ConfigFirebase.FirebaseConnState = DISCONNECTING;
            ESP_LOGI(TAG, "USER_CONNECTED - Erorr de lectura /status/ts_app_req - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
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

      //Si pasan FB_USER_CONNECTED_TIMEOUT_S segundos sin actualizar el status, lo damos por desconectado
      xElapsed=xTaskGetTickCount()-xStarted;
      if(pdTICKS_TO_MS(xElapsed)>FB_USER_CONNECTED_TIMEOUT){
        ConfigFirebase.ClientConnected  = false;
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = IDLE;
        ESP_LOGI(TAG, "USER_CONNECTED - ts_app_req no actualizado en %i segundos", FB_USER_CONNECTED_TIMEOUT_S);
        ESP_LOGI(TAG, "USER_CONNECTED - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
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
          ESP_LOGI(TAG, "Orden de Update recibida");
          UpdateCheckTimeout=0;
          Comands.fw_update=0;
          ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
          ConfigFirebase.FirebaseConnState = UPDATING;
          ESP_LOGI(TAG, "USER_CONNECTED - fw_update - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
          break;
        }
      }
      

      //Mientras hay un cliente conectado y nada que hacer, miramos control
      Error_Count+=!ReadFirebaseControl("/control");

      break;

    /*********************** WRITING states **********************/
    case WRITING_CONTROL:
      Error_Count+=!WriteFirebaseControl("/control");
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = IDLE;
      ESP_LOGI(TAG, "WRITING_CONTROL - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      break;

    case WRITING_STATUS:
      Error_Count+=!WriteFirebaseStatus("/status");
      if(askForPermission){
        askForPermission = false;
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = IDLE;
        ESP_LOGI(TAG, "WRITING_STATUS - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
        break;
      }
      ConfigFirebase.FirebaseConnState = NextState ==0 ? READING_CONTROL : NextState;
      break;

    case WRITING_COMS:
      Error_Count+=!WriteFirebaseComs("/coms");
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = IDLE;
      ESP_LOGI(TAG, "WRITING_COMS - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      break;
    
    case WRITING_PARAMS:
      Error_Count+=!WriteFirebaseParams("/params");
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = IDLE;
      ESP_LOGI(TAG, "WRITING_PARAMS - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      break;

    case WRITING_TIMES:
      Error_Count+=!WriteFirebaseTimes("/status/times");
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = IDLE;
      ESP_LOGI(TAG, "WRITING_TIMES - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      break;
    /*********************** READING states **********************/
    case READING_CONTROL:
      Error_Count+=!ReadFirebaseControl("/control");
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = NextState == 0 ? IDLE : NextState;
      ESP_LOGI(TAG, "READING_CONTROL - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      break;

    case READING_PARAMS:
      Error_Count+=!ReadFirebaseParams("/params");
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = IDLE;
      ESP_LOGI(TAG, "READING_PARAMS - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      break;
    
    //Está preparado pero nunca lee las comunicaciones de firebase
    case READING_COMS: 
      Error_Count+=!ReadFirebaseComs("/coms");
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = IDLE;
      ESP_LOGI(TAG, "READING_COMS - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      NextState=READING_PARAMS;
      break;

#ifdef NO_EJECUTAR
    case READING_GROUP:
      Error_Count+=!ReadFirebaseGroups("123456789");
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = IDLE;
      NextState=WRITING_STATUS;
      break;
#endif

    /*********************** UPDATING states **********************/
    case UPDATING:
      delayeando = 10;
      bool CheckResult;
      CheckResult = CheckForUpdate();

      if(CheckResult){
        xTaskCreate(DownloadFileTask,"DOWNLOAD FILE", 4096*3, NULL, 5,NULL);
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = DOWNLOADING;
        ESP_LOGI(TAG, "UPDATING - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      }
      else{
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = IDLE;
        ESP_LOGI(TAG, "Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      }
      break;

    case DOWNLOADING:
      if(!UpdateStatus.DescargandoArchivo){
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = INSTALLING;
        ESP_LOGI(TAG, "DOWNLOADING - Maquina de estados de Firebase pasa de %s a %s", 
         String(ConfigFirebase.LastConnState).c_str(), 
         String(ConfigFirebase.FirebaseConnState).c_str());
         }  
      break;

    case INSTALLING:
      vTaskDelay(pdMS_TO_TICKS(4000));

      if(UpdateStatus.DobleUpdate){
        ESP_LOGI(TAG, "INSTALLING - Update segundo micro. Descargando");
        xTaskCreate(DownloadFileTask,"DOWNLOAD FILE", 4096*3, NULL, 5,NULL);
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = DOWNLOADING;
        ESP_LOGI(TAG, "INSTALLING - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      }
      //vTaskDelay(pdMS_TO_TICKS(4000));

      if(!UpdateStatus.DescargandoArchivo && !UpdateStatus.InstalandoArchivo){
        ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
        ConfigFirebase.FirebaseConnState = IDLE;
        ESP_LOGI(TAG, "INSTALLING - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      }
      //Do nothing
      break;

    /*********************** DISCONNECT states **********************/
    case DISCONNECTING:
      ESP_LOGW (TAG, "DISCONNECTING - Borrando Database");
      Database->end();
      Database->endAuth();
      delete Database;
      Database = nullptr;  
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = DISCONNECTED;
      ESP_LOGI(TAG, "DISCONNECTING - Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      delayeando = 10;
      break;

    case WRITING_RECORD:
      break;
    default:
      delay(1000);
      ESP_LOGE (TAG, "Error en la maquina de estados de Firebase. Estado: %i", ConfigFirebase.FirebaseConnState);
      ESP_LOGW (TAG, "Borrando Database");

      Database->end();
      Database->endAuth();
      delete Database;
      Database = nullptr;  
      ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
      ConfigFirebase.FirebaseConnState = DISCONNECTED;
      ESP_LOGI(TAG, "Maquina de estados de Firebase pasa de %i a %i", ConfigFirebase.LastConnState, ConfigFirebase.FirebaseConnState);
      break;
    }

    if (ConfigFirebase.FirebaseConnState != DISCONNECTING && delayeando == 0){
      delay(ConfigFirebase.ClientConnected ? 300 : ConfigFirebase.PeriodoLectura);
    }
    else{
      delay(delayeando);
    }

#ifdef DEBUG
    // chivatos de la ram
    if (ESP.getFreePsram() < 2000000 || ESP.getFreeHeap() < 20000){
      Serial.printf("FirebaseClient - Firebase_Conn_Task: Free PSRAM = %i\n", ESP.getFreePsram());
      Serial.printf("FirebaseClient - Firebase_Conn_Task: Free HEAP = %i\n", ESP.getFreeHeap());
    }
#endif
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

//Helpers para la matriz de carga
uint8_t CreateMatrix(uint16_t* inBuff, uint8_t* outBufff){
    uint8_t sch[168] = {0};

    memcpy(sch, outBufff,168);

    //Si la programacion no está activa, volvemos
    if(inBuff[0]!= 1) return 1;

    //Obtenemos la potencia programada para estas horas
    uint8_t power = (inBuff[4]*0x100 + inBuff[5])/100;


    //Obtener los dias de la semana
    uint8_t dias = inBuff[1];

    //Iterar por los dias de la semana
    for(uint8_t dia=0; dia < 7;dia++){
        uint8_t mask = 1 << dia;
        //Si el dia está habilitado
        if ((mask & dias) !=0){
            
            //Obtenemos las horas de inicio y final
            uint8_t init = (dia*24) + inBuff[2];
            uint8_t fin  = init + deltaprogram(inBuff[2],inBuff[3]);

            for(uint8_t hora = init;hora < fin;hora++){
              sch[(hora+24)%168]=power;
            }     
        }
    }

    memcpy(outBufff,sch,168);

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

uint8_t deltaprogram(uint8_t s, uint8_t e){
  int d = e - s;
  if(d<=0){
    d+=24;
  }
  return d;
}

uint16_t hexChar_To_uint16_t(const uint8_t num){
    char hexCar[3];
    sprintf(hexCar, "%02X", num);
    uint16_t valor = hex2int(hexCar[0]) *0x10 + hex2int(hexCar[1]);
    return valor;
}

void Hex_Array_To_Uint16A_Array(const char* inBuff, uint16_t* outBuff) {
    for(int i=0; i<6; i++){
        outBuff[i] = hexChar_To_uint16_t((uint8_t)inBuff[i]);
    }
}

#endif