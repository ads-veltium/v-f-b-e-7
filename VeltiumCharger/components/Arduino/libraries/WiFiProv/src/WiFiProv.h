 /*
    WiFiProv.h - Base class for provisioning support
    All right reserved.
 
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
  
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
  
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "WiFi.h"
#include "wifi_provisioning/manager.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp32-hal.h>

#include <nvs_flash.h>

#include <wifi_provisioning/scheme_softap.h>
#include <wifi_provisioning/manager.h>
#undef IPADDR_NONE

//Select the scheme using which you want to provision
typedef enum {
    WIFI_PROV_SCHEME_SOFTAP,
    WIFI_PROV_SCHEME_MAX
} prov_scheme_t;

typedef enum {
    WIFI_PROV_SCHEME_HANDLER_NONE,
    WIFI_PROV_SCHEME_HANDLER_MAX
} scheme_handler_t;

//Provisioning class 
class WiFiProvClass
{  
    public:
        bool StopProvision();
        uint8_t beginProvision(prov_scheme_t prov_scheme = WIFI_PROV_SCHEME_SOFTAP, scheme_handler_t scheme_handler = WIFI_PROV_SCHEME_HANDLER_NONE,
        		wifi_prov_security_t security = WIFI_PROV_SECURITY_1, const char * pop = "abcd1234", const char * service_name = NULL, bool provisioning = false, const char * service_key = NULL, uint8_t *uuid = NULL);
};

extern WiFiProvClass WiFiProv;
