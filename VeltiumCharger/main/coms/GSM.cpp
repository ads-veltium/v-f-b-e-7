#include "../control.h"


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
   
	ppposInit();

}