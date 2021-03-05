#ifndef Contador_h
#define Contador_h

#define ARDUINOJSON_USE_LONG_LONG 1
#include "HTTPClient.h"
#include "ArduinoJson.h"
#include "../control.h"
#include "WiFi.h"

class Contador{
    HTTPClient CounterClient; 

    String CounterUrl= "http://";
    StaticJsonDocument<2048> Measurements;
  public:

    void begin(String Host);
    void read();
    void end();
    void parse();
};


#endif