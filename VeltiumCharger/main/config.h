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
#ifdef CONNECTED
        void Group_Json_to_carac(DynamicJsonDocument& ConfigJSON);
        void Group_Carac_to_json(DynamicJsonDocument& ConfigJSON);       
#endif
        File ConfigFile;
    
    public:
        //ordenes de guardado y carga de datos
        bool Guardar = false;
        bool Cargar = false;
#ifdef CONNECTED
        bool Group_Guardar = false;
        bool Group_Cargar = false;
#endif
        //Inicializador
        void init();

        //Atributos
        carac_config data;
        carac_group_config group_data;
        
        //Acciones sobre el archivo al completo
        bool Load();
        bool Store();
#ifdef CONNECTED
        bool Group_Load();
        bool Group_Store();
#endif
        bool Erase();

        //Acciones sobre claves unitarias
        bool ModifyKey();
        bool DeleteKey();
        bool AddKey();

};

extern Config Configuracion;


#endif