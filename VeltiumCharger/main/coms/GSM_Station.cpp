#include "../control.h"
#include "GSM_Station.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/semphr.h"

#include "driver/uart.h"

#include "netif/ppp/pppos.h"
#include "netif/ppp/ppp.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "netif/ppp/pppapi.h"
#include "lwip/apps/sntp.h"

#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include <esp_event.h>
#include <esp_wifi.h>

#include "cJSON.h"

#include "libGSM.h"

#define SerialAT Serial1

static const char *TIME_TAG = "[SNTP]";

extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Coms  Coms;
extern uint8_t ConnectionState;

bool gsm_connected   = false;

QueueHandle_t http_mutex;

String SendAt(String command){
	String inchar;

	Serial.printf("Enviando commando: ");
	
	uint8 count = 0;
	while(++count < 100){
		SerialAT.println(command);
		delay(500);
		if (SerialAT.available()){
			inchar = SerialAT.readString();
			if(inchar.indexOf("OK")>0){
				Serial.println(inchar);
				break;
			}
			else if(inchar.indexOf("ERROR")>0){
				Serial.println(inchar);
				break;
			}
			else if(inchar.indexOf(">")>0){
				Serial.println(inchar);
				break;
			}
		}
	}


	return inchar;
}

void StartGSM(){
	Coms.GSM.Internet = false;
	gsm_connected   = false;
	if (ppposInit() == 0) {
		ESP_LOGE("PPPoS EXAMPLE", "ERROR: GSM not initialized, HALTED");
		//ppposDisconnect(0, 1);
	}
	Coms.GSM.Internet = true;
	gsm_connected   = true;
	//probarConexionGSM();

}


void FinishGSM(){
	ppposDisconnect(0, 1);
	apagarModem();

	gsm_connected = false;
	Coms.GSM.Internet = false;
	ConnectionState = DISCONNECTED;
	ConfigFirebase.InternetConection=false;
}

void shutdownGSM(){
	apagarModem();
}

void probarConexionGSM(){
		// ==== Get time from NTP server =====
	time_t now = 0;
	struct tm timeinfo = { 0 };
	int retry = 0;
	const int retry_count = 10;

	time(&now);
	localtime_r(&now, &timeinfo);

	while (1) {
		printf("\r\n");
		
		ESP_LOGE(TIME_TAG,"OBTAINING TIME");
	    ESP_LOGE(TIME_TAG, "Initializing SNTP");
	    sntp_setoperatingmode(SNTP_OPMODE_POLL);
	    sntp_setservername(0, "pool.ntp.org");
	    sntp_setservername(1, "europe.pool.ntp.org");
	    sntp_setservername(2, "uk.pool.ntp.org ");
	    sntp_setservername(3, "us.pool.ntp.org");
	    sntp_init();
		ESP_LOGE(TIME_TAG,"SNTP INITIALIZED");

		// wait for time to be set
		now = 0;
		while ((timeinfo.tm_year < (2016 - 1900)) && (++retry < retry_count)) {
			//ESP_LOGI(TIME_TAG, "Esperando a obtener hora (%d/%d)", retry, retry_count);
			vTaskDelay(2000 / portTICK_PERIOD_MS);
			time(&now);
			localtime_r(&now, &timeinfo);
			if (ppposStatus() != GSM_STATE_CONNECTED) break;
		}
		if (ppposStatus() != GSM_STATE_CONNECTED) {
			sntp_stop();
			ESP_LOGE(TIME_TAG, "Desconectado, esperando a reconexion");
			retry = 0;
			while (ppposStatus() != GSM_STATE_CONNECTED) {
				vTaskDelay(100 / portTICK_RATE_MS);
			}
			continue;
		}

		if (retry < retry_count) {
			ESP_LOGE(TIME_TAG, "La hora es: %s", asctime(&timeinfo));
			Coms.GSM.Internet = true;
			gsm_connected   = true;
			break;
		}
		else {
			ESP_LOGE(TIME_TAG, "ERROR AL OBTENER HORA\n");
		}
		sntp_stop();
		break;
	}

    while(1)
	{
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}