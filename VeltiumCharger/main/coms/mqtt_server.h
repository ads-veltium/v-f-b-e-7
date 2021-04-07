#ifndef MAIN_MQTT_SERVER_H_
#define MAIN_MQTT_SERVER_H_

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "mongoose.h"

// A list of client, held in memory
struct client {
  struct client *next;
  struct mg_connection *c;
  struct mg_str cid;
};

// A list of subscription, held in memory
struct sub {
  struct sub *next;
  struct mg_connection *c;
  struct mg_str topic;
  uint8_t qos;
};

// A list of will topic & message, held in memory
struct will {
  struct will *next;
  struct mg_connection *c;
  struct mg_str topic;
  struct mg_str payload;
  uint8_t qos;
  uint8_t retain;
};

//Estructura para la creacion de pubs y subs
typedef struct{
  char url[64];
  char *Client_ID;
  char *Will_Topic;
  char *Will_Message;
}mqtt_sub_pub_opts;

void mqtt_server(void *pvParameters);
void mqtt_connect(mqtt_sub_pub_opts *pub_opts);
void mqtt_publisher(void *pvParameters);
void mqtt_subscribe(char* Topic);
void SetStopMQTT(bool value);

#endif /* MAIN_MQTT_SERVER_H_ */
