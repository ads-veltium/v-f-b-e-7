#ifndef Contador_h
#define Contador_h

#include "ArduinoJson.h"
#include "../control.h"
#include "Eth_Station.h"
#include "helper_coms.h"
#include "esp_http_client.h"

class Contador{
  String MeterUrl = "http://";
  StaticJsonDocument<4096> Measurements;
public:
  bool Inicializado = false;
  void find();
  void begin(String Host);
  bool read();
  void end();
  void parse();
};

class Meter_HTTP_Client{
    String _url;
    esp_http_client_handle_t _client; 
    String _response;
    int  _timeout;
    
  public:
    #define METER_WRITE     0
    #define METER_UPDATE    1
    #define METER_TIMESTAMP 4
    #define METER_READ      5
    
    //Functions
    bool Send_Command(String url,  uint8_t Command);
    void begin();
    void end();
    String ObtenerRespuesta();
    void set_url(String url);

    //Constructor
    Meter_HTTP_Client(String url, int Timeout){
      _url=url;
      _timeout = Timeout;
    }
};

#endif