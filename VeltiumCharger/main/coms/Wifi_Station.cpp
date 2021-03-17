#include "Wifi_Station.h"

esp_netif_t *sta_netif;
esp_netif_t *ap_netif;

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;

extern bool eth_connected;
extern bool eth_connecting;
extern bool eth_link_up;
extern bool eth_started;

bool wifi_connected  = false;
bool wifi_connecting = false;

static uint8 Reintentos = 0;
static void smartconfig_example_task(void * parm);

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    

    if (event_base == WIFI_EVENT){
        switch(event_id){
            case WIFI_EVENT_STA_START:{
                esp_wifi_connect();
            break;
            }
            case WIFI_EVENT_STA_DISCONNECTED:
                if(!Coms.StartSmartconfig && ! Coms.StartProvisioning){
                    esp_wifi_connect();
                    Serial.println("Reconectando...");
                }

                if(++Reintentos>=7 ){
                    Serial.println("Intentado demasiadas veces, desconectando");
                    stop_wifi();
                    if(Coms.StartSmartconfig){
                        Coms.StartSmartconfig = 0;
                        esp_smartconfig_stop();
                    }
                    else if(Coms.StartProvisioning){
                        Coms.StartProvisioning = 0;
                        wifi_prov_mgr_deinit();
                    }
                    Coms.Wifi.ON = false;
                    SendToPSOC5(Coms.Wifi.ON,COMS_CONFIGURATION_WIFI_ON);
                }
            break;
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;
        Serial.println( "Wifi got IP Address\n");
        Serial.printf( "Wifi: %d %d %d %d\n",IP2STR(&ip_info->ip));


        Coms.Wifi.IP[0] =ip4_addr1(&ip_info->ip);
        Coms.Wifi.IP[1] =ip4_addr2(&ip_info->ip);
        Coms.Wifi.IP[2] =ip4_addr3(&ip_info->ip);
        Coms.Wifi.IP[3] =ip4_addr4(&ip_info->ip);

        wifi_config_t wifi_actual_config;
        esp_wifi_get_config(WIFI_IF_STA, &wifi_actual_config);

        uint8_t len= sizeof(wifi_actual_config.sta.ssid) <=32 ? sizeof(wifi_actual_config.sta.ssid):32;
        memcpy(Coms.Wifi.AP,wifi_actual_config.sta.ssid,len);

        modifyCharacteristic((uint8_t*)Coms.Wifi.AP, 16, COMS_CONFIGURATION_WIFI_SSID_1);
        if(len>16){
            modifyCharacteristic(&Coms.Wifi.AP[16], 16, COMS_CONFIGURATION_WIFI_SSID_2);
        }
        Reintentos = 0;
        wifi_connected = true;
        wifi_connecting = false;
        delay(1000);
        ConfigFirebase.InternetConection = ComprobarConexion();
    }

    else if(event_base == SC_EVENT){
         switch(event_id){
            case SC_EVENT_SCAN_DONE:
                Reintentos = 0;
                Serial.printf( "Scan done\n");
            break;
            case SC_EVENT_FOUND_CHANNEL:
                 Serial.printf( "Found channel\n");
            break;
            case SC_EVENT_GOT_SSID_PSWD:{
                Serial.printf( "Got SSID and password\n");

                smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
                wifi_config_t wifi_config;
                uint8_t ssid[33] = { 0 };
                uint8_t password[65] = { 0 };
                uint8_t rvd_data[33] = { 0 };

                bzero(&wifi_config, sizeof(wifi_config_t));
                memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
                memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
                wifi_config.sta.bssid_set = evt->bssid_set;
                if (wifi_config.sta.bssid_set == true) {
                    memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
                }

                memcpy(ssid, evt->ssid, sizeof(evt->ssid));
                memcpy(password, evt->password, sizeof(evt->password));
                Serial.printf( "SSID:%s\n", ssid);
                Serial.printf( "PASSWORD:%s\n", password);

                ESP_ERROR_CHECK( esp_wifi_disconnect() );

                ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
                ESP_ERROR_CHECK( esp_wifi_connect() );
                Serial.println("Pass recibido");
            
            break;}

            case SC_EVENT_SEND_ACK_DONE:
                ESP_ERROR_CHECK(esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler));
                Serial.printf( "smartconfig over\n");
                esp_smartconfig_stop();
            break;
         }
    }
    else if(event_base == WIFI_PROV_EVENT){
        switch (event_id) {
            case WIFI_PROV_START:
                Serial.println( "Provisioning started");
                Reintentos = 0;
                Coms.Provisioning = true;
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                Serial.printf( "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s\n",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                Serial.printf( "Provisioning failed!\n\tReason : %s"
                         "\n\tPlease reset to factory and retry provisioning\n",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                Serial.printf("Provisioning successful\n");
                Coms.Wifi.ON = true;
                SendToPSOC5(Coms.Wifi.ON,COMS_CONFIGURATION_WIFI_ON);
                Coms.Provisioning = false;
                break;
            case WIFI_PROV_END:
                Serial.printf("Provisioning service deinit\n");
                ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler));
                wifi_prov_mgr_deinit();
                if(!Coms.Provisioning){
                    MAIN_RESET_Write(0);
                    ESP.restart();
                }   
;
                break;
            default:
                break;
        }
    }
}

