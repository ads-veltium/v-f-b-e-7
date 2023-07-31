#ifndef ETH_STATION_h
#define ETH_STATION_h


#include <string.h>
#include <stdlib.h>
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "../control.h"
#include "esp_netif.h"
#include "esp_eth_netif_glue.h"
#include "esp_http_client.h"
#include "Wifi_Station.h"

#define ETH_POWER_PIN  	12 
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_ADDR        0
#define ETH_MDC_PIN    	23 
#define ETH_MDIO_PIN    18

//Prototipos de funciones
void BuscarContador_Task(void *args);
void initialize_ethernet(void);
void initialize_ethernet_1(void);
void initialize_ethernet_2(void);
bool stop_ethernet(void);
void kill_ethernet(void);
bool restart_ethernet(void);
bool ComprobarConexion(void);
#endif