#ifndef ETH_STATION_h
#define ETH_STATION_h

#include <string.h>
#include <stdlib.h>
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "WiFi.h"

#define ETH_POWER_PIN  	12 
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_ADDR        0
#define ETH_MDC_PIN    	23 
#define ETH_MDIO_PIN    18

//Prototipos de funciones
void initialize_ethernet(void);
bool stop_ethernet(void);
bool restart_ethernet(void);
#endif