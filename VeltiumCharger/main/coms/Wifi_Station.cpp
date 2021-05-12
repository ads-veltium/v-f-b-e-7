#include "Wifi_Station.h"

esp_netif_t *sta_netif;
esp_netif_t *ap_netif;

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Contador   ContadorExt;
extern carac_Params Params;
extern carac_group  ChargingGroup;

//Contador trifasico
Contador Counter   EXT_RAM_ATTR;

extern bool eth_connected;
extern bool eth_connecting;
extern bool eth_link_up;
extern bool eth_started;
extern uint8_t ConnectionState;

bool wifi_connected  = false;
bool wifi_connecting = false;

static uint8 Reintentos = 0;

void stop_MQTT();
void start_udp();
void InitServer(void);
void StopServer(void);
void coap_start_server();
void coap_start_client();


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    Serial.printf("Evento de wifi %s %i\n", event_base, event_id );
    if (event_base == WIFI_EVENT){
        switch(event_id){
            case WIFI_EVENT_STA_START:{
                Coms.Wifi.Internet = false;
                esp_wifi_disconnect();
                esp_wifi_connect();
            break;
            }
            case WIFI_EVENT_STA_DISCONNECTED:
                Coms.Wifi.Internet = false;
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
            case WIFI_EVENT_STA_CONNECTED:
               wifi_connected = true;
                wifi_connecting = false;
            break;
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;
        Serial.println( "Wifi got IP Address\n");
        Serial.printf( "Wifi: %d %d %d %d\n",IP2STR(&ip_info->ip));

        Coms.Wifi.IP.addr = ip_info->ip.addr;
        /*Coms.Wifi.IP[0] =ip4_addr1(&ip_info->ip);
        Coms.Wifi.IP[1] =ip4_addr2(&ip_info->ip);
        Coms.Wifi.IP[2] =ip4_addr3(&ip_info->ip);
        Coms.Wifi.IP[3] =ip4_addr4(&ip_info->ip);*/

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
        if(!Coms.Provisioning){
            delay(1000);
            Coms.Wifi.Internet = ComprobarConexion();
        }
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
                esp_wifi_connect();
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
    esp_err_t err = esp_wifi_stop();
    if(err==ESP_ERR_WIFI_NOT_INIT){
        return;
    }
    esp_wifi_deinit();

    if(sta_netif != NULL){
        esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif);
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }
    esp_netif_deinit();
    ESP_ERROR_CHECK(esp_netif_init());

    Coms.Wifi.Internet = false;
    wifi_connected  = false;
    wifi_connecting = false;
}

void initialise_smartconfig(void){
    stop_wifi();
    wifi_connecting = true;
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

    if(ap_netif != NULL){
        esp_wifi_clear_default_wifi_driver_and_handlers(ap_netif);
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }
    wifi_prov_mgr_stop_provisioning();
    wifi_prov_mgr_deinit();
    wifi_connecting = true;

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
    Serial.println("Starting wifi");
    //Station_Begin();
    //Wifi de esp

    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;

    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        Serial.printf( "Sin credenciales almacenadas, pausando wifi\n");    
        stop_wifi();
        wifi_prov_mgr_stop_provisioning();
        wifi_prov_mgr_deinit();
        Coms.Wifi.ON=false;
        SendToPSOC5(Coms.Wifi.ON,COMS_CONFIGURATION_WIFI_ON);
    } else {
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        wifi_connecting = true;
        esp_wifi_connect();
        Serial.println("Hay credenciales y nos conectamos");
    }
}

