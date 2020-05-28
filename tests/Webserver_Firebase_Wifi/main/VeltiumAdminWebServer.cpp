////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  VeltiumAdminWebServer.cpp
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 28/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "VeltiumAdminWebServer.h"
#include "FirebaseClient.h"
#include "FirebaseJson.h"
#include <WebServer.h>






























static const char html_root[] = \
"<!DOCTYPE html>\n"\
"<html>\n"\
"  <head>\n"\
"  </head>\n"\
"  <body>\n"\
"      <script>window.location.href=\"./login.html\"</script>\n"\
"  </body>\n"\
"</html>\n"\
"\n"\
;
static const char html_login[] = \
"<!DOCTYPE html>\n"\
"<html lang=\"en\">\n"\
"<head>\n"\
"    <meta charset=\"UTF-8\">\n"\
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"\
"    <title>Document</title>\n"\
"    <style>\n"\
"#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}\n"\
"input{box-sizing: border-box;background:#f1f1f1;border:0;padding:0 15px}\n"\
"body{background:#000000;font-family:sans-serif;font-size:14px;color:#777}\n"\
"#file-input{padding:0;border:1px solid #EEE;line-height:44px;text-align:left;display:block;cursor:pointer}\n"\
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}\n"\
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}\n"\
".btn{background:#3498db;color:#fff;cursor:pointer}\n"\
"    </style>\n"\
"</head>\n"\
"<body>\n"\
"    <form name=loginForm>\n"\
"        <h1>Log into Veltium</h1>\n"\
"        <input name=email placeholder='User email'>\n"\
"        <input name=pass placeholder=Password type=Password> \n"\
"        <input type=button onclick=check(this.form) class=btn value=Login>\n"\
"        </br></br>\n"\
"        <p>VeltiumBackend / Firebase</p>\n"\
"    </form>\n"\
"    <script>\n"\
"        function check(form) {\n"\
"            console.log(\"CHECK\");\n"\
"            if (form.email.value.length == 0) {\n"\
"                alert(\"email must not be empty\");\n"\
"                return;\n"\
"            }\n"\
"            if (form.pass.value.length == 0) {\n"\
"                alert(\"password must not be empty\");\n"\
"                return;\n"\
"            }\n"\
"            var paramObject = {\n"\
"                e:form.email.value,\n"\
"                p:form.pass.value,\n"\
"            };\n"\
"            var paramString = JSON.stringify(paramObject);\n"\
"            console.log(paramString);\n"\
"\n"\
"            var b64string = btoa(paramString)\n"\
"\n"\
"            var href = './auth.html:' + b64string;\n"\
"            console.log(href);\n"\
"            window.location.href = href;\n"\
"        }\n"\
"    </script>\n"\
"</body>\n"\
"</html>\n"\
;
static const char html_auth_fail[] = \
"<!DOCTYPE html>\n"\
"<html>\n"\
"  <head>\n"\
"  </head>\n"\
"  <body>\n"\
"    <p>Authentication FAILED.</p>\n"\
"    <a href=\"./login.html\">Back to Login</a>\n"\
"  </body>\n"\
"</html>\n"\
"\n"\
;
static const char html_auth_ok[] = \
"<!DOCTYPE html>\n"\
"<html>\n"\
"  <head>\n"\
"  </head>\n"\
"  <body>\n"\
"      <script>window.location.href=\"./user_root.html\"</script>\n"\
"  </body>\n"\
"</html>\n"\
"\n"\
;
static const char html_user_root[] = \
"<!DOCTYPE html>\n"\
"<html lang=\"en\">\n"\
"<head>\n"\
"    <meta charset=\"UTF-8\">\n"\
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"\
"    <title>Document</title>\n"\
"</head>\n"\
"<body>\n"\
"    <p>user email: PLACEHOLDER_EMAIL</p>\n"\
"    <p>user id: PLACEHOLDER_ID</p>\n"\
"\n"\
"    <p>Charger Device List for current user:</p>\n"\
"    <div id=\"deviceList\"></div>\n"\
"    <script>\n"\
"var req = new XMLHttpRequest();\n"\
"var devlist = document.getElementById(\"deviceList\");\n"\
"\n"\
"function deviceListFromJSON(txt)\n"\
"{\n"\
"    var obj = JSON.parse(txt);\n"\
"    var res = '';\n"\
"    for (key in obj) {\n"\
"        res += '<p><a href=\"./device:' + key + '\">' + key + '</a></p>'\n"\
"    }\n"\
"    console.log(obj);\n"\
"    return res;\n"\
"}\n"\
"\n"\
"//var testtext = '{\"000000000BCD17010003CDCDE074\":{\"enabled\":true,\"name\":\"mi placa\",\"timestamp\":1519662804132},\"000000000BCD1701000453A975D7\":{\"enabled\":true,\"name\":\"vitoria\",\"timestamp\":1521460195442},\"000000000BCD1701000524AE4541\":{\"enabled\":true,\"name\":\"veltium real\",\"timestamp\":1519656237801},\"000000000BCD170100085A1F39FC\":{\"enabled\":true,\"name\":\"mipr8\",\"timestamp\":1521113539288},\"000000000BCD170100113E74913C\":{\"enabled\":true,\"name\":\"lek t1 sn 11\",\"timestamp\":1523262593928},\"000000000BCD1701001539195525\":{\"enabled\":true,\"name\":\"placa 15\",\"timestamp\":1523263982848},\"000000000BCD17040003CB06229F\":{\"enabled\":false,\"name\":\"Veltium\",\"timestamp\":1545063658707},\"000000000BCD180500017CA939D2\":{\"enabled\":true,\"name\":\"Veltium 18050001\",\"timestamp\":1545064288723},\"123451234512345123453B48CE16\":{\"enabled\":true,\"name\":\"chaleterequerele\",\"timestamp\":1428135244596},\"24680246802468024680BB055263\":{\"enabled\":true,\"name\":\"history test\",\"timestamp\":1539864544817}}';\n"\
"//devlist.innerHTML = deviceListFromJSON(testtext);\n"\
"\n"\
"req.addEventListener('load', function(ev){\n"\
"    // console.log(ev);\n"\
"    // console.log(this);\n"\
"    if (200 == this.status)\n"\
"    {\n"\
"        //let parseOk = parseData(this.responseText);\n"\
"        devlist.innerHTML = deviceListFromJSON(this.responseText);\n"\
"    }\n"\
"    else\n"\
"    {\n"\
"        devlist.innerHTML = \"<p>No devices for this user</p>\"\n"\
"    }\n"\
"});\n"\
"req.open('GET', './devices.json');\n"\
"req.send();\n"\
"    </script>\n"\
"</body>\n"\
"</html>\n"\
;

























