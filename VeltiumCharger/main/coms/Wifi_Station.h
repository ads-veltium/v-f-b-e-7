#ifndef WIFI_STATION_H
#define WIFI_STATION_H

#include "Eth_Station.h"
#include "../control.h"
#include "FirebaseClient.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "../control.h"
#include "wifi_provisioning/scheme_softap.h"

void initialise_smartconfig(void);
void start_wifi(void);
void stop_wifi(void);
void initialise_provisioning(void);
void ComsTask(void *args);

#endif