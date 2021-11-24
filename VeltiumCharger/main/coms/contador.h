#ifndef Contador_h
#define Contador_h

#include "VeltFirebase.h"
#include "ArduinoJson.h"
#include "../control.h"
#include "Eth_Station.h"
#include "helper_coms.h"

class Contador{
    
    String CounterUrl= "http://";
    StaticJsonDocument<4096> Measurements;
  public:
    
    bool Inicializado = false;

    void find();
    void begin(String Host);
    bool read();
    void end();
    void parse();
};


#endif