void stop_wifi(void){
    Serial.println("Stoping wifi");
    Reintentos = 0;
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_destroy(sta_netif);
    delay(50);
    wifi_connected  = false;
    wifi_connecting = false;
}

void initialise_smartconfig(void)
{
    stop_wifi();

    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );   

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    esp_err_t err = (esp_wifi_start());
    if(err != ESP_OK){
        Serial.printf("Error de wifi start de smartconfig %i \n", err);
    }

    delay(100);
    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
    smartconfig_start_config_t cfg2 = SMARTCONFIG_START_CONFIG_DEFAULT();
    esp_smartconfig_start(&cfg2);
}

void initialise_provisioning(void){
    stop_wifi();
    esp_netif_destroy(ap_netif);
    wifi_prov_mgr_stop_provisioning();
    wifi_prov_mgr_deinit();

    //borrar credenciales almacenadas
    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    if(esp_wifi_set_config(WIFI_IF_STA, &conf)){
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        esp_wifi_set_config(WIFI_IF_STA, &conf);
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    //configuracion del provision
    sta_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    char POP [20] ="WF_";
    strcat(POP,ConfigFirebase.Device_Id);
    wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

    const char *service_key = NULL;

    /* Start provisioning service */
    wifi_prov_mgr_start_provisioning(security,  ConfigFirebase.Device_Ser_num, POP,service_key);
}

void start_wifi(void)
{
    wifi_connected  = false;
    wifi_connecting = true;
    Serial.println("Starting wifi");
    /* Initialize Wi-Fi including netif with default config */
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    /* Let's find out if the device is provisioned */
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    /* If device is not yet provisioned start provisioning service */
    if (!provisioned) {
        Serial.printf( "Sin credenciales almacenadas, pausando wifi\n");    
        wifi_prov_mgr_deinit();   
        esp_wifi_stop();

        Coms.Wifi.ON=false;
        SendToPSOC5(Coms.Wifi.ON,COMS_CONFIGURATION_WIFI_ON);
    } else {
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        wifi_connecting = true;
    }
}

void ComsTask(void *args){
    bool ServidorArrancado = false;

    while(!Coms.StartConnection){
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());  
    ESP_ERROR_CHECK(esp_event_loop_create_default());       

        /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL)); 

    while (1){
        if(Coms.StartConnection){
            //Arranque del provisioning
            if(Coms.StartProvisioning || Coms.StartSmartconfig){
                ConfigFirebase.InternetConection=0;
                Serial.println("Starting provisioning sistem");

                if(ServidorArrancado){
                    StopServer();
                    ServidorArrancado = false;
                }
                if(eth_connected || eth_connecting){
                    kill_ethernet();
                    Coms.ETH.ON = false;
                }
                delay(100);
                wifi_connecting=true;
                if(Coms.StartSmartconfig){
                    initialise_smartconfig();
                }
                else{
                    initialise_provisioning();
                }

                Coms.StartProvisioning = false;
                Coms.StartSmartconfig  = false;
            }

            //Comprobar si hemos perdido todas las conexiones
            if(!eth_connected && !wifi_connected){
                ConfigFirebase.InternetConection=false;
            }

            //Encendido de las interfaces          
            if(Coms.Wifi.ON && !wifi_connected && !wifi_connecting){
                start_wifi();                     
            }
            else if(!Coms.Wifi.ON && (wifi_connected || wifi_connecting)){
                stop_wifi();
            }

            if(Coms.ETH.ON){
                if(!eth_started){
                    initialize_ethernet_1();
                    delay(1000);
                    initialize_ethernet_2();
                }
                else if((!eth_connected && !eth_connecting && eth_link_up)){
                    restart_ethernet();
                }
            }
            else if(eth_connected){
                stop_ethernet();
            }

            if(ConfigFirebase.InternetConection && !ServidorArrancado){
                InitServer();
                ServidorArrancado = true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


