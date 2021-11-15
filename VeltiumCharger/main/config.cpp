#include "config.h"


void Config::init(){
    printf("Has llamado al constructor!!\n");

    //Arrancar el SPIFFS
    if(!SPIFFS.begin(1,"/spiffs",1,"CONFIG")){
      SPIFFS.end();					
      SPIFFS.begin(1,"/spiffs",1,"CONFIG");
    }

    //Sino existe el archivo, crear el de defecto
    if(!SPIFFS.exists("/config.json")){
      SPIFFS.format();
      Store();
    }

    //Abrir el archivo para lectura y 
    Load();

}

bool Config::Load(){
    printf("Cargando datos desde la flash!!\n");
    ConfigFile = SPIFFS.open("config.json", FILE_READ);
    String data_to_read;
    data_to_read = ConfigFile.readString();
    deserializeJson(ConfigJSON, data_to_read);
    ConfigFile.close();

    serializeJson(ConfigJSON, Serial);
    return true;
}

bool Config::Store(){
    printf("Guardando datos a la flash!!\n");

    ConfigFile = SPIFFS.open("config.json", FILE_WRITE);
    String data_to_store;
    serializeJson(ConfigJSON, data_to_store);
    ConfigFile.print(data_to_store);
    ConfigFile.close();

    return true;
}

bool Config::AddKey(){
    return true;
}

bool Config::ModifyKey(){
    return true;
}

bool Config::DeleteKey(){
    return true;
}

