#include "Eth_Station.h"
#include "esp_netif.h"
#include "esp_eth_netif_glue.h"
#include "esp_http_client.h"
#include "Wifi_Station.h"



bool eth_link_up     = false;
bool eth_connected   = false;
bool eth_connecting  = false;
bool eth_started     = false;

extern bool wifi_connected;

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Contador   ContadorExt;

static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_handle_t s_eth_handle1 = NULL;
static esp_eth_handle_t s_eth_handle2 = NULL;
static uint8_t s_eth_mac[6];
esp_eth_mac_t *mac;
esp_eth_phy_t *phy=NULL;
esp_eth_phy_t *phy1=NULL;
esp_eth_phy_t *phy2=NULL;
uint8 ZeroBuffer[10] ={'0'};
esp_netif_t *eth_netif;
esp_netif_t *eth_netif1;
esp_netif_t *eth_netif2;
//Variables para buscar el contador
xParametrosPing ParametrosPing EXT_RAM_ATTR;



bool CheckContador(int ip4){
    Contador ContadorCheck;

    char ip[15]={"0"};
    sprintf(ip,"%i.%i.%i.%i", ip4_addr1(&ParametrosPing.BaseAdress),ip4_addr2(&ParametrosPing.BaseAdress),ip4_addr3(&ParametrosPing.BaseAdress),ip4);
    ContadorCheck.begin(ip);
    for (int i=0; i<2;i++){
        if(ContadorCheck.read()){           
            if(ContadorCheck.parseModel()){
                ContadorCheck.end();
                memcpy(ContadorExt.ContadorIp, ip,15);
                return true;
            }
            else{
                ContadorCheck.end();
                return false;
            } 
        }
    }
    ContadorCheck.end();
    return false;
}

void pingResults(ping_target_id_t msgType, esp_ping_found * pf){
    /*Serial.println(pf->max_time);
    Serial.println(pf->resp_time);
    Serial.println(pf->total_time);
    Serial.println(pf->ping_err);
    Serial.println(pf->recv_count);
    Serial.println(pf->recv_count);*/

    if(pf->timeout_count == ParametrosPing.ping_count){     

        ParametrosPing.NextOne=true;
        ping_deinit();
    }
    if(pf->max_time != 0){      
        if(ParametrosPing.CheckingConn){
            Serial.println("Conexion a internet detectada!"); 
            ParametrosPing.CheckingConn = false;
            ConfigFirebase.InternetConection = true;
        }
        ParametrosPing.Found= true;
        ping_deinit();
        return;
    }
    
}

void BuscarContador_Task(void *args){

    ip4_addr_t BaseAdress = ParametrosPing.BaseAdress;
    uint8 i = 100;
    uint8 Sup = 1, inf = 1 ;
    bool Sentido = 0;
    bool TopeInf = false;
    bool TopeSup = false;
    i = ip4_addr4(&BaseAdress);
    ParametrosPing.Found = false;
    ParametrosPing.NextOne =true;

    while(!TopeSup || !TopeInf){
        if(ParametrosPing.NextOne){
            if(Sentido && !TopeInf){ //Pabajo
                i = ip4_addr4(&ParametrosPing.BaseAdress) - inf > 1 ? ip4_addr4(&ParametrosPing.BaseAdress) - inf : 0;
                if(i != 0 ){
                    inf++;
                    Sentido = false;
                }
                else{
                    TopeInf = true;
                    Sentido = false;
                    continue;
                }
            }
            else if(!Sentido && !TopeSup){ //Parriba
                i = ip4_addr4(&ParametrosPing.BaseAdress) + Sup < 255 ? ip4_addr4(&ParametrosPing.BaseAdress) + Sup : 255;
                if(i != 254 ){
                    Sup++;
                    Sentido = true;
                }
                else{
                    TopeSup = true;
                    Sentido = true;
                    continue;
                }
            }
            if(i==-1){
                break;
            }
            
            /*printf("Buscando en %i \n", i);
            
            esp_ping_set_target(PING_TARGET_RES_RESET, &BaseAdress, sizeof(uint32_t));
            IP4_ADDR(&BaseAdress, ip4_addr1(&BaseAdress),ip4_addr2(&BaseAdress),ip4_addr3(&BaseAdress),i);
            esp_ping_set_target(PING_TARGET_IP_ADDRESS_COUNT, &ParametrosPing.ping_count, sizeof(uint32_t));
            esp_ping_set_target(PING_TARGET_RCV_TIMEO, &ParametrosPing.ping_timeout, sizeof(uint32_t));
            esp_ping_set_target(PING_TARGET_DELAY_TIME, &ParametrosPing.ping_delay, sizeof(uint32_t));
            esp_ping_set_target(PING_TARGET_IP_ADDRESS, &BaseAdress, sizeof(uint32_t));
            func(pingResults);
            ping_init();*/
            ParametrosPing.NextOne = false;
            ParametrosPing.Found = true;
            i++;
        }
        else if(ParametrosPing.Found ){
            if(CheckContador(i-1)){
                Serial.println("Busqueda finalizada!");
                Serial.printf("Contador encontrado en %d %d %d %d \n", ip4_addr1(&BaseAdress), ip4_addr2(&BaseAdress), ip4_addr3(&BaseAdress), i-1);
	            ContadorExt.ContadorConectado = true;
                break;
            }
            else{
                Serial.println("Nada, seguimos buscando");
                ParametrosPing.Found= false;
                ParametrosPing.NextOne = true;
            }
        }
        delay(20);
    }
    if(!ContadorExt.ContadorConectado){
        Serial.println("No he encontrado ningun medidor");
    }
    vTaskDelete(NULL);
}

