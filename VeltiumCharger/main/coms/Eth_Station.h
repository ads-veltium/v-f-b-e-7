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
#include "WiFi.h"
#include "contador.h"
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

typedef struct Parametros_Ping
{
  bool NextOne  = false;
  bool Found    = false;
  bool Finished = false;
  bool CheckingConn = false;

  ip4_addr_t BaseAdress;
  uint32_t ping_count = 2;      //how many pings per report
  uint32_t ping_timeout = 200; //mS till we consider it timed out
  uint32_t ping_delay = 10; //mS between pings
  uint8_t maxTimeout = 50; //mS Para considerar que un equipo a contestado
} xParametrosPing;

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