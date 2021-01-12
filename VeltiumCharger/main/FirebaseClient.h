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

#include <Arduino.h>
#include <HTTPClient.h>
#include "Update.h"
#include "control.h"
#include "tipos.h"
#include "FirebaseESP32.h"
#include "controlLed.h"
#include "BLEDevice.h"
#include "esp32-hal-psram.h"

void initFirebaseClient();
void GetUpdateFile(String URL);
void CheckForUpdate();
void stopFirebaseClient();
void UpdateFirebaseStatus();
void UpdateFirebaseControl();
uint16  ParseFirmwareVersion(String Texto);

#endif