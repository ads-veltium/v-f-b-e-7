#ifndef WIFI_STATION_H_
#define WIFI_STATION_H_

#include "Arduino.h"
#include "WiFi.h"
#include "CPPNVS.h"
#include "esp32-hal-psram.h"
#include "tipos.h"
#include "FirebaseClient.h"

#define WIFI_SSID "VELTIUM_WF"
#define WIFI_PASSWORD "W1f1d3V3lt1um$m4rtCh4rg3r$!"


void Station_Begin();
void Station_Pause();
void Station_Resume();
void Station_Scan();


#endif