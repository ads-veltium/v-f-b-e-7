
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



// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port);

// CoAP server endpoint url callback
void callback_light(CoapPacket &packet, IPAddress ip, int port);

// UDP and CoAP class
WiFiUDP  Udp;
Coap coap(Udp);

// LED STATE
bool LEDSTATE;

// CoAP server endpoint URL
void callback_light(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Light] ON/OFF");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  String message(p);

  if (message.equals("0"))
    LEDSTATE = false;
  else if(message.equals("1"))
    LEDSTATE = true;
      
  if (LEDSTATE) {
    digitalWrite(LEDP, HIGH) ; 
    coap.sendResponse(ip, port, packet.messageid, "1");
  } else { 
    digitalWrite(LEDP, LOW) ; 
    coap.sendResponse(ip, port, packet.messageid, "0");
  }
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
    coap.put(IPAddress(192, 168, 20, 163), 5683, "light", "1");
    while(1){
        // send GET or PUT coap request to CoAP server.
        // To test, use libcoap, microcoap server...etc
        
        Serial.println("Send Request");
        int msgid = coap.get(IPAddress(192, 168, 20, 163), 5683, "light");
        printf("%i\n",msgid);

        delay(1000);
        coap.loop();
        }
    }


void coap_start() {

  coap.server(callback_light, "light");

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
  
  xTaskCreate(coap_loop,"coap", 4096,NULL,2,NULL);

}

void coap_client() {
  
  coap.server(callback_light, "light");

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
  
  xTaskCreate(coap_loop,"coap", 4096,NULL,2,NULL);

}

