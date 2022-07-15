#include "Wifi_Station.h"
#include "../control.h"

#ifdef CONNECTED
esp_netif_t *sta_netif;
esp_netif_t *ap_netif;

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Contador   ContadorExt;
extern carac_Params Params;
extern carac_charger charger_table[MAX_GROUP_SIZE];

//Contador trifasico
Contador Counter   EXT_RAM_ATTR;

extern bool gsm_connected;
extern bool eth_connected;
extern bool eth_connecting;
extern bool eth_link_up;
extern bool eth_started;
extern uint8_t ConnectionState;

bool wifi_connected  = false;
bool wifi_connecting = false;
bool ServidorArrancado = false;
static uint8 Reintentos = 0;



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
    while(ConnectionState != DISCONNECTED && --time > 0){
        delay(100);
    }
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    #ifdef DEBUG_WIFI
    printf("Evento de wifi %i %i \n", (int)event_base, event_id);
    #endif
    if (event_base == WIFI_EVENT){
        switch(event_id){
            case WIFI_EVENT_STA_START:{
                Coms.GSM.ON=false;
                Coms.Wifi.Internet = false;
                esp_wifi_disconnect();
                esp_wifi_connect();
                Update_Status_Coms(0,WIFI_BLOCK);

            break;
            }
            case WIFI_EVENT_STA_DISCONNECTED:
                Update_Status_Coms(0,WIFI_BLOCK);
                Coms.Wifi.Internet = false;
                if(! Coms.StartProvisioning){
                    esp_wifi_connect();
                    #ifdef DEBUG_WIFI
                    Serial.println("Reconectando...");
                    #endif
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

        #ifdef DEBUG_WIFI
            Serial.println( "Wifi got IP Address\n");
            Serial.printf( "Wifi: %d %d %d %d\n",IP2STR(&ip_info->ip));
        #endif

        Coms.Wifi.IP.addr = ip_info->ip.addr;

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
                #ifdef DEBUG_WIFI
                Serial.printf("Provisioning successful\n");
                #endif
                Update_Status_Coms(WIFI_CONNECTED);
                Coms.Wifi.ON = true;
                SendToPSOC5(Coms.Wifi.ON,COMS_CONFIGURATION_WIFI_ON);
                Coms.Provisioning = false;
                wifi_prov_mgr_stop_provisioning();
                delay(8000);
                
                MAIN_RESET_Write(0);
                ESP.restart();
                
                break;
            case WIFI_PROV_END:
                #ifdef DEBUG_WIFI
                Serial.printf("Provisioning service deinit\n");
                #endif
                ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler));
                wifi_prov_mgr_deinit();
                if(!Coms.Provisioning){
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
    wifi_connected  = false;
    wifi_connecting = false;
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
    wifi_connected  = false;

    #ifdef DEBUG_WIFI
    Serial.println("Starting wifi");
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

        wifi_connecting = true;
        esp_wifi_connect();
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
            Serial.printf( "Sin credenciales almacenadas, pausando wifi\n");    
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
            wifi_connecting = true;
            esp_wifi_connect();
            #ifdef DEBUG_WIFI
            Serial.println("Hay credenciales y nos conectamos");
            #endif
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
        case APAGADO:
            initialize_ethernet();
            Coms.ETH.State = CONNECTING;
            xStart = xTaskGetTickCount();
            xFirstStart = xTaskGetTickCount();
            SendToPSOC5(1,SEND_GROUP_DATA);
            Coms.ETH.finding = false;
            Coms.ETH.conectado = false;
            Coms.ETH.Wifi_Perm = false;
            
            
        break;
        case CONNECTING:
            if(eth_connected){
                //Solo comprobamos la conexion a internet si no hemos activado el servidor dhcp y si el usuario a activado el ETH
                if(!Coms.ETH.DHCP && Coms.ETH.ON){
                    if(Coms.Wifi.ON){
                        wifi_last = true;
                    }
                    Coms.Wifi.ON = false;
                    
                    if ((wifi_connected || wifi_connecting) && ++escape < 20){
                        break;
                    }
                    if(ComprobarConexion()){
                        ConnectionState = DISCONNECTED;
                        Coms.ETH.Internet = true;
                    }
                    else{
                        xCheckConn = xTaskGetTickCount();
                        Coms.Wifi.ON = wifi_last;
                        Coms.ETH.Internet = false;
                        Coms.ETH.Wifi_Perm = true;
                    }
                }

                SendToPSOC5(2,SEND_GROUP_DATA);
                SendToPSOC5(3,SEND_GROUP_DATA);          
                delay(5000);
                start_udp();

                escape =0;
                wifi_last = false;
                Coms.ETH.State = CONECTADO;
                xConnect = xTaskGetTickCount();
            }
            //Si tenemos un grupo activo y al minuto no tenemos conexion a internet, activamos el DHCP
            //Si no soy el maestro, espero 2 minutos
            else if(!Coms.ETH.ON){
                if(!Coms.ETH.medidor && !ChargingGroup.Params.GroupActive){
                    if(GetStateTime(xStart) > 5000){
                        Coms.ETH.Wifi_Perm = true;
                    }
                }
                if(!Coms.Provisioning){
                    if((ChargingGroup.Params.GroupMaster && (ChargingGroup.Params.GroupActive || ChargingGroup.Creando) )|| Coms.ETH.medidor){
                        if(GetStateTime(xFirstStart) > 90000){
                            #ifdef DEBUG_ETH
                                Serial.println("Activo DHCP");
                            #endif
                            Coms.ETH.DHCP = true;
                            kill_ethernet();
                            Coms.ETH.State = KILLING;
                            ChargingGroup.Creando = false;
                        }
                    }
                    else if((ChargingGroup.Params.GroupActive && !ChargingGroup.Conected)){
                        uint8 wait_time = reintentos == 0 ? 110000:30000;
                        if(GetStateTime(xStart) > wait_time){
                            if(!memcmp(charger_table[reintentos].name,ConfigFirebase.Device_Id,8)){
                                #ifdef DEBUG_ETH
                                Serial.println("Activo DHCP");
                                #endif
                                Coms.ETH.DHCP = true;
                                kill_ethernet();
                                Coms.ETH.State = KILLING;
                                ChargingGroup.Creando = false;
                            }
                            else{
                                xStart = xTaskGetTickCount();
                                reintentos++;
                            }
                            
                        }
                    }
                }
            }
            
        break;
        case CONECTADO:{
            reintentos=0;
            // Recomprobar si tenemos conexion a firebase
            if(Coms.ETH.ON){
                if(!Coms.ETH.DHCP && !Coms.ETH.Internet && !ConfigFirebase.InternetConection && GetStateTime(xCheckConn) > 60000){
                    xCheckConn = xTaskGetTickCount();
                    if(ComprobarConexion()){
                        ConnectionState = DISCONNECTED;
                        Coms.ETH.Internet = true;
                        Coms.ETH.Wifi_Perm = false;
                    }
                }
            }
            else{
                if(!Coms.ETH.medidor || (Coms.ETH.medidor && ContadorExt.GatewayConectado)){
                    Coms.ETH.Wifi_Perm = true;
                }
            }

            //Buscar el contador
            if((Coms.ETH.medidor || (ChargingGroup.Params.CDP >> 4 && ChargingGroup.Params.GroupMaster && ChargingGroup.Conected)) && !Coms.ETH.finding){
                if(GetStateTime(xConnect) > 20000){
                    xTaskCreate( BuscarContador_Task, "BuscarContador", 4096*4, &Coms.ETH.finding, 5, NULL); 
                    Coms.ETH.finding = true;
                }
            }
            
            //Arrancar los grupos
            else if(!ChargingGroup.Conected && (Coms.ETH.conectado || Coms.ETH.DHCP)){
                if(ChargingGroup.Params.GroupActive && GetStateTime(xConnect) > 5000 ){
                    if(ConnectionState == IDLE || ConnectionState == DISCONNECTED){       //Esperar a que arranque firebase
                        if(ChargingGroup.StartClient){
                            coap_start_client();
                        }
                        else{
                            if(ChargingGroup.Params.GroupMaster){
                                coap_start_server();
                            }
                            else if(GetStateTime(xConnect) > 60000){
                                coap_start_server();
                            }
                        }
                        
                    }
                }
            }

            //Apagar el eth
            if(Coms.ETH.restart){  
                if(Coms.ETH.ON){
                    wait_for_firebase_stop(100);
                }
                if(ChargingGroup.Conected){
                    ChargingGroup.Params.GroupActive = false;
                    ChargingGroup.StopOrder = true;   
                }
                close_udp();   
                kill_ethernet();
                Coms.ETH.State = KILLING;
                Coms.ETH.DHCP = false;
                Coms.ETH.restart = false;
                break;               
            }

            //Lectura del contador
			if(ContadorExt.GatewayConectado && (Params.Tipo_Sensor || (ChargingGroup.Params.CDP >> 4 && ChargingGroup.Params.GroupMaster && ChargingGroup.Conected))){
				if(!Counter.Inicializado){
					Counter.begin(ContadorExt.ContadorIp);
                    SendToPSOC5(0, BLOQUEO_CARGA);
				}

				Counter.read();
				Counter.parse();

                if(Params.Tipo_Sensor){
                    uint8 buffer_contador[7] = {0}; 
                
                    buffer_contador[0] = ContadorExt.GatewayConectado;
                    buffer_contador[1] = (uint8)(ContadorExt.DomesticPower& 0x00FF);
                    buffer_contador[2] = (uint8)((ContadorExt.DomesticPower >> 8) & 0x00FF);

                    SendToPSOC5((char*)buffer_contador,3,MEASURES_EXTERNAL_COUNTER);
                }
			}

            //Pausar lectura del contador
            else if(ContadorExt.GatewayConectado && !Params.Tipo_Sensor && !(ChargingGroup.Params.CDP >> 4)){
                ContadorExt.GatewayConectado = false;
                ContadorExt.MeidorConectado = false;
                Counter.Inicializado = false;
                Counter.end();
                Coms.ETH.finding = false;
                Update_Status_Coms(0,MED_BLOCK);
            }
            }
        break;
            
        case DISCONNECTING:{
            uint8_t ip_Array[4] = { 0,0,0,0};
			modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);
            if(!eth_connected && !eth_connecting){        
                Coms.ETH.finding = false;
                if(ContadorExt.GatewayConectado){
                    ContadorExt.GatewayConectado = false;
                    ContadorExt.MeidorConectado = false;
                    Counter.end();
                    Update_Status_Coms(0,MED_BLOCK);
                }
                
                Coms.ETH.State = DISCONNECTED;
                Coms.ETH.Internet = false;
                Coms.ETH.conectado = false;
            }
        }
        break;

        case DISCONNECTED:
            restart_ethernet();
            Coms.ETH.State = CONNECTING;
            xStart = xTaskGetTickCount();
            
        break;
        case KILLING:{   
            uint8_t ip_Array[4] = { 0,0,0,0};
			modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);
            if(!eth_connected && !eth_connecting){
                Coms.ETH.finding = false;
                Coms.ETH.Internet = false;
                Coms.ETH.conectado = false;

                if(ContadorExt.GatewayConectado){
                    ContadorExt.GatewayConectado = false;
                    ContadorExt.MeidorConectado = false;
                    Counter.end();
                    Update_Status_Coms(0,MED_BLOCK);
                }           
                
                wait_for_firebase_stop(100);

                stop_wifi();
                Coms.ETH.Wifi_Perm = false;

                if(Coms.GSM.ON && gsm_connected){
                    FinishGSM();
                }
                Coms.ETH.State = CONNECTING;
                
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
    if(LastStatus!= Coms.ETH.State){
      
      Serial.printf("Maquina de estados de ETH de % i a %i \n", LastStatus, Coms.ETH.State);
      LastStatus= Coms.ETH.State;
    }
    #endif
}

void ComsTask(void *args){
    
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
            if(Coms.StartProvisioning && !Coms.Provisioning){
                if(Coms.GSM.ON){
                    if(gsm_connected){
                        Coms.GSM.ON = false;
                        FinishGSM();
                    }
                }
                ConfigFirebase.InternetConection=0;
                #ifdef DEBUG_WIFI
                Serial.println("Starting provisioning sistem");
                #endif
                Coms.Wifi.ON = true;
                
                if(ServidorArrancado){
                    StopServer();
                    ServidorArrancado = false;
                }
                Coms.ETH.Wifi_Perm = true;

                delay(100);
                initialise_provisioning();
                Coms.Wifi.restart = false;
                Coms.StartProvisioning = false;
            }

            if(Coms.ETH.Internet){
                Coms.GSM.ON = false;
            }
            //Comprobar si hemos perdido todas las conexiones
            if(!Coms.ETH.Internet && !Coms.Wifi.Internet && !Coms.GSM.Internet){
                ConfigFirebase.InternetConection=false;
            }
            else{
                ConfigFirebase.InternetConection=true;
            }

            //Encendido de las interfaces GSM     
            if(Coms.GSM.ON){
                if(!gsm_connected){
                    if(Coms.ETH.ON){
                        if(!Coms.ETH.Internet && Coms.ETH.Wifi_Perm && !Coms.Wifi.Internet){
                            StartGSM();                    
                        }
                    }
                    else if(Coms.ETH.Wifi_Perm && !Coms.Wifi.Internet){
                        StartGSM();
                    }                
                }
                else{
                    if(Coms.GSM.reboot){
                        FinishGSM();
                        delay(2000);
                        StartGSM();
                        Coms.GSM.reboot=false;
                    }
                }                
            }
            if((!Coms.GSM.ON || !Coms.ETH.Wifi_Perm)){
                if(gsm_connected){
                    FinishGSM();
                }
            }

            //Encendido de las interfaces     
            if(Coms.Wifi.ON && !wifi_connected && !wifi_connecting){
                if(Coms.ETH.ON){
                    if(!Coms.ETH.Internet && Coms.ETH.Wifi_Perm ){
                        start_wifi();                     
                    }
                }
                else if(Coms.ETH.Wifi_Perm){
                    start_wifi();
                }                
            }

            else if((!Coms.Wifi.ON || Coms.Wifi.restart || !Coms.ETH.Wifi_Perm) && (wifi_connected || wifi_connecting)){
                stop_wifi();
                Coms.Wifi.restart = false;
            }
            if((eth_connected || wifi_connected) && !ServidorArrancado){
                if(!wifi_connecting ){
                    InitServer();
                    ServidorArrancado = true;
                }
            }
        }
        delay(500);
    }
}
#endif