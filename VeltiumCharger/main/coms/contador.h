#ifndef Contador_h
#define Contador_h

#define ARDUINOJSON_USE_LONG_LONG 1

#include "VeltFirebase.h"
#include "ArduinoJson.h"
#include "../control.h"
#include "Eth_Station.h"


class Contador{
    
    String CounterUrl= "http://";
    StaticJsonDocument<2048> Measurements;
  public:
    
    bool Inicializado = false;

    void find();
    void begin(String Host);
    bool read();
    void end();
    void parse();
};


#endif