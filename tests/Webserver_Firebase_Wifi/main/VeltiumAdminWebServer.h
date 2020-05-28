////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  VeltiumAdminWebServer.h
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 28/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef VSC_ESP32_VeltiumAdminWebServer_h
#define VSC_ESP32_VeltiumAdminWebServer_h

#include <Arduino.h>

class VeltiumAdminWebServer
{
public:
    static void initialize();
    static void handleClient();
};

#endif // VSC_ESP32_VeltiumAdminWebServer_h