#include "Eth_Station.h"



static const char *TAG = "Eth_Controller";
bool eth_link_up     = false;
bool eth_connected   = false;
bool eth_connecting  = false;
bool eth_started     = false;

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;

static esp_eth_handle_t s_eth_handle = NULL;
static uint8_t s_eth_mac[6];
esp_eth_phy_t *phy=NULL;
esp_eth_phy_t *phy2=NULL;
uint8 ZeroBuffer[10] ={'0'};

//Variables para buscar el contador
xParametrosPing ParametrosPing EXT_RAM_ATTR;
void tcpipInit();

bool CheckContador(int ip4){
    Contador ContadorCheck;

    char ip[15]={"0"};
    sprintf(ip,"%i.%i.%i.%i", ip4_addr1(&ParametrosPing.BaseAdress),ip4_addr2(&ParametrosPing.BaseAdress),ip4_addr3(&ParametrosPing.BaseAdress),ip4);
    ContadorCheck.begin(ip);
    for (int i=0; i<=2;i++){
        if(ContadorCheck.read()){           
            if(ContadorCheck.parseModel()){
                ContadorCheck.end();
                memcpy(Coms.ContadorIp, ip,15);
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

    if(pf->timeout_count == ParametrosPing.ping_count){     

        ParametrosPing.NextOne=true;
        ping_deinit();
    }
    if(pf->max_time != 0){      
        if(ParametrosPing.CheckingConn){
            Serial.println("Conexion a internet detectada!"); 
            ParametrosPing.CheckingConn = false;
            ConfigFirebase.InternetConection = true;
            if(Coms.Wifi.ON){
                Station_Stop();
            }
        }
        ParametrosPing.Found= true;
        ping_deinit();
        return;
    }
    
}

void BuscarContador_Task(void *args){

    ip4_addr_t BaseAdress = ParametrosPing.BaseAdress;
    uint8 i = 100;
   
    i = ip4_addr4(&BaseAdress);
    ParametrosPing.Found = false;
    ParametrosPing.NextOne =true;

    while(i <= ParametrosPing.max){
        if(ParametrosPing.NextOne){
            printf("Buscando en %i \n", i);
            ParametrosPing.NextOne = false;
            esp_ping_set_target(PING_TARGET_RES_RESET, &BaseAdress, sizeof(uint32_t));
            IP4_ADDR(&BaseAdress, ip4_addr1(&BaseAdress),ip4_addr2(&BaseAdress),ip4_addr3(&BaseAdress),i);
            esp_ping_set_target(PING_TARGET_IP_ADDRESS_COUNT, &ParametrosPing.ping_count, sizeof(uint32_t));
            esp_ping_set_target(PING_TARGET_RCV_TIMEO, &ParametrosPing.ping_timeout, sizeof(uint32_t));
            esp_ping_set_target(PING_TARGET_DELAY_TIME, &ParametrosPing.ping_delay, sizeof(uint32_t));
            esp_ping_set_target(PING_TARGET_IP_ADDRESS, &BaseAdress, sizeof(uint32_t));
            
            func(pingResults);
            ping_init();
            i++;
        }
        else if(ParametrosPing.Found ){
            Serial.println("Hemos encontrado algo, a ver si es un contador");
            if(CheckContador(i-1)){
                break;
            }
            else{
                ParametrosPing.Found= false;
                ParametrosPing.NextOne = true;
            }
        }
        delay(50);
    }
    Serial.println("Busqueda finalizada!");
    Serial.printf("Contador encontrado en %d %d %d %d \n", ip4_addr1(&BaseAdress), ip4_addr2(&BaseAdress), ip4_addr3(&BaseAdress), i-1);
	Coms.ContadorConectado = true;
    vTaskDelete(NULL);
}


bool ComprobarConexion(){
    uint32_t ping_count = 5;  //how many pings per report
    uint32_t ping_timeout = 1000; //mS till we consider it timed out
    uint32_t ping_delay = 100; //mS between pings
    
	esp_ping_set_target(PING_TARGET_IP_ADDRESS_COUNT, &ping_count, sizeof(uint32_t));
    esp_ping_set_target(PING_TARGET_RCV_TIMEO, &ping_timeout, sizeof(uint32_t));
    esp_ping_set_target(PING_TARGET_DELAY_TIME, &ping_delay, sizeof(uint32_t));
    esp_ping_set_target(PING_TARGET_IP_ADDRESS, &ParametrosPing.BaseAdress, sizeof(uint32_t));
    func(pingResults);
    ping_init();
    
	return ESP_OK;
}
// Event handler for Ethernet
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:

        if(phy->link1 && phy->link2)Coms.ETH.Puerto = 3;
        else if(phy->link1)Coms.ETH.Puerto = 1;
        else if(phy->link2)Coms.ETH.Puerto = 2;

        Serial.printf( "Ethernet Link Up: %u \n", Coms.ETH.Puerto);
        eth_link_up    = true;
        esp_eth_ioctl(s_eth_handle, ETH_CMD_G_MAC_ADDR, s_eth_mac);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        Serial.println( "Ethernet Link Down");
        eth_connected = false;
        eth_link_up = false;
        Coms.ETH.conectado = false;
        break;
    case ETHERNET_EVENT_START:
        Serial.println( "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        Serial.println( "Ethernet Stopped");
        eth_connected = false;
        break;
    case IP_EVENT_ETH_GOT_IP:{
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        const tcpip_adapter_ip_info_t *ip_info = &event->ip_info;

        Serial.println( "Ethernet Got IP Address");
        Serial.printf( "ETHIP: %d %d %d %d\n",IP2STR(&ip_info->ip));

        if(Coms.ETH.Puerto==1){
            //Coms.ETH.IP1 = ETH.localIP();               
            //modifyCharacteristic(ip_info->ip, 4, COMS_CONFIGURATION_LAN_IP1);
            modifyCharacteristic(ZeroBuffer, 4, COMS_CONFIGURATION_LAN_IP2);
        }
        else if(Coms.ETH.Puerto==2){
            //Coms.ETH.IP2 = ETH.localIP();
            //modifyCharacteristic(&ip_info->ip, 4, COMS_CONFIGURATION_LAN_IP2);
            modifyCharacteristic(ZeroBuffer, 4, COMS_CONFIGURATION_LAN_IP1);
        }

        eth_connected = true;
        eth_connecting = false;

        
        //Comprobar si la red tiene conexion a internet
        IP4_ADDR(&ParametrosPing.BaseAdress , 8,8,8,8);
        ParametrosPing.CheckingConn = true;
        ComprobarConexion();
        Coms.ETH.conectado = true;
        break;
    }
    default:
        break;
    }
}

void initialize_ethernet(void)
{

    tcpipInit();

    Coms.ETH.Auto = 1;
    if(!Coms.ETH.Auto){
        ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH));
        tcpip_adapter_ip_info_t  info;
        memset(&info, 0, sizeof(info));
        IP4_ADDR(&info.ip, 192, 168, 1, 20);
	    IP4_ADDR(&info.gw, 192, 168, 1, 1);
	    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &info));  
    }
 
    ESP_ERROR_CHECK(tcpip_adapter_set_default_eth_handlers());

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_event_handler, NULL));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    mac_config.smi_mdc_gpio_num = ETH_MDC_PIN;
    mac_config.smi_mdio_gpio_num = ETH_MDIO_PIN;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);

    phy_config.phy_addr = ETH_ADDR;
    phy_config.reset_gpio_num = ETH_POWER_PIN;
    phy_config.phy_addr1 = 1;
    phy_config.phy_addr2 = 2;
    phy = esp_eth_phy_new_lan8720(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));

    esp_eth_ioctl(s_eth_handle, ETH_CMD_S_PROMISCUOUS, (void *)true);
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

    eth_connecting = true;
    eth_started = true;
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