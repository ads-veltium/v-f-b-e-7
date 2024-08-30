#include "config.h"


//**********Comparadores de las caracteristicas**********/
static bool operator==(const carac_config& lhs, const carac_config& rhs){
    if(lhs.FirmwareESP != rhs.FirmwareESP){
      return false;
    }
    if(lhs.FirmwarePSOC != rhs.FirmwarePSOC){
      return false;
    }
    if(lhs.Data_cleared != rhs.Data_cleared){
      return false;
    }
    
    /*if(lhs.Part_Number != rhs.Part_Number){
      return false;
    }*/
    if(lhs.potencia_contratada1 != rhs.potencia_contratada1){
      return false;
    }
    if(lhs.potencia_contratada2 != rhs.potencia_contratada2){
      return false;
    }
    if(lhs.install_current_limit != rhs.install_current_limit){
      return false;
    }
    if(lhs.CDP != rhs.CDP){
      return false;
    }
    if(lhs.count_reinicios_malos != rhs.count_reinicios_malos){
      return false;
    }
        if(lhs.velt_v != rhs.velt_v){
      return false;
    }
     if(memcmp(lhs.device_ID, rhs.device_ID,sizeof(lhs.device_ID))){
        return false;
    }
     if(memcmp(lhs.deviceSerNum, rhs.deviceSerNum,sizeof(lhs.deviceSerNum))){
        return false;
    }
    if(memcmp(lhs.autentication_mode, rhs.autentication_mode,sizeof(lhs.autentication_mode))){
        return false;
    }
    if(memcmp(lhs.policy, rhs.policy,sizeof(lhs.policy))){
        return false;
    }
    if(lhs.medidor485 != rhs.medidor485){
        return false;
    }
    
    return true; 
}

static bool operator!=(const carac_config& lhs, const carac_config& rhs){
    return !(lhs==rhs); 
}

//*******************Tarea de configuracion**************/
//Atiende a las ordenes que se le envian desde el resto de tareas
//Para leer y almacenar datos en el spiffs.
void ConfigTask(void *arg){
    carac_config old_data = Configuracion.data;

    while(true){
        delay(100);
        if(Configuracion.data != old_data){
            Configuracion.Guardar = true;
            old_data = Configuracion.data;
        }
        if(Configuracion.Guardar){
            Configuracion.Store();
            Configuracion.Guardar = false;
        }
        else if(Configuracion.Cargar){
            Configuracion.Load();
            Configuracion.Cargar=false;
        }
#ifdef CONNECTED        
        else if(Configuracion.Group_Guardar){
            Configuracion.Group_Store();
            Configuracion.Group_Guardar = false;
        }

        else if(Configuracion.Group_Cargar){
            Configuracion.Group_Load();
            Configuracion.Group_Cargar=false;
        }
#endif
    }
}

//**********Funciones internas de la case de configuracion**************/

void Config::Carac_to_json(DynamicJsonDocument& ConfigJSON){
    ConfigJSON.clear();
    ConfigJSON["fw_esp"]      = data.FirmwareESP;
    ConfigJSON["fw_psoc"]     = data.FirmwarePSOC;
    //ConfigJSON["part_number"] = data.Part_Number;
    ConfigJSON["auth_mode"] = String(data.autentication_mode);
    ConfigJSON["pot_contratada_1"] = data.potencia_contratada1;
    ConfigJSON["pot_contratada_2"] = data.potencia_contratada2;
    ConfigJSON["inst_curr_limit"]  = data.install_current_limit;
    ConfigJSON["CDP"] = data.CDP;
    ConfigJSON["device_ID"]      = String(data.device_ID);
    ConfigJSON["device_ser_num"] = String(data.deviceSerNum);
    ConfigJSON["policy"] = String(data.policy);
    ConfigJSON["data_cleared"] = data.Data_cleared;
    ConfigJSON["count_reinicios_malos"] = data.count_reinicios_malos;
    ConfigJSON["velt_v"] = data.velt_v;
    ConfigJSON["medidor485"] = data.medidor485;
}

void Config::Json_to_carac(DynamicJsonDocument& ConfigJSON){
    data.FirmwareESP    = ConfigJSON["fw_esp"].as<uint16_t>();
    data.FirmwarePSOC   = ConfigJSON["fw_psoc"].as<uint16_t>();
    //data.Part_Number    = ConfigJSON["part_number"].as<String>();

    data.potencia_contratada1 = ConfigJSON["pot_contratada_1"].as<uint16_t>();
    data.potencia_contratada2 = ConfigJSON["pot_contratada_2"].as<uint16_t>();
    data.install_current_limit   = ConfigJSON["inst_curr_limit"].as<uint8_t>();

    data.CDP = ConfigJSON["CDP"].as<uint8_t>();

    memcpy(data.autentication_mode, ConfigJSON["auth_mode"].as<String>().c_str(),2);
    memcpy(data.device_ID, ConfigJSON["device_ID"].as<String>().c_str(),sizeof(data.device_ID));
    memcpy(data.deviceSerNum, ConfigJSON["device_ser_num"].as<String>().c_str(),sizeof(data.deviceSerNum));
    memcpy(data.policy, ConfigJSON["policy"].as<String>().c_str(),3);
    data.Data_cleared = ConfigJSON["data_cleared"].as<uint8_t>(); 
    data.count_reinicios_malos = ConfigJSON["count_reinicios_malos"].as<uint8_t>(); 
    data.velt_v = ConfigJSON["velt_v"].as<uint8_t>();
    data.medidor485 = ConfigJSON["medidor485"].as<uint8_t>();
}

