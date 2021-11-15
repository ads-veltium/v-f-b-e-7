#ifndef __CONFIG__
#define __CONFIG__

#include "tipos.h"
#include "SPIFFS.h"
#include "ArduinoJson.h"

//Clase para gestionar la configuración del equipo, 
class Config {
    private:
        void init();
        File ConfigFile;
        StaticJsonDocument<2048>  ConfigJSON;

    public:

        //Inicializador
        Config(){init();};

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



#endif