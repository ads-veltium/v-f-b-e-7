#ifndef __COMMS
#define __COMMS

// IMPORTANTE: el ssid de la wifi va aquí
#define WIFI_SSID "VELTIUM_WF"
#define WIFI_PASSWORD "W1f1d3V3lt1um$m4rtCh4rg3rs!"



void ServerInit();
void CommsControlTask(void *arg) ;
#endif // __COMMS