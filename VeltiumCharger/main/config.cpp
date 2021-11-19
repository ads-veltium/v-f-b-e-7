#include "config.h"

//**********Comparadores de las caracteristicas**********/
static bool operator==(const carac_config& lhs, const carac_config& rhs){
    if(lhs.Firmware != rhs.Firmware){
      return false;
    }
    if(lhs.Part_Number != rhs.Part_Number){
      return false;
    }
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
    if(memcmp(lhs.autentication_mode, rhs.autentication_mode,2)){
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
        delay(50);

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

void Config::Carac_to_json(){
    ConfigJSON.clear();
    ConfigJSON["fw_esp"]      = data.Firmware;
    ConfigJSON["part_number"] = data.Part_Number;
    ConfigJSON["auth_mode"] = String(data.autentication_mode);
    ConfigJSON["pot_contratada_1"] = data.potencia_contratada1;
    ConfigJSON["pot_contratada_2"] = data.potencia_contratada2;
    ConfigJSON["inst_curr_limit"] = data.inst_current_limit;
    ConfigJSON["CDP"] = data.CDP;
}

void Config::Json_to_carac(){
    data.Firmware       = ConfigJSON["fw_esp"].as<String>();
    data.Part_Number    = ConfigJSON["part_number"].as<String>();

    data.potencia_contratada1 = ConfigJSON["pot_contratada_1"].as<uint16_t>();
    data.potencia_contratada2 = ConfigJSON["pot_contratada_2"].as<uint16_t>();
    data.inst_current_limit   = ConfigJSON["inst_curr_limit"].as<uint8_t>();

    data.CDP = ConfigJSON["CDP"].as<uint8_t>();

    memcpy(data.autentication_mode, ConfigJSON["auth_mode"].as<String>().c_str(),2);
}

//**********Funciones externas de la case de configuracion**************/
void Config::init(){
    //Arrancar el SPIFFS
    if(!SPIFFS.begin(false,"/spiffs",1,"CONFIG")){
      SPIFFS.end();					
      SPIFFS.begin(false,"/spiffs",1,"CONFIG");
    }

    //Sino existe el archivo, crear el de defecto
    if(!SPIFFS.exists("/config.json")){
      SPIFFS.format();
      Store();
    }

    //Abrir el archivo para lectura y 
    Load();

    xTaskCreate(ConfigTask,"Task CONFIG",4096*2,NULL,PRIORIDAD_CONFIG,NULL);

}

bool Config::Load(){
   
    ConfigFile = SPIFFS.open("/config.json", FILE_READ);
    String data_to_read;
    data_to_read = ConfigFile.readString();
    deserializeJson(ConfigJSON, data_to_read);
    ConfigFile.close();
    Json_to_carac();

    #ifdef DEBUG_CONFIG
        printf("Cargando datos desde la flash!!\n");
        serializeJsonPretty(ConfigJSON, Serial);
    #endif
    return true;
}

bool Config::Store(){
    #ifdef DEBUG_CONFIG
        printf("Guardando datos a la flash!!\n");
    #endif
    Carac_to_json();
    ConfigFile = SPIFFS.open("/config.json", FILE_WRITE);
    String data_to_store;
    serializeJson(ConfigJSON, data_to_store);
    ConfigFile.print(data_to_store);
    ConfigFile.close();

    return true;
}

Config Configuracion;