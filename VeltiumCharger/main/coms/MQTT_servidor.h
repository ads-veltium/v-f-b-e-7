#ifndef MQTT_SERVER_H
#define MQTT_SERVER_H

#include "../control.h"
#include "esp_log.h"
#include "mdns.h"

#include "mongoose.h"
extern "C" {
#include "mqtt_server.h"
}

void start_MQTT_server();

#endif