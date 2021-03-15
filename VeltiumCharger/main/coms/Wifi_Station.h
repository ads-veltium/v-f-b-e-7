#ifndef WIFI_STATION_H_
#define WIFI_STATION_H_

#include "Arduino.h"
#include "WiFi.h"
#include "../control.h"
#include "FirebaseClient.h"
#include "ESPAsyncWebServer.h"
#include "Eth_Station.h"
#include "SmartConfig.h"

void InitServer(void) ;
void Station_Begin();
void Delete_Credentials();
void Station_Pause();
void Station_Resume();
void Station_Scan();
void ComsTask(void *args);

#endif
