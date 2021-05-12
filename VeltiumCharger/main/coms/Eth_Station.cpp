#include "Eth_Station.h"
#include "VeltFirebase.h"
#include "lwip/sys.h"

#define TRABAJANDO   0
#define PING_END     1
#define PING_SUCCESS 2
#define PING_TIMEOUT 3

bool eth_link_up     = false;
bool eth_connected   = false;
bool eth_connecting  = false;
bool eth_started     = false;

extern bool wifi_connected;

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Contador   ContadorExt;
extern carac_Params Params;
extern carac_group  ChargingGroup;

static esp_eth_handle_t s_eth_handle = NULL;

esp_eth_mac_t *mac = NULL;
esp_eth_phy_t *phy=NULL;

esp_netif_t *eth_netif;

void initialize_ping(ip_addr_t target_addr, uint8_t *resultado);
void end_ping();

void BuscarContador_Task(void *args){

    uint8 i = 100;
    uint8 Sup = 1, inf = 1 ;
    char ip[16]={"0"};
    bool Sentido = 0;
    bool TopeInf = false;
    bool TopeSup = false;
    i =ip4_addr4(&Coms.ETH.IP);
    bool NextOne =true;

    Cliente_HTTP Cliente("http://192.168.1.1", 1000);
    Cliente.begin();

    while(!TopeSup || !TopeInf){
        if(NextOne){
            if(Sentido && !TopeInf){ //Pabajo
                i = ip4_addr4(&Coms.ETH.IP) - inf > 1 ? ip4_addr4(&Coms.ETH.IP) - inf : 0;
                if(i > 0 ){
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
                i = ip4_addr4(&Coms.ETH.IP) + Sup < 255 ? ip4_addr4(&Coms.ETH.IP) + Sup : 255;
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
            if(i-1==-1){
                break;
            }
            NextOne = false;
            i++;
        }
        else{            
            sprintf(ip,"%i.%i.%i.%i", ip4_addr1(&Coms.ETH.IP),ip4_addr2(&Coms.ETH.IP),ip4_addr3(&Coms.ETH.IP),i-1); 
            printf("%s\n", ip);
            ip_addr_t target;
            ipaddr_aton(ip,&target);
            uint8_t resultado = 0;
            initialize_ping(target, &resultado);

            while(resultado == TRABAJANDO){
                delay(10);
            }
            end_ping();

            if(resultado == PING_SUCCESS){           
                char url[100] = "http://";
                strcat(url, ip);
                strcat(url, "/get_command?command=get_measurements");

                if(Cliente.Send_Command(url,LEER)) {

                    String respuesta = Cliente.ObtenerRespuesta();               
                    if(respuesta.indexOf("IE38MD")>-1){
                        strcpy(ContadorExt.ContadorIp,ip);
                        ContadorExt.ContadorConectado = true;
                        break;
                    }
                }
            }
            Serial.println("Nada, seguimos buscando");
            NextOne = true;
        }
        delay(250);
    }
    Coms.ETH.Wifi_Perm = true;
    Cliente.end();
    
    if(!ContadorExt.ContadorConectado){
        Serial.println("No he encontrado ningun medidor");
    }
    vTaskDelete(NULL);
}

bool ComprobarConexion(void){
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/get",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    for(int i =0;i<=5;i++){
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            if(esp_http_client_get_status_code(client) == 200){
                esp_http_client_cleanup(client);
                Serial.println("Detectada conexion a internet!");
                return true;
            }
        }
        delay(250);
    }

    esp_http_client_cleanup(client);
    Serial.println("Sin salida a internet detectada");
	return false;
}

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    if(event_base == ETH_EVENT){
        switch (event_id) {
            case ETHERNET_EVENT_CONNECTED:
                if(phy->link1 && phy->link2)Coms.ETH.Puerto = 3;
                else if(phy->link1)Coms.ETH.Puerto = 1;
                else if(phy->link2)Coms.ETH.Puerto = 2;

                Serial.printf( "Ethernet Link Up: %u \n", Coms.ETH.Puerto);
                eth_link_up    = true;

              break;
            case ETHERNET_EVENT_START:
                Serial.println( "Ethernet Started");
                break;
            case ETHERNET_EVENT_STOP:
                Serial.println( "Ethernet Stopped");
                eth_connected = false;
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                Serial.println( "Ethernet Link Down");
                eth_connected = false;
                eth_link_up = false;
                Coms.ETH.conectado = false;
                Coms.ETH.Internet = false;
                ChargingGroup.StopOrder = true;
                break;
            default:
                break;
        }
    }
    else{
        if(event_id == IP_EVENT_ETH_GOT_IP){
            ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
            const esp_netif_ip_info_t *ip_info = &event->ip_info;

            Serial.printf( "ETHIP: %d %d %d %d\n",IP2STR(&ip_info->ip));
            
            Coms.ETH.IP.addr = ip_info->ip.addr;

            uint8_t ip_Array[4] = { ip4_addr1(&Coms.ETH.IP),ip4_addr2(&Coms.ETH.IP),ip4_addr3(&Coms.ETH.IP),ip4_addr4(&Coms.ETH.IP)};
            modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);

            eth_connected = true;
            eth_connecting = false;

            //Si el ethernet tiene conexion a internet, deasactivamos el wifi
            delay(5000);
            Coms.ETH.conectado = true;
        }
    }
}

