////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  FirebaseClient.h
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 26/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "esp_log.h"


#include "FirebaseESP32.h"
#include <Arduino.h>


static const char *TAG = "FirebaseClient";

const char* veltiumbackend_firebase_project = "veltiumbackend.firebaseio.com";
const char* veltiumbackend_database_secret = "Y8LoNguJJ0NJzCNdqpTIIuK7wcNGBPog8kTv5lQn";

//Define FirebaseESP32 data object
static FirebaseData firebaseData;

static FirebaseJson json;


void initFirebaseClient()
{
    Serial.println("INIT Firebase Client");
    ESP_LOGI("FIREBASECLIENT", "initFirebaseClient");
    ESP_LOGI(TAG, "initFirebaseClient");

    Firebase.begin(
        veltiumbackend_firebase_project,
        veltiumbackend_database_secret
    );


    //Firebase.reconnectWiFi(true);

    //Set database read timeout to 1 minute (max 15 minutes)
    Firebase.setReadTimeout(firebaseData, 1000 * 60);
    //tiny, small, medium, large and unlimited.
    //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
    Firebase.setwriteSizeLimit(firebaseData, "tiny");

    Firebase.setDouble(firebaseData, "/test/esp32test/doubletest_5", 3.141592);

    Firebase.setTimestamp(firebaseData, "/test/esp32test/ts");

    Serial.println("Written DOUBLE value to /test/esp32test/doubletest_5");

	Serial.println("FREE HEAP MEMORY [after firebase write] **************************");
	Serial.println(ESP.getFreeHeap());


}

/*
<!-- The core Firebase JS SDK is always required and must be listed first -->
<script src="https://www.gstatic.com/firebasejs/7.14.5/firebase-app.js"></script>

<!-- TODO: Add SDKs for Firebase products that you want to use
     https://firebase.google.com/docs/web/setup#available-libraries -->

<script>
  // Your web app's Firebase configuration
  var firebaseConfig = {
    apiKey: "AIzaSyBaJA88_Y3ViCzNF_J08f4LBMAM771aZLs",
    authDomain: "veltiumbackend.firebaseapp.com",
    databaseURL: "https://veltiumbackend.firebaseio.com",
    projectId: "veltiumbackend",
    storageBucket: "veltiumbackend.appspot.com",
    messagingSenderId: "8626093121",
    appId: "1:8626093121:web:87bf7c640ccdad9dfe9ac0"
  };
  // Initialize Firebase
  firebase.initializeApp(firebaseConfig);
</script>*/

