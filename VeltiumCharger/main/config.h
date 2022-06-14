#ifndef __CONFIG__
#define __CONFIG__

#include "SPIFFS.h"
#include "ArduinoJson.h"
#include "control.h"

//Clase para gestionar la configuración del equipo, 
class Config {
    private:
        void Json_to_carac(DynamicJsonDocument& ConfigJSON);
        void Carac_to_json(DynamicJsonDocument& ConfigJSON);       

        File ConfigFile;
    
    public:
        //ordenes de guardado y carga de datos
        bool Guardar = false, Cargar = false;

        //Inicializador
        void init();

        //Atributos
        carac_config data;
        
        //Acciones sobre el archivo al completo
        bool Load();
        bool Store();
        bool Erase();

        //Acciones sobre claves unitarias
        bool ModifyKey();
        bool DeleteKey();
        bool AddKey();

};

extern Config Configuracion;


#endif