void Eth_Loop(){
    static TickType_t xStart = 0;
    static bool finding = false;
    static uint8_t LastStatus = APAGADO;
    switch (Coms.ETH.State){
        case APAGADO:
            if(Coms.ETH.ON){
                initialize_ethernet();
                Coms.ETH.State = CONNECTING;
                xStart = xTaskGetTickCount();
            }
        break;
        case CONNECTING:
            if(eth_connected){
                Coms.ETH.State = CONECTADO;
                SendToPSOC5(0,SEND_GROUP_DATA);
                delay(100);
                start_udp();
                xStart = xTaskGetTickCount();
                //Solo comprobamos la conexion a internet si no hemos activado el servidro dhcp
                if(!Coms.ETH.DHCP){
                    if(ComprobarConexion()){
                        Coms.ETH.Internet = true;
                        Coms.Wifi.ON = false;
                    }
                }
            }

            else if(!Coms.ETH.ON){
                stop_ethernet();
                Coms.ETH.State = DISCONNECTING;                
            }
            
        break;

        case CONECTADO:
            //Buscar el contador
            if(Params.Tipo_Sensor && !finding){
                if(GetStateTime(xStart) > 60000){
                    Serial.println("Iniciando fase busqueda ");
                    xTaskCreate( BuscarContador_Task, "BuscarContador", 4096*4, NULL, 5, NULL); 
                    finding = true;
                }
            }

            //Arrancar los grupos
            else if(!ChargingGroup.Conected && Coms.ETH.conectado){
                if(ChargingGroup.Params.GroupActive){
                    if(GetStateTime(xStart) > 30000){
                        if(ChargingGroup.StartClient){
                            coap_start_client();
                        }
                        else{
                            coap_start_server();
                        }
                        
                    }
                }
            }

            //Apagar el eth
            if(!Coms.ETH.ON && (ConnectionState ==IDLE || ConnectionState==DISCONNECTED)){  
                //Si lo queremos reinicializar para ponerle una ip estatica o quitarsela
                if(Coms.ETH.restart){
                    if(ChargingGroup.Conected){
                        ChargingGroup.Params.GroupActive = false;
                        //stop_MQTT();
                        
                    }
                    kill_ethernet();
                    Coms.ETH.State = KILLING;
                    Coms.ETH.restart = false;
                }
                else{
                    if(ChargingGroup.Conected){
                        ChargingGroup.Params.GroupActive = false;
                        stop_MQTT();
                    }
                    stop_ethernet();
                    Coms.ETH.State = DISCONNECTING;
                }                
            }

            //Desconexion del cable
            if(!eth_connected && !eth_link_up && (ConnectionState ==IDLE || ConnectionState==DISCONNECTED)){
                Coms.ETH.State = DISCONNECTING;
                if(ChargingGroup.Conected){
                    ChargingGroup.Params.GroupActive = false;
                    stop_MQTT();
                }
            }
            //Lectura del contador
			if(ContadorExt.ContadorConectado && Params.Ubicacion_Sensor){
				if(!Counter.Inicializado){
                    printf("Arrancando lectura del contador\n");
					Counter.begin(ContadorExt.ContadorIp);
				}

				Counter.read();
				Counter.parse();
				uint8 buffer_contador[7] = {0}; 

				buffer_contador[0] = ContadorExt.ContadorConectado;
				buffer_contador[1] = (uint8)(ContadorExt.DomesticCurrentA& 0x00FF);
				buffer_contador[2] = (uint8)((ContadorExt.DomesticCurrentA >> 8) & 0x00FF);

				if(Status.Trifasico){
					buffer_contador[3] = (uint8)(ContadorExt.DomesticCurrentB & 0x00FF);
					buffer_contador[4] = (uint8)((ContadorExt.DomesticCurrentB >> 8) & 0x00FF);

					buffer_contador[5] = (uint8)(ContadorExt.DomesticCurrentC & 0x00FF);
					buffer_contador[6] = (uint8)((ContadorExt.DomesticCurrentC >> 8) & 0x00FF);
				}

				SendToPSOC5((char*)buffer_contador,7,MEASURES_EXTERNAL_COUNTER);
			}
            else if(ContadorExt.ContadorConectado && !Params.Ubicacion_Sensor){
                ContadorExt.ContadorConectado = false;
                Counter.end();
            }

        break;

        case DISCONNECTING:
            if(!eth_connected && !eth_connecting){        
                finding = false;
                if(ContadorExt.ContadorConectado){
                    ContadorExt.ContadorConectado = false;
                    Counter.end();
                }
                
                Coms.ETH.State = DISCONNECTED;
                Coms.ETH.Internet = false;
            }
        break;

        case DISCONNECTED:
            if(Coms.ETH.ON){
                restart_ethernet();
                Coms.ETH.State = CONNECTING;
                xStart = xTaskGetTickCount();
            }
        break;
        case KILLING:        
            if(!eth_connected && !eth_connecting){
                Coms.ETH.Internet = false;
                if(!Coms.ETH.ON){
                    Coms.ETH.State = APAGADO;
                    break;
                }

                if(ContadorExt.ContadorConectado){
                    ContadorExt.ContadorConectado = false;
                    Counter.end();
                }

                Coms.ETH.Wifi_Perm = false;
                stop_wifi();
                Coms.ETH.State = CONNECTING;
                initialize_ethernet();         
                xStart = xTaskGetTickCount();
            }
        break;

        default:
            Serial.println("Estado no implementado en maquina de ETH!!!");
        break;
    }
    if(LastStatus!= Coms.ETH.State){
      Serial.printf("Maquina de estados de ETH de % i a %i \n", LastStatus, Coms.ETH.State);
      LastStatus= Coms.ETH.State;
    }
}

void ComsTask(void *args){
    bool ServidorArrancado = false;
    Coms.ETH.State=APAGADO;
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

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL)); 
    while (1){
        if(Coms.StartConnection){
            Eth_Loop();
            //Arranque del provisioning
            if(Coms.StartProvisioning || Coms.StartSmartconfig){
                ConfigFirebase.InternetConection=0;
                Serial.println("Starting provisioning sistem");
                Coms.Wifi.ON = true;
                if(ServidorArrancado){
                    StopServer();
                    ServidorArrancado = false;
                }
                if(eth_connected || eth_connecting){
                    Coms.ETH.State = KILLING;
                    Coms.ETH.ON = false;
                    kill_ethernet();
                }
                delay(100);
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
            if(!Coms.ETH.Internet && !Coms.Wifi.Internet){
                ConfigFirebase.InternetConection=false;
            }
            else{
                ConfigFirebase.InternetConection=true;
            }

            //Encendido de las interfaces          
            if(Coms.Wifi.ON && !wifi_connected && !wifi_connecting){
                if(Coms.ETH.ON){
                    if(!Coms.ETH.Internet && Coms.ETH.Wifi_Perm){
                        start_wifi();                     
                    }
                }
                else{
                    start_wifi();
                }                
            }
            else if(!Coms.Wifi.ON && (wifi_connected || wifi_connecting)){
                if(ConnectionState ==IDLE ||ConnectionState==DISCONNECTED){
                    stop_wifi();
                }

            }
            if((eth_connected || wifi_connected ) && !ServidorArrancado){

                InitServer();
                ServidorArrancado = true;
            }
        }
        delay(500);
    }
}