#ifdef CONNECTED
void Config::Group_Carac_to_json(DynamicJsonDocument& ConfigJSON){
    ConfigJSON.clear();
    ConfigJSON["group"] = String(group_data.group);
    JsonArray paramsArray = ConfigJSON.createNestedArray("params");
    for (int i = 0; i < SIZE_OF_GROUP_PARAMS; i++) {
        paramsArray.add(group_data.params[i]);
    }
    JsonArray circuitsArray = ConfigJSON.createNestedArray("circuits");
    for (int i = 0; i < MAX_GROUP_SIZE; i++) {
        circuitsArray.add(group_data.circuits[i]);
    }
}
#endif

#ifdef CONNECTED
void Config::Group_Json_to_carac(DynamicJsonDocument& ConfigJSON){
    memcpy(group_data.group, ConfigJSON["group"].as<String>().c_str(),sizeof(group_data.group));
    JsonArray paramsArray = ConfigJSON["params"].as<JsonArray>();
    for (int i = 0; i < paramsArray.size() && i < sizeof(group_data.params); i++) {
        group_data.params[i] = paramsArray[i].as<int>();
    }
    JsonArray circuitsArray = ConfigJSON["circuits"].as<JsonArray>();
    for (int i = 0; i < circuitsArray.size() && i < MAX_GROUP_SIZE; i++) {
        group_data.circuits[i] = circuitsArray[i].as<int>();
    }}
#endif

//**********Funciones externas de la case de configuracion**************/
void Config::init(){
  
    //Arrancar el SPIFFS
    if(!SPIFFS.begin(1,"/spiffs",2,"ESP32")){
      SPIFFS.end();					
      SPIFFS.begin(1,"/spiffs",2,"ESP32");
    }

    //Sino existe el archivo, crear el de defecto
    if(!SPIFFS.exists("/config.json")){
      Store();
    }
#ifdef CONNECTED
    if(!SPIFFS.exists("/group.json")){
      Group_Store();
    }
#endif

    //Abrir el archivo para lectura y cargar los datos
    Load();
#ifdef CONNECTED
    Group_Load();
#endif

    //Crear la tarea encargada de detectar y almacenar cambios
    xTaskCreate(ConfigTask,"Task CONFIG",4096*2,NULL,PRIORIDAD_CONFIG,NULL);

    //Publicar las caracteristicas que solo se almacenan en el ble, 
    //comprobando que tengan valores logicos.
    if(memcmp(Configuracion.data.policy, "ALL",3) && memcmp(Configuracion.data.policy,"AUT",3) && memcmp(Configuracion.data.policy, "NON",3)){
        printf("Cambiando el policy porque no tenia!\n");
        memcpy(Configuracion.data.policy,"ALL",3);
    }

#ifdef CONNECTED
    if(Configuracion.data.medidor485 == 0x01 ){
      Update_Status_Coms(MED_LEYENDO_MEDIDOR);
    }
#endif

	modifyCharacteristic((uint8_t*)&Configuracion.data.policy, 3, POLICY);

}

bool Config::Load(){
    DynamicJsonDocument ConfigJSON(1024);
    ConfigFile = SPIFFS.open("/config.json", FILE_READ);
    String data_to_read;
    data_to_read = ConfigFile.readString();
    deserializeJson(ConfigJSON, data_to_read);
    ConfigFile.close();
    Json_to_carac(ConfigJSON);
#ifdef DEBUG_CONFIG
    printf("Cargando datos desde la flash!!\n");
    serializeJsonPretty(ConfigJSON, Serial);
#endif
    return true;
}

bool Config::Store(){
    //SPIFFS.end();
    //SPIFFS.begin(1,"/spiffs",10,"ESP32");
    DynamicJsonDocument ConfigJSON(1024);
    Carac_to_json(ConfigJSON);
    ConfigFile = SPIFFS.open("/config.json", FILE_WRITE);
    String data_to_store;
    serializeJson(ConfigJSON, data_to_store);
#ifdef DEBUG_CONFIG
    Serial.printf("Guardando datos en la flash =\n%s\n",data_to_store.c_str());
#endif
    ConfigFile.print(data_to_store);
    ConfigFile.close();

    return true;
}

#ifdef CONNECTED
bool Config::Group_Load(){
    DynamicJsonDocument ConfigJSON(1024);
    ConfigFile = SPIFFS.open("/group.json", FILE_READ);
    String data_to_read;
    data_to_read = ConfigFile.readString();
    deserializeJson(ConfigJSON, data_to_read);
    ConfigFile.close();
    Group_Json_to_carac(ConfigJSON);
#ifdef DEBUG_GROUPS
    Serial.printf("Leyendo datos del grupo de la flash =\n%s\n",data_to_read.c_str());
#endif
    return true;
}
#endif

#ifdef CONNECTED
bool Config::Group_Store(){
    DynamicJsonDocument ConfigJSON(1024);
    Group_Carac_to_json(ConfigJSON);
    ConfigFile = SPIFFS.open("/group.json", FILE_WRITE);
    String data_to_store;
    serializeJson(ConfigJSON, data_to_store);
#ifdef DEBUG_GROUPS
    Serial.printf("Guardando datos del grupo en la flash =\n%s\n",data_to_store.c_str());
#endif
    ConfigFile.print(data_to_store);
    ConfigFile.close();
    return true;
}
#endif

Config Configuracion;