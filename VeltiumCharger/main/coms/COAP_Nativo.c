#ifdef NO_COMPILE
#include <string.h>
#include <sys/socket.h>

#include "esp_log.h"
#include "esp_event.h"

#include "nvs_flash.h"

//#include "../control.h"


/* Needed until coap_dtls.h becomes a part of libcoap proper */
#include "libcoap.h"
#include "coap_dtls.h"
#include "coap.h"

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Params Params;
extern carac_group  ChargingGroup;
extern carac_chargers FaseChargers;
extern carac_chargers net_group;
extern carac_Comands  Comands ;

static StackType_t xServerStack [4096*4]     EXT_RAM_ATTR;
static StackType_t xClientStack [4096*4]     EXT_RAM_ATTR;
StaticTask_t xServerBuffer;
StaticTask_t xClientBuffer;

/* The examples use simple Pre-Shared-Key configuration that you can set via
   'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_COAP_PSK_KEY "some-agreed-preshared-key"
   Note: PSK will only be used if the URI is prefixed with coaps://
   instead of coap:// and the PSK must be one that the server supports
   (potentially associated with the IDENTITY)
*/
#define EXAMPLE_COAP_PSK_KEY "CONFIG_EXAMPLE_COAP_PSK_KEY"

const static char *TAG = "CoAP_server";

static char espressif_data[100];
static int espressif_data_len = 0;

#define INITIAL_DATA "hola desde coap!"

static void message_handler(coap_context_t *ctx, coap_session_t *session,coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id){
    unsigned char *data = NULL;
    size_t data_len;
    coap_pdu_t *pdu = NULL;
    coap_opt_t *block_opt;
    coap_opt_iterator_t opt_iter;
    unsigned char buf[4];
    coap_optlist_t *option;
    coap_tid_t tid;

    if (COAP_RESPONSE_CLASS(received->code) == 2) {
        /* Need to see if blocked response */
        block_opt = coap_check_option(received, COAP_OPTION_BLOCK2, &opt_iter);
        if (block_opt) {
            uint16_t blktype = opt_iter.type;

            if (coap_opt_block_num(block_opt) == 0) {
                printf("Received:\n");
            }
            if (coap_get_data(received, &data_len, &data)) {
                printf("%.*s", (int)data_len, data);
            }
            if (COAP_OPT_BLOCK_MORE(block_opt)) {
                /* more bit is set */

                /* create pdu with request for next block */
                pdu = coap_new_pdu(session);
                if (!pdu) {
                    ESP_LOGE(TAG, "coap_new_pdu() failed");
                    goto clean_up;
                }
                pdu->type = COAP_MESSAGE_CON;
                pdu->tid = coap_new_message_id(session);
                pdu->code = COAP_REQUEST_GET;

                /* add URI components from optlist */
                for (option = optlist; option; option = option->next ) {
                    switch (option->number) {
                    case COAP_OPTION_URI_HOST :
                    case COAP_OPTION_URI_PORT :
                    case COAP_OPTION_URI_PATH :
                    case COAP_OPTION_URI_QUERY :
                        coap_add_option(pdu, option->number, option->length,
                                        option->data);
                        break;
                    default:
                        ;     /* skip other options */
                    }
                }

                /* finally add updated block option from response, clear M bit */
                /* blocknr = (blocknr & 0xfffffff7) + 0x10; */
                coap_add_option(pdu,
                                blktype,
                                coap_encode_var_safe(buf, sizeof(buf),
                                                     ((coap_opt_block_num(block_opt) + 1) << 4) |
                                                     COAP_OPT_BLOCK_SZX(block_opt)), buf);

                tid = coap_send(session, pdu);

                if (tid != COAP_INVALID_TID) {
                    resp_wait = 1;
                    wait_ms = COAP_DEFAULT_TIME_SEC * 1000;
                    return;
                }
            }
            printf("\n");
        } else {
            if (coap_get_data(received, &data_len, &data)) {
                printf("Received: %.*s\n", (int)data_len, data);
            }
        }
    }
clean_up:
    resp_wait = 0;
}