static WebServer server(80);



static const char* stringForHTTPMethod(HTTPMethod method)
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

static FirebaseClient fireclient;

#include <libb64/cdecode.h>

static String decodeBase64String(String b64)
{
	char* plaintext_out = new char[b64.length()];
	memset(plaintext_out, 0, b64.length());
	base64_decode_chars(b64.c_str(), b64.length(), plaintext_out);
	String result = plaintext_out;
	delete[] plaintext_out;
	return result;
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

		String uri;
		String arg = "";
		String dec_arg = "";
		FirebaseJson json;
		int index = requestUri.indexOf(":");
		if (-1 == index) {
			uri = requestUri;
		}
		else {
			uri = requestUri.substring(0, index);
			arg = requestUri.substring(index+1);
			if (arg.length() > 0) {
				dec_arg = decodeBase64String(arg);
				json.setJsonData(dec_arg);
			}
		}

		Serial.print("URI: ");
		Serial.println(uri);
		Serial.print("ARG: ");
		Serial.println(arg);
		Serial.print("DEC_ARG: ");
		Serial.println(dec_arg + "||||");
		Serial.print("JSON: ");
		String jsonstr;
		json.toString(jsonstr);
		Serial.println(jsonstr);

		if (uri == "/")
		{
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", html_root);
		}
		else if (uri == "/login.html")
		{
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", html_login);
		}
		else if (uri == "/auth.html")
		{
			bool initialized = false;
			Serial.println("serving /auth request");
			FirebaseJsonData jsondata;
			if (json.get(jsondata, "e")) {
				String user_email = jsondata.stringValue;
				Serial.print("user_email: ");
				Serial.println(user_email);
				if (json.get(jsondata, "p")) {
					String user_pass = jsondata.stringValue;
					Serial.print("user_pass: ");
					Serial.println(user_pass);
					initialized = fireclient.initialize(user_email.c_str(), user_pass.c_str());
				}
			}

			if (initialized)
			{
				server.sendHeader("Connection", "close");
				server.send(200, "text/html", html_auth_ok);
			}
			else
			{
				server.sendHeader("Connection", "close");
				server.send(200, "text/html", html_auth_fail);
			}
		}
		else if (uri == "/user_root.html")
		{
			String doctored_html = html_user_root;
			doctored_html.replace("PLACEHOLDER_EMAIL", fireclient.getEmail());
			doctored_html.replace("PLACEHOLDER_ID", fireclient.getLocalId());
			
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", doctored_html);
		}
		else if (uri == "/devices.json")
		{
			String jsonString = fireclient.getUserDeviceListJSONString();
			server.sendHeader("Connection", "close");
			server.send(200, "text/json", jsonString);
		}
		else
		{
			server.sendHeader("Connection", "close");
			server.send(404, "text/html", "NOT FOUND");
		}
		
		Serial.println("FREE HEAP MEMORY [after serving request] **************************");
		Serial.println(ESP.getFreeHeap());

		return true;
	}

    virtual void upload(WebServer& server, String requestUri, HTTPUpload& upload)
	{
		(void) server; (void) requestUri; (void) upload;
	}
};

MyRequestHandler myHandler;

void VeltiumAdminWebServer::initialize() 
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

void VeltiumAdminWebServer::handleClient()
{
    server.handleClient();
}
