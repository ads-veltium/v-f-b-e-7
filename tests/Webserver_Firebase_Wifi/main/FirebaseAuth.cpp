////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  FirebaseAuth.cpp
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 27/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FirebaseAuth.h"
#include "FirebaseJson.h"
#include <HTTPClient.h>
#include <Arduino.h>

static const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n"\
"MIIDujCCAqKgAwIBAgILBAAAAAABD4Ym5g0wDQYJKoZIhvcNAQEFBQAwTDEgMB4G\n"\
"A1UECxMXR2xvYmFsU2lnbiBSb290IENBIC0gUjIxEzARBgNVBAoTCkdsb2JhbFNp\n"\
"Z24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMDYxMjE1MDgwMDAwWhcNMjExMjE1\n"\
"MDgwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMjETMBEG\n"\
"A1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjCCASIwDQYJKoZI\n"\
"hvcNAQEBBQADggEPADCCAQoCggEBAKbPJA6+Lm8omUVCxKs+IVSbC9N/hHD6ErPL\n"\
"v4dfxn+G07IwXNb9rfF73OX4YJYJkhD10FPe+3t+c4isUoh7SqbKSaZeqKeMWhG8\n"\
"eoLrvozps6yWJQeXSpkqBy+0Hne/ig+1AnwblrjFuTosvNYSuetZfeLQBoZfXklq\n"\
"tTleiDTsvHgMCJiEbKjNS7SgfQx5TfC4LcshytVsW33hoCmEofnTlEnLJGKRILzd\n"\
"C9XZzPnqJworc5HGnRusyMvo4KD0L5CLTfuwNhv2GXqF4G3yYROIXJ/gkwpRl4pa\n"\
"zq+r1feqCapgvdzZX99yqWATXgAByUr6P6TqBwMhAo6CygPCm48CAwEAAaOBnDCB\n"\
"mTAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUm+IH\n"\
"V2ccHsBqBt5ZtJot39wZhi4wNgYDVR0fBC8wLTAroCmgJ4YlaHR0cDovL2NybC5n\n"\
"bG9iYWxzaWduLm5ldC9yb290LXIyLmNybDAfBgNVHSMEGDAWgBSb4gdXZxwewGoG\n"\
"3lm0mi3f3BmGLjANBgkqhkiG9w0BAQUFAAOCAQEAmYFThxxol4aR7OBKuEQLq4Gs\n"\
"J0/WwbgcQ3izDJr86iw8bmEbTUsp9Z8FHSbBuOmDAGJFtqkIk7mpM0sYmsL4h4hO\n"\
"291xNBrBVNpGP+DTKqttVCL1OmLNIG+6KYnX3ZHu01yiPqFbQfXf5WRDLenVOavS\n"\
"ot+3i9DAgBkcRcAtjOj4LaR0VknFBbVPFd5uRHg5h6h+u/N5GJG79G+dwfCMNYxd\n"\
"AfvDbbnvRG15RjF+Cv6pgsH/76tuIMRQyV+dTZsXjAzlAcmgQWpzU/qlULRuJQ/7\n"\
"TBj0/VLZjmmx6BEP3ojY+x1J96relc8geMJgEtslQIxq/H5COEBkEveegeGTLg==\n"\
"-----END CERTIFICATE-----\n";

static const char* firebaseAuthURL = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=AIzaSyBaJA88_Y3ViCzNF_J08f4LBMAM771aZLs";

bool FirebaseAuth::performAuth(const char* email, const char* password)
{
    if (0 == email || 0 == *email || 0 == password || 0 == *password)
    {
        Serial.println("FirebaseAuth::performAuth: null/empty email/password");
        return false;
    }

    String postData;
    postData += "{\"email\":\"";
    postData += email;
    postData += "\",\"password\":\"";
    postData += password;
    postData += "\",\"returnSecureToken\":true}";

    // Serial.print("POST data: ");
    // Serial.println(postData);

    HTTPClient http;

	http.begin(firebaseAuthURL, root_ca);
	http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(postData);
    String response = http.getString();

    // don't forget to trim response (FirebaseJson doesn't like trailing space or newline)
    response.trim();

    if (httpResponseCode == 200)
    {
        // Serial.println(response+"||||");

        FirebaseJson fjson;
        fjson.setJsonData(response);

        // String fjsonStr;
        // fjson.toString(fjsonStr, false);
        // Serial.println(fjsonStr);
      
        FirebaseJsonData fdata;

        fjson.get(fdata, "localId");
        _localId = fdata.stringValue;

        fjson.get(fdata, "idToken");
        _idToken = fdata.stringValue;

        fjson.get(fdata, "email");
        _email = fdata.stringValue;

        return true;
    }
    else
    {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
        Serial.println("Full response: ");
        Serial.println(response);
    }
    

    return false;
}




#ifdef COMPILE_JSON_TESTS

