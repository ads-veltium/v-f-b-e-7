#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include "COAP_Server.h"


// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port);

// CoAP server endpoint url callback
void callback_light(CoapPacket &packet, IPAddress ip, int port);

// UDP and CoAP class
WiFiUDP udp;
Coap coap(udp);


// CoAP server endpoint URL
void callback_light(CoapPacket &packet, IPAddress ip, int port) {

  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  String message(p);
  Serial.println(message);
  coap.sendResponse(ip,port,packet.messageid,"KOXKA");

}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {

  
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  Serial.printf("El cargador con la IP %s responde: %s \n", ip.toString().c_str(), p);

}

void Inicializacion() {

  
  // add server url endpoints.
  // can add multiple endpoint urls.
  // exp) coap.server(callback_switch, "switch");
  //      coap.server(callback_env, "env/temp");
  //      coap.server(callback_env, "env/humidity");
  Serial.println("Setup Callback VCD");
  coap.server(callback_light, "VCD");

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
}

/*
if you change LED, req/res test with coap-client(libcoap), run following.
coap-client -m get coap://(arduino ip addr)/light
coap-client -e "1" -m put coap://(arduino ip addr)/light
coap-client -e "0" -m put coap://(arduino ip addr)/light
*/

static void coap_example_server(void *p)
{
    // send GET or PUT coap request to CoAP server.
    // To test, use libcoap, microcoap server...etc
    // int msgid = coap.put(IPAddress(10, 0, 0, 1), 5683, "light", "1");
    while(1){
      //int msgid = coap.get(IPAddress(192, 168, 20, 192), 5683, "time");

      delay(1000);
      coap.loop();
    }
}

void coap_start(void)
{
    Inicializacion();
    xTaskCreate(coap_example_server, "coap", 8 * 1024, NULL, 5, NULL);
}