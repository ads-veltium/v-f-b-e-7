////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  FirebaseAuth.h
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 27/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////


// #define COMPILE_JSON_TESTS

#include <Arduino.h>

class FirebaseAuth
{
public:
    bool performAuth(const char* email, const char* password);

    // accessors for auth results after successful auth
    String& getEmail  () { return _email;   }
    String& getLocalId() { return _localId; }
    String& getIdToken() { return _idToken; }
    int getExpirationTime(); // TBD

#ifdef COMPILE_JSON_TESTS
    static void testJSON();
#endif // COMPILE_JSON_TESTS

private:
    String _email;
    String _localId;
    String _idToken;
};