static void hnd_espressif_put(coap_context_t *ctx,coap_resource_t *resource,coap_session_t *session,coap_pdu_t *request,coap_binary_t *token,coap_string_t *query, coap_pdu_t *response){
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

static void coap_client(void *p)
{
    struct hostent *hp;
    coap_address_t    dst_addr;
    static coap_uri_t uri;
    const char       *server_uri = COAP_DEFAULT_DEMO_URI;
    char *phostname = NULL;

    coap_set_log_level(EXAMPLE_COAP_LOG_DEFAULT_LEVEL);

    while (1) {
        #define BUFSIZE 40
        unsigned char _buf[BUFSIZE];
        unsigned char *buf;
        size_t buflen;
        int res;
        coap_context_t *ctx = NULL;
        coap_session_t *session = NULL;
        coap_pdu_t *request = NULL;

        optlist = NULL;
        if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1) {
            ESP_LOGE(TAG, "CoAP server uri error");
            break;
        }

        if (uri.scheme == COAP_URI_SCHEME_COAPS && !coap_dtls_is_supported()) {
            ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");
            break;
        }
        if (uri.scheme == COAP_URI_SCHEME_COAPS_TCP && !coap_tls_is_supported()) {
            ESP_LOGE(TAG, "CoAP server uri coaps+tcp:// scheme is not supported");
            break;
        }

        phostname = (char *)calloc(1, uri.host.length + 1);
        if (phostname == NULL) {
            ESP_LOGE(TAG, "calloc failed");
            break;
        }

        memcpy(phostname, uri.host.s, uri.host.length);
        hp = gethostbyname(phostname);
        free(phostname);

        if (hp == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            free(phostname);
            continue;
        }
        char tmpbuf[INET6_ADDRSTRLEN];
        coap_address_init(&dst_addr);
        switch (hp->h_addrtype) {
            case AF_INET:
                dst_addr.addr.sin.sin_family      = AF_INET;
                dst_addr.addr.sin.sin_port        = htons(uri.port);
                memcpy(&dst_addr.addr.sin.sin_addr, hp->h_addr, sizeof(dst_addr.addr.sin.sin_addr));
                inet_ntop(AF_INET, &dst_addr.addr.sin.sin_addr, tmpbuf, sizeof(tmpbuf));
                ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);
                break;
            case AF_INET6:
                dst_addr.addr.sin6.sin6_family      = AF_INET6;
                dst_addr.addr.sin6.sin6_port        = htons(uri.port);
                memcpy(&dst_addr.addr.sin6.sin6_addr, hp->h_addr, sizeof(dst_addr.addr.sin6.sin6_addr));
                inet_ntop(AF_INET6, &dst_addr.addr.sin6.sin6_addr, tmpbuf, sizeof(tmpbuf));
                ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);
                break;
            default:
                ESP_LOGE(TAG, "DNS lookup response failed");
                goto clean_up;
        }

        if (uri.path.length) {
            buflen = BUFSIZE;
            buf = _buf;
            res = coap_split_path(uri.path.s, uri.path.length, buf, &buflen);

            while (res--) {
                coap_insert_optlist(&optlist,
                                    coap_new_optlist(COAP_OPTION_URI_PATH,
                                                     coap_opt_length(buf),
                                                     coap_opt_value(buf)));

                buf += coap_opt_size(buf);
            }
        }

        if (uri.query.length) {
            buflen = BUFSIZE;
            buf = _buf;
            res = coap_split_query(uri.query.s, uri.query.length, buf, &buflen);

            while (res--) {
                coap_insert_optlist(&optlist,
                                    coap_new_optlist(COAP_OPTION_URI_QUERY,
                                                     coap_opt_length(buf),
                                                     coap_opt_value(buf)));

                buf += coap_opt_size(buf);
            }
        }

        ctx = coap_new_context(NULL);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed");
            goto clean_up;
        }

        /*
         * Note that if the URI starts with just coap:// (not coaps://) the
         * session will still be plain text.
         *
         * coaps+tcp:// is NOT supported by the libcoap->mbedtls interface
         * so COAP_URI_SCHEME_COAPS_TCP will have failed in a test above,
         * but the code is left in for completeness.
         */
        if (uri.scheme == COAP_URI_SCHEME_COAPS || uri.scheme == COAP_URI_SCHEME_COAPS_TCP) {
            session = coap_new_client_session_psk(ctx, NULL, &dst_addr,
                                                  uri.scheme == COAP_URI_SCHEME_COAPS ? COAP_PROTO_DTLS : COAP_PROTO_TLS,
                                                  EXAMPLE_COAP_PSK_IDENTITY,
                                                  (const uint8_t *)EXAMPLE_COAP_PSK_KEY,
                                                  sizeof(EXAMPLE_COAP_PSK_KEY) - 1);


        } else {
            session = coap_new_client_session(ctx, NULL, &dst_addr,
                                              uri.scheme == COAP_URI_SCHEME_COAP_TCP ? COAP_PROTO_TCP :
                                              COAP_PROTO_UDP);
        }
        if (!session) {
            ESP_LOGE(TAG, "coap_new_client_session() failed");
            goto clean_up;
        }

        coap_register_response_handler(ctx, message_handler);

        request = coap_new_pdu(session);
        if (!request) {
            ESP_LOGE(TAG, "coap_new_pdu() failed");
            goto clean_up;
        }
        request->type = COAP_MESSAGE_CON;
        request->tid = coap_new_message_id(session);
        request->code = COAP_REQUEST_GET;
        coap_add_optlist_pdu(request, &optlist);

        resp_wait = 1;
        coap_send(session, request);

        wait_ms = COAP_DEFAULT_TIME_SEC * 1000;

        while (resp_wait) {
            int result = coap_run_once(ctx, wait_ms > 1000 ? 1000 : wait_ms);
            if (result >= 0) {
                if (result >= wait_ms) {
                    ESP_LOGE(TAG, "select timeout");
                    break;
                } else {
                    wait_ms -= result;
                }
            }
        }
