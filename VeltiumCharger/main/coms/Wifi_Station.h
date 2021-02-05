#ifndef WIFI_STATION_H_
#define WIFI_STATION_H_

#include "Arduino.h"
#include "WiFi.h"
#include "esp32-hal-psram.h"
#include "../control.h"
#include "FirebaseClient.h"
#include "WiFiProv.h"

#define WIFI_SSID "VELTIUM_WF"
#define WIFI_PASSWORD "W1f1d3V3lt1um$m4rtCh4rg3r$!"


#ifdef USE_ETH
    #include <ETH.h>
    #include "ESPAsyncWebServer.h"

    //#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN
    #define ETH_POWER_PIN  	12 
    #define ETH_TYPE        ETH_PHY_LAN8720
    #define ETH_ADDR        0
    #define ETH_MDC_PIN    	23 
    #define ETH_MDIO_PIN    18


    static bool eth_connected = false;
#endif // USE_ETH

void InitServer(void) ;
void ETH_begin();
void Station_Begin();
void ESP_Station_begin();
void Station_Pause();
void Station_Resume();
void Station_Scan();
void ComsTask(void *args);


#endif