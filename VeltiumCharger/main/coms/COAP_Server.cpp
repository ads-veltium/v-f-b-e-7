
#include <coap-simple.h>
#include <WiFiUdp.h>
#include "../control.h"
#define LEDP 9

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Params Params;
extern carac_group  ChargingGroup;
extern carac_Comands  Comands ;


// UDP and CoAP class
WiFiUDP  Udp;
Coap coap(Udp);

// CoAP server endpoint URL
void callback_RTS(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("RTS");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  String message(p);

  Serial.println(message);
  Serial.println(ip);
  Serial.println(port);   

  coap.sendResponse(ip, port, packet.messageid, "0");
}

void callback_DATA(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("RTS");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  String message(p);

  Serial.println(message);
  Serial.println(ip);

  coap.sendResponse(ip, port, packet.messageid, "0");
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Coap Response got]");
  
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  Serial.println(p);
}

void coap_loop(void *args) {
    coap.put(IPAddress(192, 168, 20, 163), 5683, "RTS", "asdf");
    while(1){   
    
    Serial.println("Pidendo datos");
    coap.get(IPAddress(192, 168, 20, 163), 5683, "RTS");

    delay(1000);
    coap.loop();
    }
}

void coap_stop(){
    coap.
}


void coap_start() {

  if(ChargingGroup.Params.GroupMaster){
      coap.server(callback_RTS, "RTS");
      coap.server(callback_DATA, "Data");
  }
  
  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
  
  xTaskCreate(coap_loop,"coap", 4096,NULL,2,NULL);

}