bool ComprobarConexion(){
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/get",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        if(esp_http_client_get_status_code(client) == 200){
            esp_http_client_cleanup(client);
            Serial.println("Detectada conexion a internet!");
            return true;
        }
    }
    esp_http_client_cleanup(client);
    Serial.println("Sin salida a internet detectada");
	return false;
}

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    if(event_base == ETH_EVENT){
        switch (event_id) {
            case ETHERNET_EVENT_CONNECTED:

                Serial.printf( "Ethernet Link Up: %u \n", 1);
                esp_eth_ioctl(s_eth_handle1, ETH_CMD_G_MAC_ADDR, s_eth_mac);
                Serial.printf("Ethernet1 HW Addr %02x:%02x:%02x:%02x:%02x:%02x \n",s_eth_mac[0], s_eth_mac[1], s_eth_mac[2], s_eth_mac[3], s_eth_mac[4], s_eth_mac[5]);
                
                break;
            case ETHERNET_EVENT_CONNECTED2:
                Serial.printf( "Ethernet Link Up: %u \n", 2);
                esp_eth_ioctl(s_eth_handle2, ETH_CMD_G_MAC_ADDR, s_eth_mac);
                Serial.printf("Ethernet2 HW Addr %02x:%02x:%02x:%02x:%02x:%02x \n",s_eth_mac[0], s_eth_mac[1], s_eth_mac[2], s_eth_mac[3], s_eth_mac[4], s_eth_mac[5]);
                
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                Serial.println( "Ethernet Link 1 Down");
                eth_connected = false;
                eth_link_up = false;
                Coms.ETH.conectado = false;
                break;
            case ETHERNET_EVENT_DISCONNECTED2:
                Serial.println( "Ethernet Link 2 Down");
                eth_connected = false;
                eth_link_up = false;
                Coms.ETH.conectado = false;
                break;
            case ETHERNET_EVENT_START:
                Serial.println( "Ethernet 1 Started");
                break;
            case ETHERNET_EVENT_START2:
                Serial.println( "Ethernet 2 Started");
                break;
            case ETHERNET_EVENT_STOP:
                Serial.println( "Ethernet 1 Stopped");
                eth_connected = false;
                break;
            case ETHERNET_EVENT_STOP2:
                Serial.println( "Ethernet 2 Stopped");
                eth_connected = false;
                break;
            default:
                break;
        }
    }
    else{
        if(event_id == IP_EVENT_ETH_GOT_IP){
            ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
            const esp_netif_ip_info_t *ip_info = &event->ip_info;

            Serial.println( "Ethernet 1 Got IP Address");
            Serial.printf( "ETHIP: %d %d %d %d\n",IP2STR(&ip_info->ip));
            Coms.ETH.IP1[0] =ip4_addr1(&ip_info->ip);
            Coms.ETH.IP1[1] =ip4_addr2(&ip_info->ip);
            Coms.ETH.IP1[2] =ip4_addr3(&ip_info->ip);
            Coms.ETH.IP1[3] =ip4_addr4(&ip_info->ip);

            if(Coms.ETH.Puerto==1){
                Coms.ETH.IP1[0] =ip4_addr1(&ip_info->ip);
                Coms.ETH.IP1[1] =ip4_addr2(&ip_info->ip);
                Coms.ETH.IP1[2] =ip4_addr3(&ip_info->ip);
                Coms.ETH.IP1[3] =ip4_addr4(&ip_info->ip);               
                modifyCharacteristic(&Coms.ETH.IP1[0], 4, COMS_CONFIGURATION_LAN_IP1);
                modifyCharacteristic(ZeroBuffer, 4, COMS_CONFIGURATION_LAN_IP2);
            }
            else if(Coms.ETH.Puerto==2){
                Coms.ETH.IP2[0] =ip4_addr1(&ip_info->ip);
                Coms.ETH.IP2[1] =ip4_addr2(&ip_info->ip);
                Coms.ETH.IP2[2] =ip4_addr3(&ip_info->ip);
                Coms.ETH.IP2[3] =ip4_addr4(&ip_info->ip); 
                modifyCharacteristic(&Coms.ETH.IP2[0], 4, COMS_CONFIGURATION_LAN_IP2);
                modifyCharacteristic(ZeroBuffer, 4, COMS_CONFIGURATION_LAN_IP1);
            }

            eth_connected = true;
            eth_connecting = false;

            //Si el ethernet tiene conexion a internet, deasactivamos el wifi
            delay(1000);
            if(ComprobarConexion()){
                ConfigFirebase.InternetConection = true;
                Coms.Wifi.ON = false;
            }
            Coms.ETH.conectado = true;
        }
        if(event_id == IP_EVENT_ETH_GOT_IP2){
            ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
            const esp_netif_ip_info_t *ip_info = &event->ip_info;

            Serial.println( "Ethernet 2 Got IP Address");
            Serial.printf( "ETHIP: %d %d %d %d\n",IP2STR(&ip_info->ip));
            Coms.ETH.IP1[0] =ip4_addr1(&ip_info->ip);
            Coms.ETH.IP1[1] =ip4_addr2(&ip_info->ip);
            Coms.ETH.IP1[2] =ip4_addr3(&ip_info->ip);
            Coms.ETH.IP1[3] =ip4_addr4(&ip_info->ip);

            if(Coms.ETH.Puerto==1){
                Coms.ETH.IP1[0] =ip4_addr1(&ip_info->ip);
                Coms.ETH.IP1[1] =ip4_addr2(&ip_info->ip);
                Coms.ETH.IP1[2] =ip4_addr3(&ip_info->ip);
                Coms.ETH.IP1[3] =ip4_addr4(&ip_info->ip);               
                modifyCharacteristic(&Coms.ETH.IP1[0], 4, COMS_CONFIGURATION_LAN_IP1);
                modifyCharacteristic(ZeroBuffer, 4, COMS_CONFIGURATION_LAN_IP2);
            }
            else if(Coms.ETH.Puerto==2){
                Coms.ETH.IP2[0] =ip4_addr1(&ip_info->ip);
                Coms.ETH.IP2[1] =ip4_addr2(&ip_info->ip);
                Coms.ETH.IP2[2] =ip4_addr3(&ip_info->ip);
                Coms.ETH.IP2[3] =ip4_addr4(&ip_info->ip); 
                modifyCharacteristic(&Coms.ETH.IP2[0], 4, COMS_CONFIGURATION_LAN_IP2);
                modifyCharacteristic(ZeroBuffer, 4, COMS_CONFIGURATION_LAN_IP1);
            }

            eth_connected = true;
            eth_connecting = false;

            //Si el ethernet tiene conexion a internet, deasactivamos el wifi
            delay(1000);
            if(ComprobarConexion()){
                ConfigFirebase.InternetConection = true;
                Coms.Wifi.ON = false;
            }
            Coms.ETH.conectado = true;
        }
    }
}