static const char* myJSON = ""\
"{\n"\
"  \"kind\": \"identitytoolkit#VerifyPasswordResponse\",\n"\
"  \"localId\": \"WW1zp2ptgIOK2zxFUWUzjcDTFFq1\",\n"\
"  \"email\": \"grantasca@yahoo.es\",\n"\
"  \"displayName\": \"\",\n"\
"  \"idToken\": \"eyJhbGciOiJSUzI1NiIsImtpZCI6IjgyMmM1NDk4YTcwYjc0MjQ5NzI2ZDhmYjYxODlkZWI3NGMzNWM4MGEiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJodHRwczovL3NlY3VyZXRva2VuLmdvb2dsZS5jb20vdmVsdGl1bWJhY2tlbmQiLCJhdWQiOiJ2ZWx0aXVtYmFja2VuZCIsImF1dGhfdGltZSI6MTU5MDY2MTI2MSwidXNlcl9pZCI6IldXMXpwMnB0Z0lPSzJ6eEZVV1V6amNEVEZGcTEiLCJzdWIiOiJXVzF6cDJwdGdJT0syenhGVVdVempjRFRGRnExIiwiaWF0IjoxNTkwNjYxMjYxLCJleHAiOjE1OTA2NjQ4NjEsImVtYWlsIjoiZ3JhbnRhc2NhQHlhaG9vLmVzIiwiZW1haWxfdmVyaWZpZWQiOnRydWUsImZpcmViYXNlIjp7ImlkZW50aXRpZXMiOnsiZW1haWwiOlsiZ3JhbnRhc2NhQHlhaG9vLmVzIl19LCJzaWduX2luX3Byb3ZpZGVyIjoicGFzc3dvcmQifX0.BobAey062GP3urGNEtiLaE6toXbDjxochZrPyfY9j-yJFixZgM7gQKKQXu6J3r40psbS1v8BzogFMY51ctenQ67Ui7WY3q66vUEsBzYudQ7Lq9iUeUW_O4gFN_Ex4IUN6pq0hR7iqaqO-4Y6BJKYaJS2OOZkv0j46RV3EZAadmsMBVQAsz09NZbWj8cw1ZQii-tHDdV1nvaWWffl4kuP5LhfTD7B-gaNyK6V02R04bQWmFxKX5XPS0n6OJE_NTALt97ILXM3szBpWRtkyYPO77x770hi4IFANbdZnAdLhaVPnFGy6mI6URDzAoMAOK903lro7-H3pJxCQKw5eRgoxQ\",\n"\
"  \"registered\": true,\n"\
"  \"refreshToken\": \"AE0u-Nf3xoqhmLd6ZaZWw5TZduWbxmO_IDYoEZWSUN9CP1_SZic-W0wnnyyIhdBGivq5kjuc2zOXRzdnFyjCb2obKJnDZwxRBgdgBH1nJRXgBQObYzN994NGD3n5vQ0RFJ9-_wwzlq964X8Y9QxAHMgdK-2Q1i7dX4DKXN7rWO6nA-mcFraRSYnrnR1I9UJxbfVBhTduxGtd373PPc--0mPBsvD0K0pjEw\",\n"\
"  \"expiresIn\": \"3600\"\n"\
"}";

// Silly PITFALL which made me lose 1+ hours:
// JSON String must have NO WHITESPACE after the final curly brace
// must finish with "}", but not with "} " or "}\n"

void FirebaseAuth::testJSON()
{
    Serial.println(myJSON);
    Serial.println("********************************");


    FirebaseJson fjson;
    fjson.setJsonData(myJSON);
    //Print out as prettify string
    String fjsonStr;
    fjson.toString(fjsonStr, false);
    Serial.println(fjsonStr);

    FirebaseJsonData idToken;
    fjson.get(idToken, "idToken");

    Serial.println("idToken = ");
    Serial.println(idToken.type);
    Serial.println(idToken.stringValue);
}

#endif // COMPILE_JSON_TESTS

/*
Error catalog:

1) Send an invalid API key
Error on sending POST: 400
Full response:
{
  "error": {
    "code": 400,
    "message": "API key not valid. Please pass a valid API key.",
    "errors": [
      {
        "message": "API key not valid. Please pass a valid API key.",
        "domain": "global",
        "reason": "badRequest"
      }
    ],
    "status": "INVALID_ARGUMENT"
  }
}

2) Send bad URL (ex: aidentitytoolkit... instead of identitytoolkit...)
[E][ssl_client.cpp:36] _handle_error(): [data_to_read():273]: (-80) UNKNOWN ERROR CODE (0050)
Error on sending POST: 404
Full response:
<!DOCTYPE html>
<html lang=en>
  <meta charset=utf-8>
  <meta name=viewport content="initial-scale=1, minimum-scale=1, width=device-width">
  <title>Error 404 (Not Found)!!1</title>
  <style>
    *{margin:0;padding:0}html,code{font:15px/22px arial,sans-serif}html{background:#fff;color:#222;padding:15px}body{margin:7% auto 0;max-width:390px;min-height:180px;padding:30px 0 15px}* > body{background:url(//www.google.com/images/errors/robot.png) 100% 5px no-repeat;padding-right:205px}p{margin:11px 0 22px;overflow:hidden}ins{color:#777;text-decoration:none}a img{border:0}@media screen and (max-width:772px){body{background:none;margin-top:0;max-width:none;padding-right:0}}#logo{background:url(//www.google.com/images/branding/googlelogo/1x/googlelogo_color_150x54dp.png) no-repeat;margin-left:-5px}@media only screen and (min-resolution:192dpi){#logo{background:url(//www.google.com/images/branding/googlelogo/2x/googlelogo_color_150x54dp.png) no-repeat 0% 0%/100% 100

3) bad email:
Error on sending POST: 400
Full response:
{
  "error": {
    "code": 400,
    "message": "EMAIL_NOT_FOUND",
    "errors": [
      {
        "message": "EMAIL_NOT_FOUND",
        "domain": "global",
        "reason": "invalid"
      }
    ]
  }
}

4) bad password:
Error on sending POST: 400
Full response:
{
  "error": {
    "code": 400,
    "message": "INVALID_PASSWORD",
    "errors": [
      {
        "message": "INVALID_PASSWORD",
        "domain": "global",
        "reason": "invalid"
      }
    ]
  }
}

*/
