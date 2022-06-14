#ifndef VSC_FirebaseClient_h
#define VSC_FirebaseClient_h

#include "Update.h"
#include "../control.h"
#include "esp32-hal-psram.h"
#include "WiFi.h"
#include "HTTPClient.h"

bool initFirebaseClient();
void GetUpdateFile(String URL);
void Firebase_Conn_Task(void *args);
uint8_t getfirebaseClientStatus();
bool WriteFirebaseHistoric(char* buffer);
bool ReiniciarMultiusuario();

#endif