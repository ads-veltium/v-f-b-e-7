#include "config.h"


//**********Comparadores de las caracteristicas**********/
static bool operator==(const carac_config& lhs, const carac_config& rhs){
    if(lhs.Firmware != rhs.Firmware){
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
    if(lhs.inst_current_limit != rhs.inst_current_limit){
      return false;
    }
    if(lhs.CDP != rhs.CDP){
      return false;
    }
    if(lhs.count_reinicios_malos != rhs.count_reinicios_malos){
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
    }
}

//**********Funciones internas de la case de configuracion**************/

void Config::Carac_to_json(DynamicJsonDocument& ConfigJSON){
    ConfigJSON.clear();
    ConfigJSON["fw_esp"]      = data.Firmware;
    ConfigJSON["fw_psoc"]     = data.FirmwarePSOC;
    //ConfigJSON["part_number"] = data.Part_Number;
    ConfigJSON["auth_mode"] = String(data.autentication_mode);
    ConfigJSON["pot_contratada_1"] = data.potencia_contratada1;
    ConfigJSON["pot_contratada_2"] = data.potencia_contratada2;
    ConfigJSON["inst_curr_limit"] = data.inst_current_limit;
    ConfigJSON["CDP"] = data.CDP;
    ConfigJSON["device_ID"]      = String(data.device_ID);
    ConfigJSON["device_ser_num"] = String(data.deviceSerNum);
    ConfigJSON["policy"] = String(data.policy);
    ConfigJSON["data_cleared"] = data.Data_cleared;
    ConfigJSON["count_reinicios_malos"] = data.count_reinicios_malos;
}

void Config::Json_to_carac(DynamicJsonDocument& ConfigJSON){
    data.Firmware       = ConfigJSON["fw_esp"].as<uint16_t>();
    data.FirmwarePSOC   = ConfigJSON["fw_psoc"].as<uint16_t>();
    //data.Part_Number    = ConfigJSON["part_number"].as<String>();

    data.potencia_contratada1 = ConfigJSON["pot_contratada_1"].as<uint16_t>();
    data.potencia_contratada2 = ConfigJSON["pot_contratada_2"].as<uint16_t>();
    data.inst_current_limit   = ConfigJSON["inst_curr_limit"].as<uint8_t>();

    data.CDP = ConfigJSON["CDP"].as<uint8_t>();

    memcpy(data.autentication_mode, ConfigJSON["auth_mode"].as<String>().c_str(),2);
    memcpy(data.device_ID, ConfigJSON["device_ID"].as<String>().c_str(),sizeof(data.device_ID));
    memcpy(data.deviceSerNum, ConfigJSON["device_ser_num"].as<String>().c_str(),sizeof(data.deviceSerNum));
    memcpy(data.policy, ConfigJSON["policy"].as<String>().c_str(),3);
    data.Data_cleared = ConfigJSON["data_cleared"].as<uint8_t>(); 
    data.count_reinicios_malos = ConfigJSON["count_reinicios_malos"].as<uint8_t>(); 
}

//**********Funciones externas de la case de configuracion**************/
void Config::init(){
  
    //Arrancar el SPIFFS
    if(!SPIFFS.begin(1,"/spiffs",1,"ESP32")){
      SPIFFS.end();					
      SPIFFS.begin(1,"/spiffs",1,"ESP32");
    }

    //Sino existe el archivo, crear el de defecto
    if(!SPIFFS.exists("/config.json")){
      Store();
    }

    //Abrir el archivo para lectura y cargar los datos
    Load();

    //Crear la tarea encargada de detectar y almacenar cambios
    xTaskCreate(ConfigTask,"Task CONFIG",4096*2,NULL,PRIORIDAD_CONFIG,NULL);

    //Publicar las caracteristicas que solo se almacenan en el ble, 
    //comprobando que tengan valores logicos.
    if(memcmp(Configuracion.data.policy, "ALL",3) && memcmp(Configuracion.data.policy,"AUT",3) && memcmp(Configuracion.data.policy, "NON",3)){
        printf("Cambiando el policy porque no tenia!\n");
        memcpy(Configuracion.data.policy,"ALL",3);
    }
    
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
    DynamicJsonDocument ConfigJSON(1024);
    #ifdef DEBUG_CONFIG
        printf("Guardando datos a la flash!!\n");
    #endif
    Carac_to_json(ConfigJSON);
    ConfigFile = SPIFFS.open("/config.json", FILE_WRITE);
    String data_to_store;
    serializeJson(ConfigJSON, data_to_store);
    ConfigFile.print(data_to_store);
    ConfigFile.close();

    return true;
}

Config Configuracion;