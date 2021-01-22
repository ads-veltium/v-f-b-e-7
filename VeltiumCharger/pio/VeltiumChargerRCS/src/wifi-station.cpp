////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  wifi.cpp
//  Veltium Smart Charger on ESP32
//
//  Created by David Crespo on 26/05/2020.
//  Copyright © 2020 Virtual Code SL. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "wifi-station.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include <esp_wifi.h>
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <Arduino.h>


#include "lwip/err.h"
#include "lwip/sys.h"

#include "FirebaseClient.h"


#define EXAMPLE_ESP_MAXIMUM_RETRY 5

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "wifi-station";

static int s_retry_num = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("ggggg");
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
                 Serial.printf("got ip:%s",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "******** CONNECTED TO WIFI!!!! ********");
        ESP_LOGI(TAG, "before calling initFirebaseClient");
        initFirebaseClient();
        ESP_LOGI(TAG, "after calling initFirebaseClient");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP");
            }
            ESP_LOGI(TAG,"connect to the AP fail\n");
            break;
        }
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_sta(const char* wifi_ssid, const char* wifi_password)
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config;
    strcpy((char*)wifi_config.sta.ssid, wifi_ssid);
    strcpy((char*)wifi_config.sta.password, wifi_password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    Serial.println("wifi_init_sta finished");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             wifi_ssid, wifi_password);
    Serial.printf("connect to ap SSID:%s password:%s" ,wifi_ssid, wifi_password);
}

void initWifi(const char* wifi_ssid, const char* wifi_password)
{
    ESP_LOGI(TAG, "Initializing WIFI...");
    Serial.println("Initializing WIFI...");
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    Serial.println("ESP_WIFI_MODE_STA");
    ESP_LOGI(TAG, "example modified by David Crespo for Veltium Smart Chargers on May 2020");
    Serial.println("example modified by David Crespo for Veltium Smart Chargers on May 2020");
    wifi_init_sta(wifi_ssid, wifi_password);
}