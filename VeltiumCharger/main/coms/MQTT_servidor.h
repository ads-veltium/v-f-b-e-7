#ifndef MQTT_SERVER_H
#define MQTT_SERVER_H

#include "../control.h"
#include "esp_log.h"
#include "mdns.h"


extern "C" {
#include "mongoose.h"
#include "mqtt_server.h"
}

typedef struct{
    carac_charger charger_table[10];
    int size = 0;
}carac_chargers;

void start_MQTT_server();
void stop_MQTT();
void start_udp();

#endif