clean_up:
        if (optlist) {
            coap_delete_optlist(optlist);
            optlist = NULL;
        }
        if (session) {
            coap_session_release(session);
        }
        if (ctx) {
            coap_free_context(ctx);
        }
        coap_cleanup();
        /*
         * change the following line to something like sleep(2)
         * if you want the request to continually be sent
         */
        break;
    }

    vTaskDelete(NULL);
}



static void coap_server(void *p){

    coap_context_t *ctx = NULL;
    coap_address_t serv_addr;
    coap_resource_t *DATA = NULL;
    coap_resource_t *PARAMS = NULL;
    coap_resource_t *CONTROL = NULL;
    coap_resource_t *CHARGERS = NULL;
    

    snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
    espressif_data_len = strlen(espressif_data);
    coap_set_log_level(LOG_NOTICE);

    while (1) {
        coap_endpoint_t *ep = NULL;
        unsigned wait_ms;

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin.sin_family      = AF_INET;
        serv_addr.addr.sin.sin_addr.s_addr = Coms.ETH.IP.addr;
        serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);

        ctx = coap_new_context(NULL);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed");
            continue;
        }
        /* Need PSK setup before we set up endpoints */
        coap_context_set_psk(ctx, "CoAP",(const uint8_t *)EXAMPLE_COAP_PSK_KEY,sizeof(EXAMPLE_COAP_PSK_KEY) - 1);


        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_UDP);
        if (!ep) {
            ESP_LOGE(TAG, "udp: coap_new_endpoint() failed");
            goto clean_up;
        }

        if (coap_dtls_is_supported()) {
            serv_addr.addr.sin.sin_port = htons(COAPS_DEFAULT_PORT);
            ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_DTLS);
            if (!ep) {
                ESP_LOGE(TAG, "dtls: coap_new_endpoint() failed");
                goto clean_up;
            }
        }
    
        //Añadir los topics
        DATA     = coap_resource_init(coap_make_str_const("DATA"), COAP_RESOURCE_FLAGS_RELEASE_URI);
        PARAMS   = coap_resource_init(coap_make_str_const("PARAMS"), COAP_RESOURCE_FLAGS_RELEASE_URI);
        CONTROL  = coap_resource_init(coap_make_str_const("CONTROL"), COAP_RESOURCE_FLAGS_RELEASE_URI);
        CHARGERS = coap_resource_init(coap_make_str_const("CHARGERS"), COAP_RESOURCE_FLAGS_RELEASE_URI);

        if (!DATA || !PARAMS || !CONTROL || !CHARGERS) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }

        //coap_register_handler(DATA, COAP_REQUEST_GET, hnd_espressif_get);
        coap_register_handler(DATA, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(PARAMS, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(CONTROL, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(CHARGERS, COAP_REQUEST_PUT, hnd_espressif_put);

        /* We possibly want to Observe the GETs */
        coap_resource_set_get_observable(DATA, 1);
        coap_resource_set_get_observable(PARAMS, 1);
        coap_resource_set_get_observable(CONTROL, 1);
        coap_resource_set_get_observable(CHARGERS, 1);

        coap_add_resource(ctx, DATA);
        coap_add_resource(ctx, PARAMS);
        coap_add_resource(ctx, CONTROL);
        coap_add_resource(ctx, CHARGERS);

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

void start_server(void){
    xTaskCreateStatic(coap_server, "coap_server", 4096*4, NULL, 5, xServerStack,&xServerBuffer);
}

void start_client(void){
    xTaskCreateStatic(coap_client, "coap_client", 4096*4, NULL, 5, xClientStack,&xClientBuffer);
}
#endif