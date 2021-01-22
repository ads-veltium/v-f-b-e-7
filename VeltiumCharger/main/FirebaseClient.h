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

#define DISCONNECTED      0
#define CONNECTING        5
#define CONECTADO        10
#define IDLE             20
#define READING_CONTROL  25
#define READING_PARAMS   26
#define READING_COMS     27
#define WRITTING_CONTROL 35
#define WRITTING_STATUS  36
#define WRITTING_PARAMS  37
#define WRITTING_COMS    38
#define DISCONNECTING    45
#define DOWNLOADING      55
#define UPDATING         65
#define INSTALLING       70

bool initFirebaseClient();
void GetUpdateFile(String URL);
bool stopFirebaseClient();
void Firebase_Conn_Task(void *args);
uint8_t getfirebaseClientStatus();
uint16  ParseFirmwareVersion(String Texto);

#endif