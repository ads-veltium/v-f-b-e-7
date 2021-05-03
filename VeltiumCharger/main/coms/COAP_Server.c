#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"

#include "nvs_flash.h"

#if 1
/* Needed until coap_dtls.h becomes a part of libcoap proper */
#include "libcoap.h"
#include "coap_dtls.h"
#endif
#include "coap.h"

/* The examples use simple Pre-Shared-Key configuration that you can set via
   'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_COAP_PSK_KEY "some-agreed-preshared-key"
   Note: PSK will only be used if the URI is prefixed with coaps://
   instead of coap:// and the PSK must be one that the server supports
   (potentially associated with the IDENTITY)
*/
#define EXAMPLE_COAP_PSK_KEY "ZEiMl5SCGI9MEpAJ08Opu1iqJL0F1VhVX6rlwZvE9K83bPrIugEDQM++gQJIrizPJV/MDzZtrU5/v29dUCRLCw=="

/* The examples use CoAP Logging Level that
   you can set via 'make menuconfig'.
   If you'd rather not, just change the below entry to a value
   that is between 0 and 7 with
   the config you want - ie #define EXAMPLE_COAP_LOG_DEFAULT_LEVEL 7
*/
#define EXAMPLE_COAP_LOG_DEFAULT_LEVEL CONFIG_COAP_LOG_DEFAULT_LEVEL

const static char *TAG = "CoAP_server";

static char espressif_data[100];
static int espressif_data_len = 0;


#define INITIAL_DATA "Hello World!"

/*
 * The resource handler
 */
static void
hnd_espressif_get(coap_context_t *ctx, coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request, coap_binary_t *token,
                  coap_string_t *query, coap_pdu_t *response){
    coap_add_data_blocked_response(resource, session, request, response, token,
                                   COAP_MEDIATYPE_TEXT_PLAIN, 0,
                                   (size_t)espressif_data_len,
                                   (const u_char *)espressif_data);
}

static void
hnd_espressif_put(coap_context_t *ctx,
                  coap_resource_t *resource,
                  coap_session_t *session,
                  coap_pdu_t *request,
                  coap_binary_t *token,
                  coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    unsigned char *data;

    coap_resource_notify_observers(resource, NULL);

    if (strcmp (espressif_data, INITIAL_DATA) == 0) {
        response->code = COAP_RESPONSE_CODE(201);
    } else {
        response->code = COAP_RESPONSE_CODE(204);
    }

    /* coap_get_data() sets size to 0 on error */
    (void)coap_get_data(request, &size, &data);

    if (size == 0) {      /* re-init */
        snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
        espressif_data_len = strlen(espressif_data);
    } else {
        espressif_data_len = size > sizeof (espressif_data) ? sizeof (espressif_data) : size;
        memcpy (espressif_data, data, espressif_data_len);
    }
}

static void
hnd_espressif_delete(coap_context_t *ctx,
                     coap_resource_t *resource,
                     coap_session_t *session,
                     coap_pdu_t *request,
                     coap_binary_t *token,
                     coap_string_t *query,
                     coap_pdu_t *response)
{
    coap_resource_notify_observers(resource, NULL);
    snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
    espressif_data_len = strlen(espressif_data);
    response->code = COAP_RESPONSE_CODE(202);
}

static void coap_example_server(void *p)
{
    coap_context_t *ctx = NULL;
    coap_address_t serv_addr;
    coap_resource_t *resource = NULL;

    snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
    espressif_data_len = strlen(espressif_data);
    coap_set_log_level(EXAMPLE_COAP_LOG_DEFAULT_LEVEL);

    while (1) {
        coap_endpoint_t *ep = NULL;
        unsigned wait_ms;

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin.sin_family      = AF_INET;
        serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
        serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);

        ctx = coap_new_context(NULL);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed");
            continue;
        }
#ifdef CONFIG_COAP_MBEDTLS_PSK
        /* Need PSK setup before we set up endpoints */
        coap_context_set_psk(ctx, "CoAP",(const uint8_t *)EXAMPLE_COAP_PSK_KEY,sizeof(EXAMPLE_COAP_PSK_KEY) - 1);
#endif /* CONFIG_COAP_MBEDTLS_PSK */

        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_UDP);
        if (!ep) {
            ESP_LOGE(TAG, "udp: coap_new_endpoint() failed");
            goto clean_up;
        }
        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_TCP);
        if (!ep) {
            ESP_LOGE(TAG, "tcp: coap_new_endpoint() failed");
            goto clean_up;
        }
#if defined(CONFIG_COAP_MBEDTLS_PSK)
        if (coap_dtls_is_supported()) {
#ifndef CONFIG_MBEDTLS_TLS_SERVER
            /* This is not critical as unencrypted support is still available */
            ESP_LOGI(TAG, "MbedTLS (D)TLS Server Mode not configured");
#else /* CONFIG_MBEDTLS_TLS_SERVER */
            serv_addr.addr.sin.sin_port = htons(COAPS_DEFAULT_PORT);
            ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_DTLS);
            if (!ep) {
                ESP_LOGE(TAG, "dtls: coap_new_endpoint() failed");
                goto clean_up;
            }
#endif /* CONFIG_MBEDTLS_TLS_SERVER */
        } else {
            /* This is not critical as unencrypted support is still available */
            ESP_LOGI(TAG, "MbedTLS (D)TLS Server Mode not configured");
        }
#endif /* CONFIG_COAP_MBEDTLS_PSK CONFIG_COAP_MBEDTLS_PKI */
        resource = coap_resource_init(coap_make_str_const("Espressif"), 0);
        if (!resource) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_GET, hnd_espressif_get);
        coap_register_handler(resource, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(resource, COAP_REQUEST_DELETE, hnd_espressif_delete);
        /* We possibly want to Observe the GETs */
        coap_resource_set_get_observable(resource, 1);
        coap_add_resource(ctx, resource);

        wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;

        while (1) {
            int result = coap_run_once(ctx, wait_ms);
            if (result < 0) {
                break;
            } else if (result && (unsigned)result < wait_ms) {
                /* decrement if there is a result wait time returned */
                wait_ms -= result;
            }
            if (result) {
                /* result must have been >= wait_ms, so reset wait_ms */
                wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;
            }
        }
    }
clean_up:
    coap_free_context(ctx);
    coap_cleanup();

    vTaskDelete(NULL);
}

void start_coaps(void)
{

    xTaskCreate(coap_example_server, "coap", 8 * 1024, NULL, 5, NULL);
}