void initialize_ethernet(void)
{  
    Coms.ETH.Alone = 0;
    Coms.ETH.Auto  = 1;
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();

    //servidor DHCP
    if(Coms.ETH.Alone){
        esp_netif_inherent_config_t config;
        config = *cfg.base;
        config.flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP);
        config.route_prio = 10;
        cfg.base = &config;
        eth_netif= esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(eth_netif));

        esp_netif_ip_info_t DHCP_Server_IP;
        IP4_ADDR(&DHCP_Server_IP.ip, 192, 168, 1, 1);
        IP4_ADDR(&DHCP_Server_IP.gw, 192, 168, 1, 1);
        IP4_ADDR(&DHCP_Server_IP.netmask, 255, 255, 255, 0);
        ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &DHCP_Server_IP)); 

        ESP_ERROR_CHECK(esp_netif_dhcps_start(eth_netif));

        esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_START, esp_netif_action_start, eth_netif);
        esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_STOP, esp_netif_action_stop, eth_netif);
        eth_connected = true;
        
    }   
    //Si fijamos una IP estatica:
    else if(!Coms.ETH.Auto){
        eth_netif = esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));
        esp_netif_ip_info_t  info;
        memset(&info, 0, sizeof(info));
        IP4_ADDR(&info.ip, 192, 168, 1, 5);
	    IP4_ADDR(&info.gw, 192, 168, 1, 1);
	    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &info));  
        esp_eth_set_default_handlers(eth_netif);
    }
    //Configuracion automatica
    else{
        eth_netif = esp_netif_new(&cfg);
        esp_eth_set_default_handlers(eth_netif);
    }

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_event_handler, NULL));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);

    phy_config.phy_addr = ETH_ADDR;
    phy_config.reset_gpio_num = ETH_POWER_PIN;
    phy_config.phy_addr1 = 1;
    phy_config.phy_addr2 = 2;
    phy = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);

    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(s_eth_handle)));
    esp_eth_ioctl(s_eth_handle, ETH_CMD_S_PROMISCUOUS, (void *)true);
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

    eth_connecting = true;
    eth_started = true;
}

