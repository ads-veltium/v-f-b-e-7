
#include <Arduino.h>
// #include "esp_system.h"
// #include "xtensa/core-macros.h"
// #include "soc/timer_group_struct.h"
// #include "driver/periph_ctrl.h"
// #include "driver/timer.h"
// #include <ESP32CAN.h>
// #include <Wire.h>
// #include <EEPROM.h>
// #include "esp32/ulp.h"
// #include "esp_sleep.h"
// #include "driver/gpio.h"
// #include "driver/rtc_io.h"
// // OTA
// #include <ArduinoOTA.h>
// #include <FS.h>
#include <WiFi.h>
// #include <WiFiClient.h>
#include <WebServer.h>
// #include <ESPmDNS.h>
// #include <Update.h>
// #include <HardwareSerial.h>

#include "FirebaseAuth.h"

WebServer server(80);

String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#000000;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #EEE;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

String loginIndex = 
"<form name=loginForm>"
"<h1>VELTIUM Login</h1>"
"<input name=userid placeholder='User ID'> "
"<input name=pwd placeholder=Password type=Password> "
"<input type=submit onclick=check(this.form) class=btn value=Login>"
"</br></br>"
"<p>Powered by Draco Systems S.L.</p></form>"
"<script>"
"function check(form) {"
"if(form.userid.value=='admin' && form.pwd.value=='admin')"
"{window.open('/serverIndex')}"
"else"
"{alert('Error Password or Username')}"
"}"
"</script>" + style;

/* Server Index Page */
String serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='Update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br></form>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>" + style;

String myForm = 
"<h2>HTML Forms</h2>\
<form method='POST' enctype='multipart/form-data' action='/action_page.php'>\
  <label for='fname'>First name:</label><br>\
  <input type='text' id='fname' name='fname' value='John'><br>\
  <label for='lname'>Last name:</label><br>\
  <input type='text' id='lname' name='lname' value='Doe'><br><br>\
  <input type='submit' value='Submit'>\
</form>";

const char* stringForHTTPMethod(HTTPMethod method)
{
	switch(method) {
		case 0b00000001: return "HTTP_GET";
		case 0b00000010: return "HTTP_POST";
		case 0b00000100: return "HTTP_DELETE";
		case 0b00001000: return "HTTP_PUT";
		case 0b00010000: return "HTTP_PATCH";
		case 0b00100000: return "HTTP_HEAD";
		case 0b01000000: return "HTTP_OPTIONS";
		case 0b01111111: return "HTTP_ANY";
	}
	return "HTTP_METHOD_UNKNOWN";
}

class MyRequestHandler : public RequestHandler
{
    virtual bool canHandle(HTTPMethod method, String uri)
	{
		Serial.print("MyRequestHandler::canHandle: ");
		Serial.print(stringForHTTPMethod(method));
		Serial.print(" ");
		Serial.print(uri);
		Serial.println();
		return true;
	}
    
	virtual bool canUpload(String uri)
	{
		(void) uri; return false;
	}
    
	virtual bool handle(WebServer& server, HTTPMethod requestMethod, String requestUri)
	{
		Serial.print("MyRequestHandler::handle: ");
		Serial.print(stringForHTTPMethod(requestMethod));
		Serial.print(" ");
		Serial.print(requestUri);
		Serial.println();

		Serial.println("v2");

		char msg[256];
		unsigned argcount = pathArgs.size();
		sprintf(msg, "num args = %u", argcount);
		Serial.println(msg);

		Serial.println("v3");
		for (unsigned i = 0; i < argcount; i++) {
			char buf[128];
			pathArgs[i].toCharArray(buf, 128);
			sprintf(msg, "arg[%u] = %s", i, buf);
			Serial.println(msg);
		}
		Serial.println("v4");

		server.sendHeader("Connection", "close");
		server.send(200, "text/html", myForm);

		// Serial.println("CRASH!");
		// int* p = 0;
		// *p = 32;

		return true;
	}

    virtual void upload(WebServer& server, String requestUri, HTTPUpload& upload)
	{
		(void) server; (void) requestUri; (void) upload;
	}
};

