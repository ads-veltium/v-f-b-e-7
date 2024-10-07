#include "Eth_Station.h"
#ifdef IS_UNO_KUBO

#include "VeltFirebase.h"
#include "lwip/sys.h"
#include "lwip/dns.h"
#include "esp_log.h"

#define TRABAJANDO   0
#define PING_END     1
#define PING_SUCCESS 2
#define PING_TIMEOUT 3

static const char* TAG = "Eth_Station";

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Contador ContadorExt;
extern carac_Params Params;

#ifdef CONNECTED
extern carac_group  ChargingGroup;
#endif

static esp_eth_handle_t s_eth_handle = NULL;

esp_eth_mac_t *mac = NULL;
esp_eth_phy_t *phy=NULL;
esp_netif_t *eth_netif;

esp_ip4_addr_t Global_IP;

void initialize_ping(ip_addr_t target_addr, uint8_t *resultado);
void end_ping();

void BuscarContador_Task(void *args){
/*
    bool *finding = (bool*) args;
    uint8 i = 100;
    uint8 Sup = 1, inf = 1 ;
    char ip[16]={"0"};
    bool Sentido = 0;
    bool TopeInf = false;
    bool TopeSup = false;
    if(ip4_addr1(&Coms.ETH.IP)==0&& ip4_addr2(&Coms.ETH.IP)&&ip4_addr3(&Coms.ETH.IP)){
        return;
    }
    i =ip4_addr4(&Coms.ETH.IP);
    bool NextOne =true;

    Meter_HTTP_Client Cliente("http://192.168.1.1", 1000);
    Cliente.begin();
#ifdef DEBUG_ETH
    Serial.println("Eth_Station - BuscarContador_Task: Empezamos busqueda de medidor");
    Serial.println("Eth_Station - BuscarContador_Task: Espera de 5 segundos para iniciar la búsqueda\n");
#endif
    vTaskDelay (pdMS_TO_TICKS(5000)); // ADS - 5 segundos antes de iniciar la búsqueda. (El medidor a veces no tiene IP al iniciarla)
    Update_Status_Coms(MED_BUSCANDO_GATEWAY);
    ContadorExt.Searching = true;
    while(!TopeSup || !TopeInf){
        if(NextOne){    
            if(Sentido && !TopeInf){ //Pabajo
                i = ip4_addr4(&Coms.ETH.IP) - inf > 1 ? ip4_addr4(&Coms.ETH.IP) - inf : 0;
                if(i > 0 ){
                    inf++;
                    if(!TopeSup){
                        Sentido = false;
                    }
                }
                else{
                    TopeInf = true;
                    Sentido = false;
                    continue;
                }
            }
            else if(!Sentido && !TopeSup){ //Parriba
                if(Coms.ETH.DHCP){
                    i = ip4_addr4(&Coms.ETH.IP) + Sup < 65 ? ip4_addr4(&Coms.ETH.IP) + Sup : 65;
                    if(i != 65 ){
                    Sup++;
                    if(!TopeInf){
                        Sentido = true;
                        }
                    }
                    else{
                        TopeSup = true;
                        Sentido = true;
                        continue;
                    }
                }
                else{
                    i = ip4_addr4(&Coms.ETH.IP) + Sup < 253 ? ip4_addr4(&Coms.ETH.IP) + Sup : 253;
                    if(i != 253 ){
                    Sup++;
                    if(!TopeInf){
                        Sentido = true;
                        }
                    }
                    else{
                        TopeSup = true;
                        Sentido = true;
                        continue;
                    }
                }                
            }
            if(i-1==-1){
                break;
            }
            NextOne = false;
            i++;
        }
        else{            
            sprintf(ip,"%i.%i.%i.%i", ip4_addr1(&Coms.ETH.IP),ip4_addr2(&Coms.ETH.IP),ip4_addr3(&Coms.ETH.IP),i-1);
#ifdef DEBUG_ETH
            ESP_LOGI(TAG,"BuscarContador_Task: Dirección IP buscada: %s", ip);
#endif
            ip_addr_t target;
            ipaddr_aton(ip,&target);
            uint8_t resultado = 0;

            initialize_ping(target, &resultado);
            uint8_t timeout = 0;
            while(resultado == TRABAJANDO && ++timeout < 20){
                delay(15);
            }
            end_ping();
            if(resultado == PING_SUCCESS){
#ifdef DEBUG_MEDIDOR
                Serial.printf("Eth_Station - BuscarContador_Task: PING_SUCCESS a %s\n", ip);
#endif
                char url[100] = "http://";
                strcat(url, ip);
                delay(100); // ADS 100 así por las buenas
                if(Cliente.Send_Command(url,METER_READ)){               
                    String respuesta = Cliente.ObtenerRespuesta();  
#ifdef DEBUG_MEDIDOR
                    Serial.printf("Eth_Station - BuscarContador_Task: Buscando Iskra. Respuesta = %s\n", respuesta.c_str());
#endif    
                    if(respuesta.indexOf("Iskra")>-1){
                        strcpy(ContadorExt.IPadd,ip);
                        ContadorExt.GatewayConectado = true;
                        Update_Status_Coms(MED_BUSCANDO_MEDIDOR);
                        ContadorExt.Searching = false;
                        break;
                    }
                }
                if(ContadorExt.GatewayConectado){
                    break;
                }
            }
            if(!Params.Tipo_Sensor && !ContadorConfigurado()){
                Coms.ETH.Wifi_Perm = true;
                break;
            }
            NextOne = true;
        }
        delay(100);
    }
    Cliente.end();

    if(!ContadorExt.GatewayConectado){
        *finding = false;

        //si es la primera vez que intentamos buscarlo, dejamos de hacerlo
        if(!ContadorExt.ConexionPerdida){
            if(++ContadorExt.vueltas >= 2){
                Params.Tipo_Sensor = 0;
                Coms.ETH.medidor = 0;
                Coms.ETH.restart = true;
                ContadorExt.vueltas = 0;
                SendToPSOC5((uint8_t)9, DOMESTIC_CONSUMPTION_DOMESTIC_CURRENT_CHAR_HANDLE);
            }
        }
    }
    else{
        Coms.ETH.Wifi_Perm = true;
    }
    vTaskDelete(NULL);
*/
}

