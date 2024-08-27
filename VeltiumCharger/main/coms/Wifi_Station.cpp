#include "Wifi_Station.h"
#include "../control.h"
#include "lwip/dns.h"

#ifdef CONNECTED
esp_netif_t *sta_netif;
esp_netif_t *ap_netif;

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Contador ContadorExt;
extern carac_Params Params;
extern carac_charger charger_table[MAX_GROUP_SIZE];

extern uint8 Bloqueo_de_carga;

static const char *TAG = "Wifi_Station";

//Contador trifasico 
Contador Meter EXT_RAM_ATTR;

bool ServidorArrancado = false;
static uint8 Reintentos = 0;

static uint8_t ComsEHT_OldState = APAGADO;

void InitServer(void);
void StopServer(void);

extern carac_group  ChargingGroup;
void coap_start_server();
void coap_start_client();
void start_udp();
void close_udp();

/***********************HELPERS***********************************************/
void wait_for_firebase_stop(uint8_t time){
    ConfigFirebase.InternetConection = false;
    while(ConfigFirebase.FirebaseConnState != DISCONNECTED && --time > 0){
        delay(100);
    }
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
#ifdef DEBUG_WIFI
    Serial.printf("Evento de wifi %s %i \n", event_base, event_id);
#endif
    if (event_base == WIFI_EVENT){
        switch (event_id){
        case WIFI_EVENT_STA_START:{
            Coms.Wifi.Internet = false;
            // esp_wifi_disconnect();
            esp_wifi_connect();
            Update_Status_Coms(0, WIFI_BLOCK);
            break;
        }
        case WIFI_EVENT_STA_CONNECTED:
            Coms.Wifi.connected = true;
            Coms.Wifi.connecting = false;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
            ESP_LOGI(TAG, "Station disconnect. Reason code: %d", event->reason);
            Update_Status_Coms(0, WIFI_BLOCK);
            Coms.Wifi.Internet = false;
            if (!Coms.StartProvisioning){
                esp_wifi_connect();
#ifdef DEBUG_WIFI
                ESP_LOGI(TAG, "Reconectado WiFi");
#endif
            }
            break;
            }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;

#ifdef DEBUG_WIFI
        Serial.printf("Wifi got IP Address: %d %d %d %d\n", IP2STR(&ip_info->ip));
#endif

        Coms.Wifi.IP.addr = ip_info->ip.addr;
        Coms.Wifi.Mask.addr = ip_info->netmask.addr;
        Coms.Wifi.Gateway.addr = ip_info->gw.addr;

        wifi_config_t wifi_actual_config;
        esp_wifi_get_config(WIFI_IF_STA, &wifi_actual_config);

        uint8_t len= sizeof(wifi_actual_config.sta.ssid) <=32 ? sizeof(wifi_actual_config.sta.ssid):32;
        memcpy(Coms.Wifi.AP,wifi_actual_config.sta.ssid,len);

        modifyCharacteristic((uint8_t*)Coms.Wifi.AP, 16, COMS_CONFIGURATION_WIFI_SSID_1);
        if(len>16){
            modifyCharacteristic(&Coms.Wifi.AP[16], 16, COMS_CONFIGURATION_WIFI_SSID_2);
        }
        Reintentos = 0;
        Coms.Wifi.connected = true;
        Coms.Wifi.connecting = false;

        // ADS Fijamos las direcciones de los servidores DNS

        if(!Coms.Provisioning){
            delay(5000); // Aumentado de 1000 a 5000 antes de comprobar la conexión
            SetDNS();
            Coms.Wifi.Internet = ComprobarConexion();
        }
        Update_Status_Coms(WIFI_CONNECTED);
    }

    else if(event_base == WIFI_PROV_EVENT){
        switch (event_id) {
            case WIFI_PROV_START:
                #ifdef DEBUG_WIFI
                Serial.println( "Provisioning started");
                #endif
                Reintentos = 0;
                Coms.Provisioning = true;
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                #ifdef DEBUG_WIFI
                Serial.printf( "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s\n",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                #endif
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                #ifdef DEBUG_WIFI
                Serial.printf( "Provisioning failed!\n\tReason : %s\n",(*reason == WIFI_PROV_STA_AUTH_ERROR) ?"Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
                #endif
                Update_Status_Coms(WIFI_BAD_CREDENTIALS);
                esp_wifi_connect();

                if(*reason == WIFI_PROV_STA_AUTH_ERROR){
                    wifi_config_t conf;
                    memset(&conf, 0, sizeof(wifi_config_t));
                    if(esp_wifi_set_config(WIFI_IF_STA, &conf)){
                        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
                        esp_wifi_set_config(WIFI_IF_STA, &conf);
                    }
                    Coms.Provisioning = false;
                    delay(1000);
                    stop_wifi();
                }
                
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning finalizado");
                Update_Status_Coms(WIFI_CONNECTED);
                Coms.Wifi.ON = true;
                SendToPSOC5(Coms.Wifi.ON,COMS_CONFIGURATION_WIFI_ON);
                Coms.Provisioning = false;
                wifi_prov_mgr_stop_provisioning();
                delay(8000);
                ESP_LOGW(TAG, "Provisioning finalizado - Reboot");
                MAIN_RESET_Write(0);
                ESP.restart();
                
                break;
            case WIFI_PROV_END:
                ESP_LOGI(TAG, "Provisioning service deinit");
                ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler));
                wifi_prov_mgr_deinit();
                if(!Coms.Provisioning){
                    ESP_LOGW(TAG, "Reboot");
                    MAIN_RESET_Write(0);
                    ESP.restart();
                }   
                break;
            default:
                break;
        }
    }
}