MyRequestHandler myHandler;

void setupWebServer(void) 
{
	//return index page which is stored in serverIndex 

	// server.on("/", HTTP_GET, []() {
	// 		server.sendHeader("Connection", "close");
	// 		server.send(200, "text/html", loginIndex);
	// 		});
	// server.on("/serverIndex", HTTP_GET, []() {
	// 		server.sendHeader("Connection", "close");
	// 		server.send(200, "text/html", serverIndex);
	// 		});

	server.addHandler(&myHandler);

	server.begin();
}


void initFirebaseClient();
void testFirebaseAuth();

void connectToWiFi()
{
	WiFi.begin(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	Serial.print("Connecting to Wi-Fi ");
	Serial.print(CONFIG_ESP_WIFI_SSID);
	Serial.println();

	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(300);
	}
	Serial.println();
	Serial.print("Connected with IP: ");
	Serial.println(WiFi.localIP());
	Serial.println();

	Serial.println("FREE HEAP MEMORY [after connecting to WiFi] **************************");
	Serial.println(ESP.getFreeHeap());
}


FirebaseAuth auth;

void setup() 
{
	Serial.begin(115200);

	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreeHeap());

	// Serial.println(CONFIG_ESP_WIFI_SSID);
	// Serial.println(CONFIG_ESP_WIFI_PASSWORD);
	// Serial.println(CONFIG_ESP_MAXIMUM_RETRY);

	// FirebaseAuth::testJSON();

	// return;

	connectToWiFi();


	bool authResult = auth.performAuth("grantasca@yahoo.es", "pepepe");
	if (authResult)
	{
		Serial.println("Auth succeeded.");
		Serial.println("LocalId:");
		Serial.println(auth.getLocalId());
		Serial.println("IdToken:");
		Serial.println(auth.getIdToken());
	}
	else
	{
		Serial.println("Auth FAILED.");
	}
	

//	setupWebServer();

	Serial.println("FREE HEAP MEMORY [after setupWebServer] **************************");
	Serial.println(ESP.getFreeHeap());

//	initFirebaseClient();

//	testFirebaseAuth();

	Serial.println("FREE HEAP MEMORY [after initFirebaseClient] **************************");
	Serial.println(ESP.getFreeHeap());
}

void loop() 
{
	server.handleClient();
	// Serial.println("FREE HEAP MEMORY [loop] **************************");
	// Serial.println(ESP.getFreeHeap());
	vTaskDelay(10);
}





#include "FirebaseESP32.h"


const char* veltiumbackend_firebase_project = "veltiumbackend.firebaseio.com";
const char* veltiumbackend_database_secret = "Y8LoNguJJ0NJzCNdqpTIIuK7wcNGBPog8kTv5lQn";

//Define FirebaseESP32 data object
static FirebaseData firebaseData;

static FirebaseJson json;

