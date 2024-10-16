#ifndef VSC_FirebaseClient_h
#define VSC_FirebaseClient_h

#include "Update.h"
#include "../control.h"
#include "esp32-hal-psram.h"
#include "WiFi.h"
#include "HTTPClient.h"

bool initFirebaseClient();
void Firebase_Conn_Task(void *args);
uint8_t deltaprogram(uint8_t s, uint8_t e);
bool WriteFirebaseHistoric(char* buffer);
bool WriteFirebaseLastRecordSynced(uint8_t last_rec_in_mem, uint8_t rec_index, uint8_t rec_lap);
bool WriteFirebaseOlderSyncRecord (uint8_t rec_index);
bool ReadFirebaseOlderSyncRecord();
bool ReiniciarMultiusuario();

#endif