void stop_wifi(void){
    uint8_t AP[34]={'\0'};
    modifyCharacteristic((uint8_t*)AP, 16, COMS_CONFIGURATION_WIFI_SSID_1);
    modifyCharacteristic(&AP[16], 16, COMS_CONFIGURATION_WIFI_SSID_2);
    

    ConfigFirebase.InternetConection = false;
    #ifdef DEBUG_WIFI
    Serial.println("Esperando a que firebase se desconecte!");
    #endif

     wait_for_firebase_stop(100);

    if(Coms.Provisioning){
        wifi_prov_mgr_stop_provisioning();
        wifi_prov_mgr_deinit();
    }

    #ifdef DEBUG_WIFI
    Serial.println("Stoping wifi");
    #endif
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
    Coms.Wifi.connected  = false;
    Coms.Wifi.connecting = false;
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
    Coms.Wifi.connecting = true;

    //borrar credenciales almacenadas
    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    if(esp_wifi_set_config(WIFI_IF_STA, &conf)){
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        esp_wifi_set_config(WIFI_IF_STA, &conf);
    }
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    
    //configuracion del provisioning
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
    Coms.Wifi.connected  = false;

#ifdef DEBUG_WIFI
    Serial.println("Wifi_Station - start_wifi: Starting wifi");
#endif

    //Si nos conectamos con credenciales recibidas desde el navegador interno
    if(Coms.Wifi.SetFromInternal){
        #ifdef DEBUG_WIFI
        printf("Empezando wifi con credenciales internas\n");
        #endif
        sta_netif = esp_netif_create_default_wifi_sta();

        Coms.Wifi.SetFromInternal = false;
        wifi_config_t config;

        memcpy(config.sta.ssid, Coms.Wifi.AP,strlen((char*)Coms.Wifi.AP));
        memcpy(config.sta.password, Coms.Wifi.Pass,strlen((char*)Coms.Wifi.Pass));

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &config);
        ESP_ERROR_CHECK(esp_wifi_start() );

        Coms.Wifi.connecting = true;
        //esp_wifi_connect();
    }

    else{
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
            #ifdef DEBUG_WIFI
            ESP_LOGI(TAG, "Sin credenciales. Pausando WiFi");
            #endif
            Update_Status_Coms(WIFI_NO_CREDENTIALS);
            stop_wifi();
            wifi_prov_mgr_stop_provisioning();
            wifi_prov_mgr_deinit();
            Coms.Wifi.ON=false;
            SendToPSOC5(Coms.Wifi.ON,COMS_CONFIGURATION_WIFI_ON);
        } else {
            wifi_prov_mgr_deinit();
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_start());
            Coms.Wifi.connecting = true;
            ESP_ERROR_CHECK(esp_wifi_start() );
        }
    }
}