bool ComprobarConexion(void){
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/get",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    for(int i =0;i<=1;i++){
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            if(esp_http_client_get_status_code(client) == 200){
                esp_http_client_cleanup(client);
                return true;
            }
        }
        delay(250);
    }

    esp_http_client_cleanup(client);
	return false;
}

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    if(event_base == ETH_EVENT){
#ifdef DEBUG_ETH
    Serial.printf("Eth_Station - eth_event_handler: Event = %i \n", event_id);
#endif
        switch (event_id) {
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI(TAG,"link1=%i link2=%i",phy->link1,phy->link2);
                if(phy->link1 && phy->link2)Coms.ETH.Puerto = 3;
                else if(phy->link1)Coms.ETH.Puerto = 1;
                else if(phy->link2)Coms.ETH.Puerto = 2;
#ifdef DEBUG_ETH
                Serial.printf("Eth_Station - eth_event_handler: ETHERNET_EVENT_CONNECTED: Ethernet Link Up: %u \n", Coms.ETH.Puerto);
#endif
                Coms.ETH.LinkUp = true;

              break;
            case ETHERNET_EVENT_START:
#ifdef DEBUG_ETH
                Serial.println("Eth_Station - eth_event_handler: ETHERNET_EVENT_START - Ethernet Started");
#endif
                Update_Status_Coms(0,ETH_BLOCK);
                break;
            case ETHERNET_EVENT_STOP:
                ESP_LOGI(TAG,"link1=%i link2=%i",phy->link1,phy->link2);
#ifdef DEBUG_ETH
                Serial.println("Eth_Station - eth_event_handler: ETHERNET_EVENT_STOP - Ethernet Stopped");
#endif
                Update_Status_Coms(0,ETH_BLOCK);
                Update_Status_Coms(0,MED_BLOCK);
                ContadorExt.GatewayConectado = false;
                ContadorExt.MedidorConectado = false;
                if(!Coms.ETH.DHCP){
                    Coms.ETH.Wifi_Perm = true;
                }
                Coms.ETH.connected = false;
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                ESP_LOGI(TAG,"link1=%i link2=%i",phy->link1,phy->link2);
#ifdef DEBUG_ETH
                Serial.println("Eth_Station - eth_event_handler: ETHERNET_EVENT_DISCONNECTED - Ethernet Link Down");
#endif
                Update_Status_Coms(0,ETH_BLOCK);

                if(!Coms.ETH.DHCP){
                    Coms.ETH.connected = false;
                    Coms.ETH.LinkUp = false;
                    Coms.ETH.conectado = false;
                    Coms.ETH.Internet = false;
                }

                break;
            default:
                break;
        }
    }
    else{
        if(event_id == IP_EVENT_ETH_GOT_IP){
            ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
            const esp_netif_ip_info_t *ip_info = &event->ip_info;
#ifdef DEBUG_ETH
            Serial.printf("Eth_Station - eth_event_handler: IP_EVENT_ETH_GOT_IP IP: %d %d %d %d\n", IP2STR(&ip_info->ip));
#endif
            Update_Status_Coms(ETH_CONNECTED);

            Coms.ETH.IP.addr = ip_info->ip.addr;
            Coms.ETH.Mask.addr = ip_info->netmask.addr;
            Coms.ETH.Gateway.addr = ip_info->gw.addr;

            if (Coms.ETH.ON){
                uint8_t ip_Array[4] = { ip4_addr1(&Coms.ETH.IP),ip4_addr2(&Coms.ETH.IP),ip4_addr3(&Coms.ETH.IP),ip4_addr4(&Coms.ETH.IP)};
                modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);
            }

            delay(1000);

            Coms.ETH.connected = true;
            Coms.ETH.connecting = false;
            Coms.ETH.conectado = true;
            Global_IP = ip_info->ip;

        }
    }
}


