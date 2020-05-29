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

    testListenAtBranch();

    testSetJSONRecord();

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

static FirebaseData fbDataForStream;

void printResult(StreamData &data);

static void streamCallback(StreamData data)
{
    Serial.println("Stream Data1 available...");
    Serial.println("STREAM PATH: " + data.streamPath());
    Serial.println("EVENT PATH: " + data.dataPath());
    Serial.println("DATA TYPE: " + data.dataType());
    Serial.println("EVENT TYPE: " + data.eventType());
    Serial.print("VALUE: ");
    printResult(data);
    Serial.println();
}

static void streamTimeoutCallback(bool timeout)
{
    if (timeout)
    {
        Serial.println();
        Serial.println("Stream timeout, resume streaming...");
        Serial.println();
    }
}

void FirebaseClient::testListenAtBranch()
{
//    String path = "/test/users/4i3lMHjiLeVHg2jcxhDDJnDxw7j1/devices/123451234512345123453B48CE16";
    String path = "/test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/devices/123451234512345123453B48CE16";

    Serial.print("testListenAtBranch: path = ");
    Serial.println(path);

    if (!Firebase.beginStream(fbDataForStream, path))
    {
        Serial.println("------------------------------------");
        Serial.println("Can't begin stream connection...");
        Serial.println("REASON: " + fbDataForStream.errorReason());
        Serial.println("------------------------------------");
        Serial.println();
    }

    Firebase.setStreamCallback(fbDataForStream, streamCallback, streamTimeoutCallback);

}

void FirebaseClient::testSetJSONRecord()
{
    String jsonRecString = "{"\
        "\"act\" : \"AY0DcARaBL4E/AUjBS0FNAVBBUIFNQU4BUgE0gNXAkIBiwEQALsAgABYADsAKAAbABIADAAIAAUAAwAB\","\
        "\"con\" : 1451856279,"\
        "\"dis\" : 1451902265,"\
        "\"rea\" : \"ACcAWABvAHkAfwCDAIQAhQCGAIYAhQCFAIcAewBVADkAJwAbABIADAAIAAUABAACAAEAAQ==\","\
        "\"sta\" : 1451857925,"\
        "\"usr\" : 0"\
        "}";

    String path = "/test/devices/123451234512345123453B48CE16/records/1450000000";

    FirebaseJson jsonRec;
    jsonRec.setJsonData(jsonRecString);

    Serial.print("Trying to write JSON data at branch path ");
    Serial.println(path);

    bool ok = Firebase.setJSON(firebaseData, path, jsonRec);
    if (ok) {
        Serial.println("...OK");
        Serial.println(firebaseData.dataType());
        Serial.println(firebaseData.payload());
    }
    else {
        Serial.println("...FAILED");
    }
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
















/////////////////////////////////////////////
/////////////////////////////////////////////
/////////////////////////////////////////////
/////////////////////////////////////////////
/////////////////////////////////////////////
/////////////////////////////////////////////
/////////////////////////////////////////////
/////////////////////////////////////////////


void printResult(StreamData &data)
{

  if (data.dataType() == "int")
    Serial.println(data.intData());
  else if (data.dataType() == "float")
    Serial.println(data.floatData(), 5);
  else if (data.dataType() == "double")
    printf("%.9lf\n", data.doubleData());
  else if (data.dataType() == "boolean")
    Serial.println(data.boolData() == 1 ? "true" : "false");
  else if (data.dataType() == "string")
    Serial.println(data.stringData());
  else if (data.dataType() == "json")
  {
    Serial.println();
    FirebaseJson *json = data.jsonObjectPtr();
    //Print all object data
    Serial.println("Pretty printed JSON data:");
    String jsonStr;
    json->toString(jsonStr, true);
    Serial.println(jsonStr);
    Serial.println();
    Serial.println("Iterate JSON data:");
    Serial.println();
    size_t len = json->iteratorBegin();
    String key, value = "";
    int type = 0;
    for (size_t i = 0; i < len; i++)
    {
      json->iteratorGet(i, type, key, value);
      Serial.print(i);
      Serial.print(", ");
      Serial.print("Type: ");
      Serial.print(type == FirebaseJson::JSON_OBJECT ? "object" : "array");
      if (type == FirebaseJson::JSON_OBJECT)
      {
        Serial.print(", Key: ");
        Serial.print(key);
      }
      Serial.print(", Value: ");
      Serial.println(value);
    }
    json->iteratorEnd();
  }
  else if (data.dataType() == "array")
  {
    Serial.println();
    //get array data from FirebaseData using FirebaseJsonArray object
    FirebaseJsonArray *arr = data.jsonArrayPtr();
    //Print all array values
    Serial.println("Pretty printed Array:");
    String arrStr;
    arr->toString(arrStr, true);
    Serial.println(arrStr);
    Serial.println();
    Serial.println("Iterate array values:");
    Serial.println();

    for (size_t i = 0; i < arr->size(); i++)
    {
      Serial.print(i);
      Serial.print(", Value: ");

      FirebaseJsonData *jsonData = data.jsonDataPtr();
      //Get the result data from FirebaseJsonArray object
      arr->get(*jsonData, i);
      if (jsonData->typeNum == FirebaseJson::JSON_BOOL)
        Serial.println(jsonData->boolValue ? "true" : "false");
      else if (jsonData->typeNum == FirebaseJson::JSON_INT)
        Serial.println(jsonData->intValue);
      else if (jsonData->typeNum == FirebaseJson::JSON_DOUBLE)
        printf("%.9lf\n", jsonData->doubleValue);
      else if (jsonData->typeNum == FirebaseJson::JSON_STRING ||
               jsonData->typeNum == FirebaseJson::JSON_NULL ||
               jsonData->typeNum == FirebaseJson::JSON_OBJECT ||
               jsonData->typeNum == FirebaseJson::JSON_ARRAY)
        Serial.println(jsonData->stringValue);
    }
  }
}

