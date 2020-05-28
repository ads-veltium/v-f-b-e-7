
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

void setupWebServer(void) 
{
	//return index page which is stored in serverIndex 

	server.on("/", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", loginIndex);
			Serial.println("FREE HEAP MEMORY [after serving /] **************************");
			Serial.println(ESP.getFreeHeap());

			});
	server.on("/serverIndex", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", serverIndex);
			Serial.println("FREE HEAP MEMORY [after serving /serverIndex] **************************");
			Serial.println(ESP.getFreeHeap());
			});

	server.begin();
}


void setup() 
{
	Serial.begin(115200);

	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreeHeap());

	// Serial.println(CONFIG_ESP_WIFI_SSID);
	// Serial.println(CONFIG_ESP_WIFI_PASSWORD);
	// Serial.println(CONFIG_ESP_MAXIMUM_RETRY);

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

	setupWebServer();

	Serial.println("FREE HEAP MEMORY [after setupWebServer] **************************");
	Serial.println(ESP.getFreeHeap());

}

void loop() 
{
	server.handleClient();
	// Serial.println("FREE HEAP MEMORY [loop] **************************");
	// Serial.println(ESP.getFreeHeap());
	vTaskDelay(10);
}

