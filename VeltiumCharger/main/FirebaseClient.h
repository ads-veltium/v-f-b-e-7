////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  FirebaseClient.h
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 26/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef VSC_FirebaseClient_h
#define VSC_FirebaseClient_h

#include <HTTPClient.h>
#include "Update.h"
#include "control.h"
#include "Firebase_ESP_Client.h"
#include "esp32-hal-psram.h"

#define DISCONNECTED     0
#define CONNECTING       5
#define CONNECTED       10
#define IDLE            20
#define READING         25
#define WRITTING        35
#define DISCONNECTING   35

void initFirebaseClient();
void GetUpdateFile(String URL);
void CheckForUpdate();
void stopFirebaseClient();
void UpdateFirebaseStatus();
void resumeFirebaseClient();
void pauseFirebaseClient();
void UpdateFirebaseControl_Task(void *arg);
uint8_t getfirebaseClientStatus();
uint16  ParseFirmwareVersion(String Texto);

#endif