void initialize_ethernet_2(void)
{  
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_inherent_config inherent_cfg = *cfg.base;
    inherent_cfg.if_key = "ETH2";
    inherent_cfg.get_ip_event = IP_EVENT_ETH_GOT_IP2;
    cfg.base = &inherent_cfg;


    eth_netif2 = esp_netif_new(&cfg);

    //ip estatica
    /*ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif2));
    esp_netif_ip_info_t  info;
    memset(&info, 0, sizeof(info));
    IP4_ADDR(&info.ip, 192, 168, 1, 5);
    IP4_ADDR(&info.gw, 192, 168, 1, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif2, &info)); */

    esp_eth_set_default_handlers2(eth_netif2);
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP2, eth_event_handler, NULL));
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = ETH_POWER_PIN;
    phy2 = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy2);
    config.Port=2;

    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle2));
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif2, esp_eth_new_netif_glue(s_eth_handle2)));
    esp_eth_ioctl(s_eth_handle2, ETH_CMD_S_PROMISCUOUS, (void *)true);
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle2));

    eth_connecting = true;
    eth_started = true;
}

void initialize_ethernet_1(void)
{  
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_inherent_config inherent_cfg = *cfg.base;
    inherent_cfg.if_key = "ETH1";
    cfg.base = &inherent_cfg;

    eth_netif1 = esp_netif_new(&cfg);
    esp_eth_set_default_handlers(eth_netif1);

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_event_handler, NULL));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    mac = esp_eth_mac_new_esp32(&mac_config);

    phy_config.phy_addr = 2;
    phy_config.reset_gpio_num = ETH_POWER_PIN;
    phy1 = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy1);

    config.Port=1;

    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle1));
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif1, esp_eth_new_netif_glue(s_eth_handle1)));
    esp_eth_ioctl(s_eth_handle1, ETH_CMD_S_PROMISCUOUS, (void *)true);
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle1));

    eth_connecting = true;
    eth_started = true;
}

void kill_ethernet(void){
    stop_ethernet();
    esp_netif_destroy(eth_netif);
    esp_eth_driver_uninstall(s_eth_handle);
}

bool stop_ethernet(void){
    if(esp_eth_stop(s_eth_handle) != ESP_OK){
        Serial.println("esp_eth_stop failed");
        return false;
    }
    if(Coms.ETH.Alone){
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(eth_netif));
    }
    eth_connected  = false;
    eth_connecting = false;
    delay(500);
    return true;
}

bool restart_ethernet(void){
    if(esp_eth_start(s_eth_handle) != ESP_OK){
        Serial.println("esp_eth_stop failed");
        return false;
    }
    if(Coms.ETH.Alone){
        ESP_ERROR_CHECK(esp_netif_dhcps_start(eth_netif));
    }
    eth_connecting = true;
    delay(100);
    return true;
}