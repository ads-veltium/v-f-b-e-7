////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  FirebaseClient.cpp
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 28/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FirebaseClient.h"
#include "FirebaseESP32.h"
#include "FirebaseJson.h"
#include <HTTPClient.h>

static const char root_ca[] = \
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

static const char firebaseAuthURLProlog[] = \
"https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";

static const char veltiumFirebaseProject[] = \
"veltiumbackend.firebaseio.com";

static const char veltiumFirebaseApiKey[] = \
"AIzaSyBaJA88_Y3ViCzNF_J08f4LBMAM771aZLs";

static FirebaseData firebaseData;

bool FirebaseClient::initialize(const char* email, const char* password)
{
    if (0 == email || 0 == *email || 0 == password || 0 == *password)
    {
        Serial.println("FirebaseClient::initialize: null/empty email/password");
        return false;
    }

	bool authResult = performAuth(email, password);
	if (authResult)
	{
		Serial.println("FirebaseClient::initialize: Auth succeeded.");
		Serial.println("LocalId:");
		Serial.println(_localId);
		Serial.println("IdToken:");
		Serial.println(_idToken+"||||");
	}
	else
	{
		Serial.println("FirebaseClient::initialize: Auth FAILED.");
        return false;
	}

    Firebase.begin(veltiumFirebaseProject, _idToken);

    Firebase.reconnectWiFi(true);

    //Set database read timeout to 1 minute (max 15 minutes)
    Firebase.setReadTimeout(firebaseData, 1000 * 60);
    //tiny, small, medium, large and unlimited.
    //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
    Firebase.setwriteSizeLimit(firebaseData, "tiny");

    return true;
}

bool FirebaseClient::performAuth(const char* email, const char* password)
{
    String postData;
    postData += "{\"email\":\"";
    postData += email;
    postData += "\",\"password\":\"";
    postData += password;
    postData += "\",\"returnSecureToken\":true}";

    // Serial.print("POST data: ");
    // Serial.println(postData);

    HTTPClient http;

    String firebaseAuthURL = firebaseAuthURLProlog;
    firebaseAuthURL += veltiumFirebaseApiKey;

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

String FirebaseClient::getUserDeviceListJSONString()
{
    Serial.println("FirebaseClient::getUserDeviceListJSONString()");
    String path = "/test/users/";
    path += _localId;
    path += "/devices";
    Serial.println(path);
    bool ok = Firebase.get(firebaseData, path);
    if (ok) {
        Serial.println("...OK");
        return firebaseData.payload();
    }
    else {
        Serial.println("...FAIL");
        return "{}";
    }
}

void FirebaseClient::testVeltiumClient()
{
	String str;
	FirebaseJson json;
    bool result;

    result = Firebase.setDouble(firebaseData, "/test/esp32test/doubletest_6", 3.141592);
	if (result) Serial.print("[OK]"); else Serial.print("[FAIL]");
    Serial.println("Written DOUBLE value to /test/esp32test/doubletest_6");

	result = Firebase.getString(firebaseData, "/test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/data/email", str);
	if (result) Serial.print("[OK]"); else Serial.print("[FAIL]");
    Serial.println("Read STRING value from /test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/data/email");
	Serial.println(str);

	result = Firebase.getJSON(firebaseData, "/test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/data", &json);
	if (result) Serial.print("[OK]"); else Serial.print("[FAIL]");
    Serial.println("Read JSON value from /test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/data");
	json.toString(str);
	Serial.println(str+"<<<<");
	Serial.print("DataType: ");
	Serial.println(firebaseData.dataType());
	Serial.print("payload: ");
	Serial.println(firebaseData.payload()+"||||");
	json.toString(str);
	Serial.println(str+">>>>");

	result = Firebase.get(firebaseData, "/test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/devices");
	if (result) Serial.print("[OK]"); else Serial.print("[FAIL]");
    Serial.println("Read JSON value from /test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/devices");
	Serial.print("DataType: ");
	Serial.println(firebaseData.dataType());
	Serial.print("payload: ");
	Serial.println(firebaseData.payload());

	result = Firebase.getString(firebaseData, "/test/users/4i3lMHjiLeVHg2jcxhDDJnDxw7j1/data/email", str);
	if (result) Serial.print("[OK]"); else Serial.print("[FAIL]");
    Serial.println("Read STRING value from /test/users/4i3lMHjiLeVHg2jcxhDDJnDxw7j1/data/email");
	Serial.println(str);

	result = Firebase.getJSON(firebaseData, "/test/users/4i3lMHjiLeVHg2jcxhDDJnDxw7j1/data", &json);
	if (result) Serial.print("[OK]"); else Serial.print("[FAIL]");
    Serial.println("Read JSON value from /test/users/4i3lMHjiLeVHg2jcxhDDJnDxw7j1/data");
	json.toString(str, true);
	Serial.println(str);



	Serial.println("FREE HEAP MEMORY [after firebase write] **************************");
	Serial.println(ESP.getFreeHeap());
}