////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  FirebaseClient.h
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 28/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef VSC_ESP32_FirebaseClient_h
#define VSC_ESP32_FirebaseClient_h

#include <Arduino.h>

class FirebaseClient
{
public:
    bool initialize(const char* email, const char* password);

    // accessors for auth results after successful init
    String& getEmail  () { return _email;   }
    String& getLocalId() { return _localId; }
    String& getIdToken() { return _idToken; }
    int getExpirationTime();  // TBD

    String getUserDeviceListJSONString();


    void testVeltiumClient();

private:
    bool performAuth(const char* email, const char* password);
    String _email;
    String _localId;
    String _idToken;

    void testListenAtBranch();

    void testSetJSONRecord();
};

#endif // VSC_ESP32_FirebaseClient_h