void Eth_Loop(){
    static TickType_t xStart = 0;
    static TickType_t xFirstStart = 0;
    static TickType_t xCheckConn = 0;
    static TickType_t xConnect = 0;
    #ifdef DEBUG_ETH
    static uint8_t LastStatus = APAGADO;
    #endif
    static uint8_t escape =0;
    static  bool wifi_last =false;
    static uint8 reintentos = 0;

    switch (Coms.ETH.State){
#ifdef DEBUG_ETH
        Serial.printf("Wifi_Station - Eth_Loop: Coms.ETH.State = %i\n", Coms.ETH.State);
#endif
    case APAGADO:
        initialize_ethernet();
        ComsEHT_OldState = Coms.ETH.State;
        Coms.ETH.State = CONNECTING;
#ifdef DEBUG_ETH
        if (ComsEHT_OldState != Coms.ETH.State) {
            Serial.printf("Wifi_Station - Eth_Loop: APAGADO. Coms.ETH.State pasa de %i a %i\n", ComsEHT_OldState, Coms.ETH.State);
        }
#endif
        xStart = xTaskGetTickCount();
        xFirstStart = xTaskGetTickCount();
        Get_Stored_Group_Params();
        Coms.ETH.finding = false;
        Coms.ETH.conectado = false;
        Coms.ETH.Wifi_Perm = false;

        break;

    case CONNECTING:
        if (Coms.ETH.connected){
            // Solo comprobamos la conexion a internet si no hemos activado el servidor dhcp y si el usuario a activado el ETH
            if (!Coms.ETH.DHCP && Coms.ETH.ON){
                if (Coms.Wifi.ON){
                    wifi_last = true;
                }
                Coms.Wifi.ON = false;

                if ((Coms.Wifi.connected || Coms.Wifi.connecting) && ++escape < 20){
                    break;
                }
                if (ComprobarConexion()){
                    // ConfigFirebase.FirebaseConnState = DISCONNECTED;
                    Coms.ETH.Internet = true;
                }
                else{
                    xCheckConn = xTaskGetTickCount();
                    Coms.Wifi.ON = wifi_last;
                    Coms.ETH.Internet = false;
                    Coms.ETH.Wifi_Perm = true;
                }
            }

            Get_Stored_Group_Data();
            Get_Stored_Group_Circuits();

            delay(1000);
            start_udp();

            escape = 0;
            wifi_last = false;
            ComsEHT_OldState = Coms.ETH.State;
            Coms.ETH.State = CONECTADO;
#ifdef DEBUG_ETH
            if (ComsEHT_OldState != Coms.ETH.State){
                printf("Wifi_Station - Eth_Loop: CONNECTING_1 Coms.ETH.State pasa de %i a %i\n", ComsEHT_OldState, Coms.ETH.State);
            }
#endif
            xConnect = xTaskGetTickCount();
        }
        // Si tenemos un grupo activo y al minuto no tenemos conexion a internet, activamos el DHCP
        // Si no soy el maestro, espero 2 minutos
        else if (!Coms.ETH.ON){
            // if (!Coms.ETH.medidor && !ChargingGroup.Params.GroupActive){
            if (!ChargingGroup.Params.GroupActive){
                if (GetStateTime(xStart) > 1000){
                    Coms.ETH.Wifi_Perm = true;
                }
            }
            if (!Coms.Provisioning){
                //if ((ChargingGroup.Params.GroupMaster && (ChargingGroup.Params.GroupActive || ChargingGroup.Creando)) || Coms.ETH.medidor){ // ADS REVISAR Cuándo pasa por aquí
                if (ChargingGroup.Params.GroupMaster && (ChargingGroup.Params.GroupActive || ChargingGroup.Creando)){ 
                    if (GetStateTime(xFirstStart) > 5000){
#ifdef DEBUG_ETH
                        Serial.println("Wifi_Station - Eth_Loop: Master : Activo DHCP");
#endif
                        Coms.ETH.DHCP = true;
                        ComsEHT_OldState = Coms.ETH.State;
                        kill_ethernet();
                        Coms.ETH.State = KILLING;
#ifdef DEBUG_ETH
                        if (ComsEHT_OldState != Coms.ETH.State){
                            printf("Wifi_Station - Eth_Loop: CONNECTING_2. Coms.ETH.State pasa de %i a %i\n", ComsEHT_OldState, Coms.ETH.State);
                        }
#endif
                        ChargingGroup.Creando = false;
                    }
                }
/*                  // ADS  Eliminado el inicio del servidor DHCP si el equipo no es MASTER para evitar

                else if ((ChargingGroup.Params.GroupActive && !ChargingGroup.Conected)){ // ADS REVISAR Cuándo pasa por aquí
                    uint8 wait_time = reintentos == 0 ? 5000 : 3000;
                    if (GetStateTime(xStart) > wait_time){
                        if (!memcmp(charger_table[reintentos].name, ConfigFirebase.Device_Id, 8)){
#ifdef DEBUG_ETH
                            Serial.println("Wifi_Statio - Eth_Loop: Not Master: Activo DHCP");
#endif
                            Coms.ETH.DHCP = true;
                            ComsEHT_OldState = Coms.ETH.State;
                            kill_ethernet(); // ADS ¿¿¿QUITAR ESTO???
                            Coms.ETH.State = KILLING;
#ifdef DEBUG_ETH
                            if (ComsEHT_OldState != Coms.ETH.State){
                                printf("Wifi_Station - Eth_Loop: CONNECTING_3. Coms.ETH.State pasa de %i a %i\n", ComsEHT_OldState, Coms.ETH.State);
                            }
#endif
                            ChargingGroup.Creando = false;
                        }
                        else{
                            xStart = xTaskGetTickCount();
                            reintentos++;
                        }
                    }
                }
*/          
            }
        }

        break;

    case CONECTADO:{
        reintentos = 0;
        // Recomprobar si tenemos conexion a firebase
        if (Coms.ETH.ON){
            if (!Coms.ETH.DHCP && !Coms.ETH.Internet && !ConfigFirebase.InternetConection && GetStateTime(xCheckConn) > 5000){
                xCheckConn = xTaskGetTickCount();
                if (ComprobarConexion()){
                    // ConfigFirebase.FirebaseConnState = DISCONNECTED;
                    Coms.ETH.Internet = true;
                    Coms.ETH.Wifi_Perm = false;
                }
            }
        }
        else{
            if (!Coms.ETH.medidor || (Coms.ETH.medidor && ContadorExt.GatewayConectado)){
                Coms.ETH.Wifi_Perm = true;
            }
        }

        // Buscar el contador
        /*if ((Coms.ETH.medidor || (ContadorConfigurado() && ChargingGroup.Params.GroupMaster && ChargingGroup.Conected)) && !Coms.ETH.finding){
            if (GetStateTime(xConnect) > 6000){
                xTaskCreate(BuscarContador_Task, "BuscarContador", 4096 * 4, &Coms.ETH.finding, 5, NULL);
                Coms.ETH.finding = true;
            }
        }*/
        // Arrancar los grupos
        if (!ChargingGroup.Conected && (Coms.ETH.conectado || Coms.ETH.DHCP)){
            if (ChargingGroup.Params.GroupActive && GetStateTime(xConnect) > 1000){
                if (ConfigFirebase.FirebaseConnState == IDLE || ConfigFirebase.FirebaseConnState == DISCONNECTED){ // Esperar a que arranque firebase
                    if (ChargingGroup.StartClient){
                        coap_start_client();
                    }
                    else{
                        if (ChargingGroup.Params.GroupMaster)
                        {
                            coap_start_server();
                        }
                        /*      // ADS - eliminado el arranque del server y el else - ¿TIENE SENTIDO ARRANCAR AQUÍ EL SERVER ?
                        else if(GetStateTime(xConnect) > 5000){
                            coap_start_server();
                        }  */
                    }
                }
            }
        }

        // Apagar el eth
        if (Coms.ETH.restart){
            if (Coms.ETH.ON){
                wait_for_firebase_stop(100);
            }
            /*        // ADS ¿¿PARA QUÉ ESTÁ ESTO???
            if(ChargingGroup.Conected){
                ChargingGroup.Params.GroupActive = false;
                ChargingGroup.StopOrder = true;
            }
            */
            close_udp();
            kill_ethernet();
            ComsEHT_OldState = Coms.ETH.State;
            Coms.ETH.State = KILLING;
#ifdef DEBUG_ETH
            if (ComsEHT_OldState != Coms.ETH.State){
                printf("Wifi_Station - Eth_Loop: CONECTADO. Coms.ETH.State pasa de %i a %i\n", ComsEHT_OldState, Coms.ETH.State);
            }
#endif
            Coms.ETH.DHCP = false;
            Coms.ETH.restart = false;
            break;
        }

        // Lectura del contador
        /*if (ContadorExt.GatewayConectado && (Params.Tipo_Sensor || (ContadorConfigurado() && ChargingGroup.Params.GroupMaster && ChargingGroup.Conected))){
            if (!Meter.Inicializado){
                Meter.begin(ContadorExt.IPadd);
#ifdef DEBUG_ETH
                ESP_LOGI(TAG,"Eth_Loop: Envío BLOQUEO_CARGA=0 al PSoC");
#endif
                SendToPSOC5(0, BLOQUEO_CARGA);
            }

            Meter.read();
            Meter.parse();

            if (Params.Tipo_Sensor){
                uint8 buffer_contador[7] = {0};
                buffer_contador[0] = ContadorExt.GatewayConectado;
                buffer_contador[1] = (uint8)(ContadorExt.Power & 0x00FF);
                buffer_contador[2] = (uint8)((ContadorExt.Power >> 8) & 0x00FF);
                buffer_contador[3] = (uint8)((ContadorExt.Power >> 16) & 0x00FF);
                buffer_contador[4] = (uint8)((ContadorExt.Power >> 24) & 0x00FF);
                SendToPSOC5((char *)buffer_contador, 5, SEARCH_EXTERNAL_METER);
            }
        }

        // Pausar lectura del contador
        else if (ContadorExt.GatewayConectado && !Params.Tipo_Sensor && !ContadorConfigurado()){
            ContadorExt.GatewayConectado = false;
            ContadorExt.MedidorConectado = false;
            Meter.Inicializado = false;
            Meter.end();
            Coms.ETH.finding = false;
            Update_Status_Coms(0, MED_BLOCK);
        }*/
    }
    break;

    case DISCONNECTING:{
        uint8_t ip_Array[4] = {0, 0, 0, 0};
        modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);
        if (!Coms.ETH.connected && !Coms.ETH.connecting){
            Coms.ETH.finding = false;
            if (ContadorExt.GatewayConectado){
                ContadorExt.GatewayConectado = false;
                ContadorExt.MedidorConectado = false;
                Meter.end();
                Update_Status_Coms(0, MED_BLOCK);
            }
            ComsEHT_OldState = Coms.ETH.State;
            Coms.ETH.State = DISCONNECTED;
#ifdef DEBUG_ETH
            if (ComsEHT_OldState != Coms.ETH.State){
                Serial.printf("Wifi_Station - Eth_Loop: DISCONNECTING. Coms.ETH.State pasa de %i a %i\n", ComsEHT_OldState, Coms.ETH.State);
            }
#endif
            Coms.ETH.Internet = false;
            Coms.ETH.conectado = false;
        }
    }
    break;

    case DISCONNECTED:
        ComsEHT_OldState = Coms.ETH.State;
        restart_ethernet();
        Coms.ETH.State = CONNECTING;
#ifdef DEBUG_ETH
        if (ComsEHT_OldState != Coms.ETH.State)
        {
            Serial.printf("Wifi_Station - Eth_Loop: DISCONNECTED. Coms.ETH.State pasa de %i a %i\n", ComsEHT_OldState, Coms.ETH.State);
        }
#endif
        xStart = xTaskGetTickCount();

        break;
    case KILLING:{
        uint8_t ip_Array[4] = {0, 0, 0, 0};
        modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);
        if (!Coms.ETH.connected && !Coms.ETH.connecting){
            Coms.ETH.finding = false;
            Coms.ETH.Internet = false;
            Coms.ETH.conectado = false;

            if (ContadorExt.GatewayConectado){
                ContadorExt.GatewayConectado = false;
                ContadorExt.MedidorConectado = false;
                Meter.end();
                Update_Status_Coms(0, MED_BLOCK);
            }
            if (Coms.ETH.ON){
                wait_for_firebase_stop(100);
            }
            // Eliminada la parada del WiFi en este caso.  Se pierden las credenciales y no reinicia.
            // stop_wifi();
            // Coms.ETH.Wifi_Perm = false;

            ComsEHT_OldState = Coms.ETH.State;
            Coms.ETH.State = CONNECTING;
#ifdef DEBUG_ETH
            if (ComsEHT_OldState != Coms.ETH.State){
                printf("Wifi_Station - Eth_Loop: KILLING. Coms.ETH.State pasa de %i a %i\n", ComsEHT_OldState, Coms.ETH.State);
            }
#endif
            initialize_ethernet();
            xStart = xTaskGetTickCount();
        }
    }
    break;

    default:
