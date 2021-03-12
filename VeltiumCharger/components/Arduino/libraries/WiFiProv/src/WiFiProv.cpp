/*
    WiFiProv.cpp - WiFiProv class for provisioning
    All rights reserved.
 
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
  
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
  
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
    
*/

#include "WiFiProv.h"

bool wifiLowLevelInit(bool persistent);
bool wifiLowLevelDeinit();

#if CONFIG_IDF_TARGET_ESP32
static const uint8_t custom_service_uuid[16] = {  0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
                                                  0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02, };
#endif

#define SERV_NAME_PREFIX_PROV "PROV_"

static void get_device_service_name(prov_scheme_t prov_scheme, char *service_name, size_t max)
{
    uint8_t eth_mac[6] = {0,0,0,0,0,0};
    if(esp_wifi_get_mac((wifi_interface_t)WIFI_IF_STA, eth_mac) != ESP_OK){
    	log_e("esp_wifi_get_mac failed!");
    	return;
    }
    snprintf(service_name, max, "%s%02X%02X%02X",SERV_NAME_PREFIX_PROV, eth_mac[3], eth_mac[4], eth_mac[5]);

}

static esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf, ssize_t *outlen, void *priv_data){
    if (inbuf) {
    	log_d("Received data: %.*s", inlen, (char *)inbuf);
    }
    *outbuf = NULL;
    *outlen = 0;
    return ESP_OK;
}

uint8_t WiFiProvClass :: beginProvision(prov_scheme_t prov_scheme, scheme_handler_t scheme_handler, wifi_prov_security_t security, const char * pop, const char *service_name, bool provisioning, const char *service_key, uint8_t * uuid)
{
    bool provisioned = false;
    static char service_name_temp[32];

    wifi_prov_mgr_config_t config;
    config.scheme = wifi_prov_scheme_softap;

    wifi_prov_event_handler_t scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE;
    memcpy(&config.scheme_event_handler, &scheme_event_handler, sizeof(wifi_prov_event_handler_t));

    config.app_event_handler.event_cb = NULL;
    config.app_event_handler.user_data = NULL;

    wifiLowLevelInit(true);
    //esp_netif_create_default_wifi_sta();

    if(wifi_prov_mgr_init(config) != ESP_OK){
    	log_e("wifi_prov_mgr_init failed!");
    	return 0;
    }
    
    if(wifi_prov_mgr_is_provisioned(&provisioned) != ESP_OK){
        Serial.printf("wifi_prov_mgr_is_provisioned failed! : %i",wifi_prov_mgr_is_provisioned(&provisioned));
    	wifi_prov_mgr_deinit();
    	return 0;
    }
    if(provisioned == false) {
        if(!provisioning){
            StopProvision();
            wifi_prov_mgr_deinit();
            return 6;
        }

        if(service_name == NULL) {
            get_device_service_name(prov_scheme, service_name_temp, 32);
            service_name = (const char *)service_name_temp;
        }

            if(service_key == NULL) {
               Serial.printf("Starting provisioning AP using SOFTAP. service_name : %s, pop : %s \n",service_name,pop);
            } else {
               Serial.printf("Starting provisioning AP using SOFTAP. service_name : %s, password : %s, pop : %s\n  ",service_name,service_key,pop);
            }
        if(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key) != ESP_OK){
        	log_e("wifi_prov_mgr_start_provisioning failed!");
        	return 0;
        }
    } else {
        log_i("Already Provisioned");
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        static wifi_config_t conf;
        esp_wifi_get_config((wifi_interface_t)WIFI_IF_STA,&conf);
        log_i("Attempting connect to AP: %s\n",conf.sta.ssid);
#endif

        esp_wifi_start();        
        wifi_prov_mgr_deinit();
        WiFi.begin();
        return 0;
    }
    return 0;
}

bool WiFiProvClass::StopProvision(){
    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    if(esp_wifi_set_config(WIFI_IF_STA, &conf)){
        Serial.println(esp_wifi_set_config(WIFI_IF_STA, &conf));
        log_e("clear config failed!");
    }
    wifi_prov_mgr_stop_provisioning();
    wifi_prov_mgr_deinit(); 
    wifiLowLevelDeinit();
    delay(150);
    return false;
}
WiFiProvClass WiFiProv;