void initialize_ethernet(void){  
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
#ifdef DEBUG_ETH
    Serial.println("Eth_Station - initialize_ethernet: Arrancando ethernet");
#endif
    if(Coms.ETH.DHCP){
#ifdef DEBUG_ETH
        Serial.println("Eth_Station - initialize_ethernet: Arrancando con servidor DHCP");
#endif
        Update_Status_Coms(ETH_DHCP_ACTIVE);
        esp_netif_inherent_config_t config;
        config = *cfg.base;
        config.flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP);
        config.route_prio = 10;
        cfg.base = &config;
        eth_netif= esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(eth_netif));
        // Configuración manual de la dirección IP
        esp_netif_ip_info_t DHCP_Server_IP;
        if(Global_IP.addr == IPADDR_ANY){
            IP4_ADDR(&DHCP_Server_IP.ip, 192, 168, 15, 22);        
            IP4_ADDR(&DHCP_Server_IP.gw, 192, 168, 15, 22);
        }
        else{
            DHCP_Server_IP.ip = Global_IP;
            DHCP_Server_IP.gw = Global_IP;
        }
        IP4_ADDR(&DHCP_Server_IP.netmask, 255, 255, 255, 0);
        Coms.ETH.IP.addr = DHCP_Server_IP.ip.addr;
        ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &DHCP_Server_IP)); 
        ESP_ERROR_CHECK(esp_netif_dhcps_start(eth_netif));
        esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_START, esp_netif_action_start, eth_netif);
        esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_STOP, esp_netif_action_stop, eth_netif);
        Coms.ETH.connected = true;
        uint8_t ip_Array[4] = { 0,0,0,0};
		modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);
        Coms.ETH.conectado = true;
#ifdef DEBUG_ETH
        Serial.printf("Eth_Station - initialize_ethernet: Server DHCP - Dirección IP asignada: %s\n", ip4addr_ntoa((const ip4_addr_t*)&DHCP_Server_IP.ip));
#endif
    }   
    /*
    //Si fijamos una IP estatica:
    else if(!Coms.ETH.Auto && Coms.ETH.IP.addr != IPADDR_ANY){
        #ifdef DEBUG_ETH
            Serial.println("Arrancando con IP estatica!");
        #endif
        eth_netif = esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));
        esp_netif_ip_info_t  info;
        memset(&info, 0, sizeof(info));
        info.ip.addr      = Coms.ETH.IP.addr;
        info.gw.addr      = Coms.ETH.Gateway.addr;
        info.netmask.addr = Coms.ETH.Mask.addr;
	    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &info));  
        esp_eth_set_default_handlers(eth_netif);
        Coms.ETH.connected = true;
        uint8_t ip_Array[4] = { ip4_addr1(&Coms.ETH.IP),ip4_addr2(&Coms.ETH.IP),ip4_addr3(&Coms.ETH.IP),ip4_addr4(&Coms.ETH.IP)};
        modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);
        uint8_t  on =1;
        modifyCharacteristic(&on, 1, COMS_CONFIGURATION_ETH_ON);	
    }*/
    //Configuracion automatica
    else{
#ifdef DEBUG_ETH
        Serial.println("Eth_Station - initialize_ethernet: Arrancando en automático SIN servidor DHCP\n");
#endif
        eth_netif = esp_netif_new(&cfg);
        esp_eth_set_default_handlers(eth_netif);
        uint8_t on =1;
        if (Coms.ETH.ON){
            modifyCharacteristic(&on, 1, COMS_CONFIGURATION_ETH_ON);
        }
    }

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_event_handler, NULL));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    if(mac == NULL){
        mac = esp_eth_mac_new_esp32(&mac_config);
    }

    phy = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);

    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(s_eth_handle)));
    esp_eth_ioctl(s_eth_handle, ETH_CMD_S_PROMISCUOUS, (void *)true);
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

    Coms.ETH.connecting = true;
}

void kill_ethernet(void){
#ifdef DEBUG_ETH
    Serial.printf("Eht_Station - kill_ethernet\n");
#endif
    stop_ethernet();

    esp_eth_clear_default_handlers(eth_netif);
    
    esp_netif_destroy(eth_netif);
    esp_eth_driver_uninstall(s_eth_handle); /// ADS - OJOJOJOJO

    ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_event_handler));
}

bool stop_ethernet(void){
#ifdef DEBUG_ETH
    Serial.printf("Eht_Station - stop_ethernet: stop_ethernet\n");
#endif
    if(esp_eth_stop(s_eth_handle) != ESP_OK){
        return false;
    }
    Coms.ETH.connected  = false;
    Coms.ETH.connecting = false;
    delay(500);
    return true;
}

bool restart_ethernet(void){
#ifdef DEBUG_ETH
    Serial.println("Eht_Station - restart_ethernet\n");
#endif
    if(esp_eth_start(s_eth_handle) != ESP_OK){
        return false;
    }
    Coms.ETH.connecting = true;
    delay(100);
    return true;
}
#endif