//nst char* userIDToken = "eyJhbGciOiJSUzI1NiIsImtpZCI6IjgyMmM1NDk4YTcwYjc0MjQ5NzI2ZDhmYjYxODlkZWI3NGMzNWM4MGEiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJodHRwczovL3NlY3VyZXRva2VuLmdvb2dsZS5jb20vdmVsdGl1bWJhY2tlbmQiLCJhdWQiOiJ2ZWx0aXVtYmFja2VuZCIsImF1dGhfdGltZSI6MTU5MDU4MTM2NiwidXNlcl9pZCI6IldXMXpwMnB0Z0lPSzJ6eEZVV1V6amNEVEZGcTEiLCJzdWIiOiJXVzF6cDJwdGdJT0syenhGVVdVempjRFRGRnExIiwiaWF0IjoxNTkwNTgxMzY2LCJleHAiOjE1OTA1ODQ5NjYsImVtYWlsIjoiZ3JhbnRhc2NhQHlhaG9vLmVzIiwiZW1haWxfdmVyaWZpZWQiOnRydWUsImZpcmViYXNlIjp7ImlkZW50aXRpZXMiOnsiZW1haWwiOlsiZ3JhbnRhc2NhQHlhaG9vLmVzIl19LCJzaWduX2luX3Byb3ZpZGVyIjoicGFzc3dvcmQifX0.Uk9LD2tNj79YAkH0azfgYEj8HD_-E5xrQY-VoqECOaZ-zkOMy5kAKWtPArJIWuLB5xHgc2LPS1gPz3NG8i5catCmpsSrFDIdq3UfmwSCMzH5XZwBZ9wTAomqojpyI1oN1sBwE-qlmI_kClb7yj9kDvm0w2d_pMlPqsQX49Y1pkRKQl2tXkM_pJuaOctgm9vcOhPy37GUFIMswMMvkL6Bm-dTcp8uTSQr2azhcBPV9eua9o-Kf2zLjMy-g-8GzVrE3OXA0I-B1WQ5l293iuZWmdewRmCsyxAGQWnFSXUcoR7Mpb0B86QiT6nMzp3kGoj-eZiPZavdAKgQxpBb56Jm-g";
//nst char* userIDToken = "eyJhbGciOiJSUzI1NiIsImtpZCI6IjgyMmM1NDk4YTcwYjc0MjQ5NzI2ZDhmYjYxODlkZWI3NGMzNWM4MGEiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJodHRwczovL3NlY3VyZXRva2VuLmdvb2dsZS5jb20vdmVsdGl1bWJhY2tlbmQiLCJhdWQiOiJ2ZWx0aXVtYmFja2VuZCIsImF1dGhfdGltZSI6MTU5MDU5NDkwNSwidXNlcl9pZCI6IldXMXpwMnB0Z0lPSzJ6eEZVV1V6amNEVEZGcTEiLCJzdWIiOiJXVzF6cDJwdGdJT0syenhGVVdVempjRFRGRnExIiwiaWF0IjoxNTkwNTk0OTA1LCJleHAiOjE1OTA1OTg1MDUsImVtYWlsIjoiZ3JhbnRhc2NhQHlhaG9vLmVzIiwiZW1haWxfdmVyaWZpZWQiOnRydWUsImZpcmViYXNlIjp7ImlkZW50aXRpZXMiOnsiZW1haWwiOlsiZ3JhbnRhc2NhQHlhaG9vLmVzIl19LCJzaWduX2luX3Byb3ZpZGVyIjoicGFzc3dvcmQifX0.gO-bGTXzGKfw3M2BAiOX_Z41q8Z-cRpXxtv_4eNkCR7T9kE8zYKhSIlLWh86jpmeJ7HeW7BqR2rf1mQZcwEwS-6x8fGSOkw1E3DKAswYdLbB6gjZZxwB2II4dohktsDkIeOMzWVFKYm45ATLMYaCBOGoScIQkw2K8-AojRz9w9OEZuGS7g9Z8Phe0iOFrkB1WYOx0Lbpea9r5NNhBfHhgqRoDvn2wzXS08vWMff4zmVADveZjRXKmqlUVTMoMTSOZBGPOpF__tgMrlCAVwuuB4nxt9vKs_vOaHP66UfgNZ70p2iR6kvhhuc5EbB4s89vnrnZ0IPkQlnVtdMBTCUNHg";
const char* userIDToken = "eyJhbGciOiJSUzI1NiIsImtpZCI6IjgyMmM1NDk4YTcwYjc0MjQ5NzI2ZDhmYjYxODlkZWI3NGMzNWM4MGEiLCJ0eXAiOiJKV1QifQ.eyJpc3MiOiJodHRwczovL3NlY3VyZXRva2VuLmdvb2dsZS5jb20vdmVsdGl1bWJhY2tlbmQiLCJhdWQiOiJ2ZWx0aXVtYmFja2VuZCIsImF1dGhfdGltZSI6MTU5MDU5ODk5MSwidXNlcl9pZCI6IldXMXpwMnB0Z0lPSzJ6eEZVV1V6amNEVEZGcTEiLCJzdWIiOiJXVzF6cDJwdGdJT0syenhGVVdVempjRFRGRnExIiwiaWF0IjoxNTkwNTk4OTkxLCJleHAiOjE1OTA2MDI1OTEsImVtYWlsIjoiZ3JhbnRhc2NhQHlhaG9vLmVzIiwiZW1haWxfdmVyaWZpZWQiOnRydWUsImZpcmViYXNlIjp7ImlkZW50aXRpZXMiOnsiZW1haWwiOlsiZ3JhbnRhc2NhQHlhaG9vLmVzIl19LCJzaWduX2luX3Byb3ZpZGVyIjoicGFzc3dvcmQifX0.PLi4VCxE7TAtVQbpSTZprCHaD8FEDMDOlXYrteH8lFpsyTmbybByGemaWV7B0F870EjKScXh4m2CLFWucUvmmy7Yk_aaDhhQmQqqQeAtsEfqk6hWn13FQrMq8EM5bvi8A2or4L2qRsx-CiM9JV3Qgi6Hd05R9NfVZkDDA4iv-uffLQ-TO0VbY0rfgbTFLJxD7SiUxaP3gRiEzwnTi_o0A13xA3Gnb1E0Y0qsta4eaFiHQBkB4Ti1sfaqf66g4DWMgvJMzJurfAzSKUcoNxyCOinwIBLc7c2BJkj6p7oHwJEuIqRSsUKGRIIYHQRrpJBTqDeJjum7m4v9xTbYYD6ZNg";
//.,:;-_{}[]/*+!"#$%&/()=?><
void initFirebaseClient()
{
    Serial.println("INIT Firebase Client");
    ESP_LOGI("FIREBASECLIENT", "initFirebaseClient");
    ESP_LOGI(TAG, "initFirebaseClient");

    Firebase.begin(
        veltiumbackend_firebase_project,
        //veltiumbackend_database_secret
		userIDToken
    );


    //Firebase.reconnectWiFi(true);

    //Set database read timeout to 1 minute (max 15 minutes)
    Firebase.setReadTimeout(firebaseData, 1000 * 60);
    //tiny, small, medium, large and unlimited.
    //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
    Firebase.setwriteSizeLimit(firebaseData, "tiny");

	bool result;
    result = Firebase.setDouble(firebaseData, "/test/esp32test/doubletest_6", 3.141592);
	if (result) Serial.print("[OK]"); else Serial.print("[FAIL]");
    Serial.println("Written DOUBLE value to /test/esp32test/doubletest_6");

	String str;
	FirebaseJson json;

	result = Firebase.getString(firebaseData, "/test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/data/email", str);
	if (result) Serial.print("[OK]"); else Serial.print("[FAIL]");
    Serial.println("Read STRING value from /test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/data/email");
	Serial.println(str);

	result = Firebase.getJSON(firebaseData, "/test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/data", &json);
	if (result) Serial.print("[OK]"); else Serial.print("[FAIL]");
    Serial.println("Read JSON value from /test/users/WW1zp2ptgIOK2zxFUWUzjcDTFFq1/data");
	json.toString(str);
	Serial.println(str);
	Serial.print("DataType: ");
	Serial.println(firebaseData.dataType());
	Serial.print("payload: ");
	Serial.println(firebaseData.payload());

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



const char* root_ca = \
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



#include <HTTPClient.h>

void testFirebaseAuth()
{
	HTTPClient http;

	const char* url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=AIzaSyBaJA88_Y3ViCzNF_J08f4LBMAM771aZLs";
	const char* data = "{\"email\":\"grantasca@yahoo.es\",\"password\":\"pepepe\",\"returnSecureToken\":true}";


	http.begin(url, root_ca);
	http.addHeader("Content-Type", "application/json");

	int httpResponseCode = http.POST(data);

	if(httpResponseCode>0)
	{
		String response = http.getString();                       //Get the response to the request

		Serial.println(httpResponseCode);   //Print return code
		Serial.println(response);           //Print request answer
	}
	else
	{
		Serial.print("Error on sending POST: ");
		Serial.println(httpResponseCode);

	}

	http.end();  //Free resources
}