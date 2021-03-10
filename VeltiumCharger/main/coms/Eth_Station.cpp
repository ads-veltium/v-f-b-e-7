#include "Eth_Station.h"
#include "../control.h"

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

void tcpipInit();
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
            //modifyCharacteristic(ZeroBuffer, 4, COMS_CONFIGURATION_LAN_IP2);
        }
        else if(Coms.ETH.Puerto==2){
            //Coms.ETH.IP2 = ETH.localIP();
            //modifyCharacteristic(&ip_info->ip, 4, COMS_CONFIGURATION_LAN_IP2);
            //modifyCharacteristic(ZeroBuffer, 4, COMS_CONFIGURATION_LAN_IP1);
        }

        eth_connected = true;
        eth_connecting = false;
        ConfigFirebase.InternetConection=true;
        break;
    }
    default:
        break;
    }
}

void initialize_ethernet(void)
{

    tcpipInit();

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