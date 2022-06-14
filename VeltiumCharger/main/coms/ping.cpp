#include "../control.h"
#ifdef CONNECTED
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "ping/ping_sock.h"

#define TRABAJANDO   0
#define PING_END     1
#define PING_SUCCESS 2
#define PING_TIMEOUT 3

esp_ping_handle_t ping;

static void test_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    // optionally, get callback arguments
    uint8_t *resultado = (uint8_t*) args;
    // printf("%s\r\n", str); // "foo"
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    *resultado = PING_SUCCESS;
}

static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint8_t *resultado = (uint8_t*) args;
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    *resultado = PING_TIMEOUT;
}

static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint8_t *resultado = (uint8_t*) args;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    *resultado = PING_END;
}

void end_ping(){
    esp_ping_stop(ping);
    esp_ping_delete_session(ping);
}

void initialize_ping(ip_addr_t target_addr, uint8_t *resultado)
{
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;          // target IP address
    ping_config.count = ESP_PING_COUNT_INFINITE;    // ping in infinite mode, esp_ping_stop can stop it
    ping_config.timeout_ms  = 250;
    ping_config.interval_ms = 1000;
    /* set callback functions */
    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = test_on_ping_success;
    cbs.on_ping_timeout = test_on_ping_timeout;
    cbs.on_ping_end = test_on_ping_end;
    cbs.cb_args = resultado;  // arguments that will feed to all callback functions, can be NULL
    
    esp_ping_new_session(&ping_config, &cbs, &ping);
    esp_ping_start(ping);
}

#endif