#ifdef DEBUG_ETH
        Serial.println("Estado no implementado en maquina de ETH!!!");
#endif
        break;
    }

#ifdef DEBUG_ETH
    if (LastStatus != Coms.ETH.State){
        Serial.printf("Maquina de estados de ETH de % i a %i \n", LastStatus, Coms.ETH.State);
        LastStatus = Coms.ETH.State;
    }
#endif
}

void ComsTask(void *args){
    ComsEHT_OldState = Coms.ETH.State;
    Coms.ETH.State = APAGADO;
#ifdef DEBUG_ETH
    if (ComsEHT_OldState != Coms.ETH.State){
        printf("Wifi_Station - ComsTask: Coms.ETH.State pasa de %i a %i\n", ComsEHT_OldState, Coms.ETH.State);
    }
#endif
    while (!Coms.StartConnection){
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    while (1){
        if (Coms.StartConnection){
            Eth_Loop();
            // Arranque del provisioning
            if (Coms.StartProvisioning && !Coms.Provisioning){
                ConfigFirebase.InternetConection = false;
#ifdef DEBUG_WIFI
                Serial.println("Starting provisioning sistem");
#endif
                Coms.Wifi.ON = true;

                /* // Eliminado arranque y parada del WebServer
                if(ServidorArrancado){
                    StopServer();
                    ServidorArrancado = false;
                }
                */
                Coms.ETH.Wifi_Perm = true;
                delay(100);
                initialise_provisioning();
                Coms.Wifi.restart = false;
                Coms.StartProvisioning = false;
            }

            // Comprobar si hemos perdido todas las conexiones
            if (!Coms.ETH.Internet && !Coms.Wifi.Internet){
                ConfigFirebase.InternetConection = false;
            }
            else{
                ConfigFirebase.InternetConection=true;
            }

            //Encendido de las interfaces     
            if(Coms.Wifi.ON && !Coms.Wifi.connected && !Coms.Wifi.connecting){
                if(Coms.ETH.ON){
                    if(!Coms.ETH.Internet && Coms.ETH.Wifi_Perm ){
                        start_wifi();                     
                    }
                }
                else if(Coms.ETH.Wifi_Perm){
                    start_wifi();
                }                
            }

            else if((!Coms.Wifi.ON || Coms.Wifi.restart || !Coms.ETH.Wifi_Perm) && (Coms.Wifi.connected || Coms.Wifi.connecting)){
                stop_wifi();
                Coms.Wifi.restart = false;
            }
            /* // Eliminado arranque y parada del WebServer
            if((Coms.ETH.connected || Coms.Wifi.connected) && !ServidorArrancado){
                if(!Coms.Wifi.connecting ){
                    InitServer(); 
                    ServidorArrancado = true;
                }
            }
            */
        }
        delay(250);
    }
}
#endif