void initialize_ethernet(void){  

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    Serial.println("Arrancando ethernet");
    //servidor DHCP
    if(!Coms.ETH.Auto && Params.Tipo_Sensor){
        //si tenemos IP estatica y un medidor conectado, activamos el dhcp
        //Coms.ETH.DHCP = true;
        SendToPSOC5(Coms.ETH.DHCP,COMS_CONFIGURATION_ETH_DHCP);
    }

    if(Coms.ETH.DHCP){
        Serial.println("Arrancando servidor dhcp!");
        esp_netif_inherent_config_t config;
        config = *cfg.base;
        config.flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP);
        config.route_prio = 10;
        cfg.base = &config;
        eth_netif= esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(eth_netif));

        esp_netif_ip_info_t DHCP_Server_IP;
        DHCP_Server_IP.ip.addr=Coms.ETH.IP.addr;
        DHCP_Server_IP.gw.addr=Coms.ETH.Gateway.addr;
        DHCP_Server_IP.netmask.addr=Coms.ETH.Mask.addr;
        ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &DHCP_Server_IP)); 

        ESP_ERROR_CHECK(esp_netif_dhcps_start(eth_netif));

        esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_START, esp_netif_action_start, eth_netif);
        esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_STOP, esp_netif_action_stop, eth_netif);
        eth_connected = true;
        
        uint8_t ip_Array[4] = { ip4_addr1(&Coms.ETH.IP),ip4_addr2(&Coms.ETH.IP),ip4_addr3(&Coms.ETH.IP),ip4_addr4(&Coms.ETH.IP)};
        modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);
    }   
    
    //Si fijamos una IP estatica:
    else if(!Coms.ETH.Auto){
        Serial.println("Arrancando con IP estatica!");
        eth_netif = esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));
        esp_netif_ip_info_t  info;
        memset(&info, 0, sizeof(info));
        info.ip.addr=Coms.ETH.IP.addr;
        info.gw.addr=Coms.ETH.Gateway.addr;
        info.netmask.addr=Coms.ETH.Mask.addr;
	    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &info));  
        esp_eth_set_default_handlers(eth_netif);
        eth_connected = true;
    }
    //Configuracion automatica
    else{
        Serial.println("Arrancando en automatico");
        eth_netif = esp_netif_new(&cfg);
        esp_eth_set_default_handlers(eth_netif);
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

    eth_connecting = true;
    eth_started = true;
}

void kill_ethernet(void){
    stop_ethernet();

    esp_eth_clear_default_handlers(eth_netif);
    
    esp_netif_destroy(eth_netif);
    esp_eth_driver_uninstall(s_eth_handle);

    ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_event_handler));
}

bool stop_ethernet(void){
    if(esp_eth_stop(s_eth_handle) != ESP_OK){
        Serial.println("esp_eth_stop failed");
        return false;
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
    eth_connecting = true;
    delay(100);
    return true;
}