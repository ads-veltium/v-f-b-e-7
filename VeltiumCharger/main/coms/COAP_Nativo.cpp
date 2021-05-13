#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"
#include "cJSON.h"
#include "../group_control.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "nvs_flash.h"
#include "../control.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libcoap.h"
#include "coap_dtls.h"
#include "coap.h"

#ifdef __cplusplus
}
#endif

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
TaskHandle_t xServerHandle = NULL;
TaskHandle_t xClientHandle = NULL;

coap_context_t * ctx      EXT_RAM_ATTR;
coap_session_t * session  EXT_RAM_ATTR;
coap_optlist_t * optlist  EXT_RAM_ATTR;

coap_resource_t *DATA EXT_RAM_ATTR;
coap_resource_t *PARAMS  EXT_RAM_ATTR;
coap_resource_t *CONTROL  EXT_RAM_ATTR;
coap_resource_t *CHARGERS  EXT_RAM_ATTR; 
coap_resource_t *TXANDA EXT_RAM_ATTR;

const static char *TAG = "CoAP_server";
uint8_t Esperando_datos =0;
uint8_t  FallosEnvio =0;
uint8_t turno =0;
char LastData[500] EXT_RAM_ATTR;

/*****************************
 * Funciones externas
 * **************************/
IPAddress get_IP(const char* ID);
String Encipher(String input);
uint8_t check_in_group(const char* ID, carac_chargers* group);

bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);
bool remove_from_group(const char* ID ,carac_chargers* group);

void store_group_in_mem(carac_chargers* group);
void broadcast_a_grupo(char* Mensaje, uint16_t size);
void send_to(IPAddress IP,  char* Mensaje);
void coap_put( char* Topic, char* Message);

/*****************************
 * Prototipos de funciones 
 * **************************/
void Send_Params();
void Send_Data();
void Send_Chargers();
String get_passwd();
void MasterPanicTask(void *args);


static void
hnd_get(coap_context_t *ctx, coap_resource_t *resource,coap_session_t *session,coap_pdu_t *request, coap_binary_t *token,coap_string_t *query, coap_pdu_t *response){    
    response->code = COAP_RESPONSE_CODE(200);
    char buffer[500];
    if(!memcmp(resource->uri_path->s, "CHARGERS", resource->uri_path->length)){
        itoa(GROUP_CHARGERS, buffer, 10);
        
        if(ChargingGroup.group_chargers.size< 10){
            sprintf(&buffer[1],"0%i",(char)ChargingGroup.group_chargers.size);
        }
        else{
            sprintf(&buffer[1],"%i",(char)ChargingGroup.group_chargers.size);
        }
        
        for(uint8_t i=0;i< ChargingGroup.group_chargers.size;i++){
            memcpy(&buffer[3+(i*9)],ChargingGroup.group_chargers.charger_table[i].name,8);   
            itoa(ChargingGroup.group_chargers.charger_table[i].Fase,&buffer[11+(i*9)],10);
        }
    }
    else if(!memcmp(resource->uri_path->s, "PARAMS", resource->uri_path->length)){
        itoa(GROUP_PARAMS, buffer, 10);
        cJSON *Params_Json;
        Params_Json = cJSON_CreateObject();

        cJSON_AddNumberToObject(Params_Json, "cdp", ChargingGroup.Params.CDP);
        cJSON_AddNumberToObject(Params_Json, "contract", ChargingGroup.Params.ContractPower);
        cJSON_AddNumberToObject(Params_Json, "active", ChargingGroup.Params.GroupActive);
        cJSON_AddNumberToObject(Params_Json, "master", ChargingGroup.Params.GroupMaster);
        cJSON_AddNumberToObject(Params_Json, "inst_max", ChargingGroup.Params.inst_max);
        cJSON_AddNumberToObject(Params_Json, "pot_max", ChargingGroup.Params.potencia_max);
        cJSON_AddNumberToObject(Params_Json, "userID", ChargingGroup.Params.UserID);

        char *my_json_string = cJSON_Print(Params_Json);   
        cJSON_Delete(Params_Json); 

        int size = strlen(my_json_string);
        memcpy(&buffer[1], my_json_string, size);
        buffer[size+1]='\0';

        free(my_json_string);
    }
    else if(!memcmp(resource->uri_path->s, "CONTROL", resource->uri_path->length)){
        itoa(GROUP_CONTROL, buffer, 10);

    }
    else if(!memcmp(resource->uri_path->s, "DATA", resource->uri_path->length)){
        itoa(NEW_DATA, buffer, 10);
        int size = strlen(LastData);
        memcpy(&buffer[1], LastData, size);
        buffer[size+1]='\0';

    }
    else if(!memcmp(resource->uri_path->s, "TXANDA", resource->uri_path->length)){
        sprintf(buffer,"%i%i",TURNO, turno);
    }
    coap_add_data_blocked_response(resource, session, request, response, token,COAP_MEDIATYPE_TEXT_PLAIN, 0,(size_t)strlen((char*)buffer),(const u_char*)buffer);
}

static void message_handler(coap_context_t *ctx, coap_session_t *session,coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id){
    uint8_t *data = NULL;
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
                    Esperando_datos =0;
                    return;
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
                coap_add_option(pdu,blktype,coap_encode_var_safe(buf, sizeof(buf), ((coap_opt_block_num(block_opt) + 1) << 4) | COAP_OPT_BLOCK_SZX(block_opt)), buf);

                tid = coap_send(session, pdu);

                if (tid != COAP_INVALID_TID) {
                    return;
                }
            }
            printf("\n");
        } else {
            if (coap_get_data(received, &data_len, &data)) {
                printf("Received2: %.*s\n", (int)data_len, data);
            }
        }
    }
    if(data != NULL){
        switch(data[0]-'0'){
            case GROUP_PARAMS: 
                New_Params(&data[1], data_len-1);
                break;
            case GROUP_CONTROL:
                New_Control(&data[1],  data_len-1);
                break;
            case GROUP_CHARGERS:
                New_Group(&data[1],  data_len-1);
                break;
            case NEW_DATA:
                New_Data(&data[1],  data_len-1);
                break;
            case TURNO:
                if(!memcmp(ChargingGroup.group_chargers.charger_table[data[1]-'0'].name,ConfigFirebase.Device_Id, 8)){
                    printf("Debo mandar mis datos!\n");
                    Send_Data();
                }
                else{
                    printf("No me toca a mí\n");
                }
                
                break;
            default:
                printf("DAtos no esperaddos\n");
                break;
        }
    }
    

    Esperando_datos = 0;
}

static void hnd_espressif_put(coap_context_t *ctx,coap_resource_t *resource,coap_session_t *session,coap_pdu_t *request,coap_binary_t *token,coap_string_t *query, coap_pdu_t *response){
    size_t size;
    unsigned char *data;
    (void)coap_get_data(request, &size, &data);

    if(!memcmp(resource->uri_path->s, "CHARGERS", resource->uri_path->length)){
        New_Group(data,  size);
    }
    else if(!memcmp(resource->uri_path->s, "PARAMS", resource->uri_path->length)){
        New_Params(data, size);
    }
    else if(!memcmp(resource->uri_path->s, "CONTROL", resource->uri_path->length)){
        New_Control(data,  size);
    }
    else if(!memcmp(resource->uri_path->s, "DATA", resource->uri_path->length)){
        New_Data(data,  size);
        memcpy(LastData, data, size);
        //Pedir datos a los esclavos para que no envíen todos a la vez
        turno++;        
        delay(250);
        if(turno == ChargingGroup.group_chargers.size){
            turno=1;
            Send_Data(); //Mandar mis datos
        }
    }

    coap_resource_notify_observers(resource, NULL);

    response->code = COAP_RESPONSE_CODE(200);
}

static bool POLL(int timeout){
    while (Esperando_datos) {
        int result = coap_run_once(ctx, timeout);
        if (result >= 0) {
            if (result >= timeout) {
                ESP_LOGE(TAG, "select timeout");
                FallosEnvio++;
                return false;
            } else {
                timeout -= result;
            }
        }
    }
    FallosEnvio=0;
    return true;
}

void coap_get( char* Topic){
    coap_pdu_t *request = NULL;

    if (optlist) {
        coap_delete_optlist(optlist);
        optlist = NULL;
    }

    coap_insert_optlist(&optlist,coap_new_optlist(COAP_OPTION_URI_PATH,strlen(Topic),(uint8_t*)Topic));
    request = coap_new_pdu(session);
    if (!request) {
        ESP_LOGE(TAG, "coap_new_pdu() failed");
        return;
    }
    request->type = COAP_MESSAGE_CON;
    request->tid = coap_new_message_id(session);
    request->code = COAP_REQUEST_GET;

    coap_add_optlist_pdu(request, &optlist);

    coap_send(session, request);
    Esperando_datos =   1;

}

static bool Authenticate(){
    printf("Autenticandome contra el servidor!!!\n");

    Esperando_datos=1;
    
    coap_pdu_t *request = NULL;
    coap_insert_optlist(&optlist,coap_new_optlist(COAP_OPTION_URI_PATH,strlen("PARAMS"),(uint8_t*)"PARAMS"));
    request = coap_new_pdu(session);

    if (!request) {
        ESP_LOGE(TAG, "coap_new_pdu() failed");
        return false;
    }
    coap_add_optlist_pdu(request, &optlist);

    request->type = COAP_MESSAGE_CON;
    request->tid = coap_new_message_id(session);
    request->code = COAP_REQUEST_GET;

    coap_add_optlist_pdu(request, &optlist);

    coap_send(session, request);
    Esperando_datos =   1;
    POLL(20000);

        
    if(Esperando_datos){
         if (session) {
            coap_session_release(session);
        }
        delay(10000);
        return false;
    }

    printf("Autneticados!!!\n");
    return true;
}

static void Subscribe(char* TOPIC){
    coap_pdu_t *request = NULL;

    request = coap_new_pdu(session);

    //Subscribirnos
    if (!request) {
        ESP_LOGE(TAG, "coap_new_pdu() failed");
        return;
    }
    request->type = COAP_MESSAGE_CON;
    request->tid = coap_new_message_id(session);
    request->code = COAP_REQUEST_GET;

    if (optlist) {
        coap_delete_optlist(optlist);
        optlist = NULL;
    }

    uint8_t buf[4];
    coap_insert_optlist(&optlist,coap_new_optlist(COAP_OPTION_URI_PATH,strlen(TOPIC),(uint8_t*)TOPIC));
    coap_insert_optlist(&optlist,coap_new_optlist(COAP_OPTION_OBSERVE,coap_encode_var_safe(buf, sizeof(buf),COAP_OBSERVE_ESTABLISH), buf));

    coap_add_optlist_pdu(request, &optlist);

    coap_send(session, request);
    Esperando_datos =1;
}

void coap_put( char* Topic, char* Message){
    if(ChargingGroup.Params.GroupMaster){
        if(!memcmp(DATA->uri_path->s, Topic, DATA->uri_path->length)){
            strcpy(LastData, Message);
           // memcpy(LastData, Message, strlen(Message));
            coap_resource_notify_observers(DATA, NULL);
            New_Data(Message,  strlen(Message));
        }
        else if(!memcmp(PARAMS->uri_path->s, Topic, PARAMS->uri_path->length)){
            New_Params(Message,  strlen(Message));
            coap_resource_notify_observers(PARAMS, NULL);
        }
        else if(!memcmp(CONTROL->uri_path->s, Topic, CONTROL->uri_path->length)){
            New_Control(Message,  strlen(Message));
            coap_resource_notify_observers(CONTROL, NULL);
        }
        else if(!memcmp(CHARGERS->uri_path->s, Topic, CHARGERS->uri_path->length)){
            New_Group(Message,  strlen(Message));
            coap_resource_notify_observers(CHARGERS, NULL);
        }
    }
    else{
        coap_pdu_t *request = NULL;

        if (optlist) {
            coap_delete_optlist(optlist);
            optlist = NULL;
        }

        coap_insert_optlist(&optlist,coap_new_optlist(COAP_OPTION_URI_PATH,strlen(Topic),(uint8_t*)Topic));

        request = coap_new_pdu(session);
        if (!request) {
            ESP_LOGE(TAG, "coap_new_pdu() failed");
            return;
        }
        request->type = COAP_MESSAGE_CON;
        request->tid = coap_new_message_id(session);
        request->code = COAP_REQUEST_PUT;

        coap_add_optlist_pdu(request, &optlist);
        
        coap_add_data(request, strlen(Message),(uint8_t*)Message);

        coap_send(session, request);
        Esperando_datos =1;
    }

}

static void coap_client(void *p){
    ChargingGroup.Conected = true;
    struct hostent *hp;
    coap_address_t    dst_addr, src_addr;
    static coap_uri_t uri;
    char server_uri[100];
    char *phostname = NULL;
    TickType_t xTimer = xTaskGetTickCount();

    sprintf(server_uri, "coaps://%s", ChargingGroup.MasterIP.toString().c_str());

    coap_set_log_level(LOG_ERR);

    while (1) {
        session = NULL;
        ctx     = NULL;
        optlist = NULL;

        if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1) {
            ESP_LOGE(TAG, "CoAP server uri error");
            break;
        }

        if (uri.scheme == COAP_URI_SCHEME_COAPS && !coap_dtls_is_supported()) {
            ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");
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

        coap_address_init(&dst_addr);
        dst_addr.addr.sin.sin_family      = AF_INET;
        dst_addr.addr.sin.sin_port        = htons(uri.port);
        memcpy(&dst_addr.addr.sin.sin_addr, hp->h_addr, sizeof(dst_addr.addr.sin.sin_addr));

        coap_address_init(&src_addr);
        src_addr.addr.sin.sin_family      = AF_INET;
        inet_addr_from_ip4addr(&src_addr.addr.sin.sin_addr, &Coms.ETH.IP);
        src_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);

        ctx = coap_new_context(NULL);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed");
            goto clean_up;
        }
        coap_register_response_handler(ctx, message_handler);

        //Autenticarnos mediante DTLS
        if (uri.scheme == COAP_URI_SCHEME_COAPS && coap_dtls_is_supported()){
            do{
                session = coap_new_client_session_psk(ctx, &src_addr, &dst_addr,COAP_PROTO_DTLS , ConfigFirebase.Device_Id, (const uint8_t *)get_passwd().c_str(), 8);
            }while( !Authenticate());
            delay(1000);
        }

        //Subscribirnos
        Subscribe("PARAMS");
        Subscribe("CHARGERS");
        Subscribe("DATA");
        Subscribe("CONTROL");
        Subscribe("TXANDA");

        //Tras autenticarnos solicitamos los cargadores del grupo y los parametros
        coap_get("CHARGERS");

        coap_get("PARAMS");
        
        //Bucle del grupo
        xTimer = xTaskGetTickCount();
        while(1){
            coap_run_once(ctx, 1000);

            
            //Enviar nuevos parametros para el grupo
            if( ChargingGroup.SendNewParams){
                Send_Params();
                ChargingGroup.SendNewParams = false;
            }

            //Enviar los cargadores de nuestro grupo
            else if(ChargingGroup.SendNewGroup){
                Send_Chargers();
                ChargingGroup.SendNewGroup = false;
            }
            
            if(FallosEnvio > 5){
                printf("Servidor desconectado !\n");
                FallosEnvio=0;
                xTaskCreate(MasterPanicTask, "Master Panic", 4096, NULL,2,NULL);
                break;
            }
            else if(ChargingGroup.StopOrder){
                char buffer[20];
                ChargingGroup.StopOrder = false;
                memcpy(buffer,"Pause",6);
                coap_put("CONTROL", buffer);
                delay(250);
                break;
            }

            else if(ChargingGroup.DeleteOrder){
                char buffer[20];
                ChargingGroup.DeleteOrder = false;
                memcpy(buffer,"Delete",6);
                coap_put("CONTROL", buffer);
                delay(250);
                break;
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
        break;
    }
    printf("Cerrando cliente coap!\n");
    ChargingGroup.Conected    = false;
    ChargingGroup.StartClient = false;
    xClientHandle = NULL;
    vTaskDelete(xClientHandle);
}

static void coap_server(void *p){
    ChargingGroup.Conected = true;
    ctx = NULL;
    coap_address_t serv_addr;

    coap_set_log_level(LOG_DEBUG);

    DATA = NULL;
    PARAMS  = NULL;
    CONTROL  = NULL;
    CHARGERS  = NULL; 

    while (1) {
        coap_endpoint_t *ep = NULL;

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin.sin_family      = AF_INET;
        inet_addr_from_ip4addr(&serv_addr.addr.sin.sin_addr, &Coms.ETH.IP);
        serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);

        ctx = coap_new_context(NULL);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed");
            continue;
        }

        /* Need PSK setup before we set up endpoints */     
        coap_context_set_psk(ctx, "CoAP",(const uint8_t *)get_passwd().c_str(),8);
        //ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_UDP);

        if (coap_dtls_is_supported()) {
            serv_addr.addr.sin.sin_port        = htons(COAPS_DEFAULT_PORT);
            ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_DTLS);
            if (!ep) {
                ESP_LOGE(TAG, "dtls: coap_new_endpoint() failed");
                goto clean_up;
            }
        }
    
        //Añadir los topics
        coap_str_const_t Data_Name;
        Data_Name.length = sizeof("DATA")-1;
        Data_Name.s = reinterpret_cast<const uint8_t *>("DATA");

        coap_str_const_t Params_Name;
        Params_Name.length = sizeof("PARAMS")-1;
        Params_Name.s = reinterpret_cast<const uint8_t *>("PARAMS");

        coap_str_const_t Control_Name;
        Control_Name.length = sizeof("CONTROL")-1;
        Control_Name.s = reinterpret_cast<const uint8_t *>("CONTROL");

        coap_str_const_t Chargers_Name;
        Chargers_Name.length = sizeof("CHARGERS")-1;
        Chargers_Name.s = reinterpret_cast<const uint8_t *>("CHARGERS");

        DATA     = coap_resource_init(&Data_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);
        PARAMS   = coap_resource_init(&Params_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);
        CONTROL  = coap_resource_init(&Control_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);
        CHARGERS = coap_resource_init(&Chargers_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);

        if (!DATA || !PARAMS || !CONTROL || !CHARGERS) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }


        //coap_register_handler(DATA, COAP_REQUEST_GET, hnd_espressif_get);
        coap_register_handler(DATA, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(PARAMS, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(CONTROL, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(CHARGERS, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(TXANDA, COAP_REQUEST_PUT, hnd_espressif_put);

        coap_register_handler(PARAMS, COAP_REQUEST_GET, hnd_get);
        coap_register_handler(CHARGERS, COAP_REQUEST_GET, hnd_get);
        coap_register_handler(CONTROL, COAP_REQUEST_GET, hnd_get);
        coap_register_handler(DATA, COAP_REQUEST_GET, hnd_get);
        coap_register_handler(TXANDA, COAP_REQUEST_GET, hnd_get);


        /* We possibly want to Observe the GETs */
        coap_resource_set_get_observable(DATA, 1);
        coap_resource_set_get_observable(PARAMS, 1);
        coap_resource_set_get_observable(CONTROL, 1);
        coap_resource_set_get_observable(CHARGERS, 1);
        coap_resource_set_get_observable(TXANDA, 1);

        coap_add_resource(ctx, DATA);
        coap_add_resource(ctx, PARAMS);
        coap_add_resource(ctx, CONTROL);
        coap_add_resource(ctx, CHARGERS);
        coap_add_resource(ctx, TXANDA);

        int wait_ms = 1 * 1000;
        TickType_t xStart = xTaskGetTickCount();
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
                wait_ms = 1 * 1000;
                printf("%i\n", esp_get_free_internal_heap_size());
            }

            //Enviar nuevos parametros para el grupo
            if( ChargingGroup.SendNewParams){
                Send_Params();
                ChargingGroup.SendNewParams = false;
            }

            //Enviar los cargadores de nuestro grupo
            else if(ChargingGroup.SendNewGroup){
                Send_Chargers();
                ChargingGroup.SendNewGroup = false;
            }

            else if(ChargingGroup.StopOrder){
                char buffer[20];
                ChargingGroup.StopOrder = false;
                memcpy(buffer,"Pause",6);
                coap_put("CONTROL", buffer);
                delay(250);
                break;
            }

            else if(ChargingGroup.DeleteOrder){
                char buffer[20];
                ChargingGroup.DeleteOrder = false;
                memcpy(buffer,"Delete",6);
                coap_put("CONTROL", buffer);
                delay(250);
                break;
            }

            //Comprobar si los esclavos se desconectan cada 15 segundos
            TickType_t Transcurrido = xTaskGetTickCount() - xStart;
            if(Transcurrido > 15000){
                xStart = xTaskGetTickCount();
                for(uint8_t i=0 ;i<ChargingGroup.group_chargers.size;i++){
                    ChargingGroup.group_chargers.charger_table[i].Period += Transcurrido;
                    //si un equipo lleva mucho sin contestar, lo intentamos despertar
                    if(ChargingGroup.group_chargers.charger_table[i].Period >=30000 && ChargingGroup.group_chargers.charger_table[i].Period <= 60000){ 
                        send_to(ChargingGroup.group_chargers.charger_table[i].IP, "Start client");
                    }
                    
                    //si un equipo lleva muchisimo sin contestar, lo damos por muerto y lo eliminamos de la tabla
                    if(ChargingGroup.group_chargers.charger_table[i].Period >=60000 && ChargingGroup.group_chargers.charger_table[i].Period <= 65000){
                        if(memcmp(ChargingGroup.group_chargers.charger_table[i].name, ConfigFirebase.Device_Id,8)){
                            remove_from_group(ChargingGroup.group_chargers.charger_table[i].name,&net_group);
                            if(ChargingGroup.group_chargers.charger_table[i].Fase == Params.Fase){
                            remove_from_group(ChargingGroup.group_chargers.charger_table[i].name,&FaseChargers);
                            }
                        }
                    }
                }
            }
        }
    }
clean_up:
    coap_free_context(ctx);
    coap_cleanup();

    printf("Cerrando servidor coap!\n");
    ChargingGroup.Conected    = false;
    ChargingGroup.StartClient = false;
    xServerHandle = NULL;
    vTaskDelete(NULL);
}


/*****************************************************************************
 *                              HELPERS
*****************************************************************************/
//Enviar mis datos de carga
void Send_Data(){
  
  cJSON *Datos_Json;
  Datos_Json = cJSON_CreateObject();

  cJSON_AddStringToObject(Datos_Json, "device_id", ConfigFirebase.Device_Id);
  cJSON_AddNumberToObject(Datos_Json, "fase", Params.Fase);
  cJSON_AddNumberToObject(Datos_Json, "current", Status.Measures.instant_current);

  //si es trifasico, enviar informacion de todas las fases
  if(Status.Trifasico){
      cJSON_AddNumberToObject(Datos_Json, "currentB", Status.MeasuresB.instant_current);
      cJSON_AddNumberToObject(Datos_Json, "currentC", Status.MeasuresC.instant_current);
  }
  cJSON_AddNumberToObject(Datos_Json, "Delta", Status.Delta);
  cJSON_AddStringToObject(Datos_Json, "HPT", Status.HPT_status);
  cJSON_AddNumberToObject(Datos_Json, "limite_fase",Status.limite_Fase);


  char *my_json_string = cJSON_Print(Datos_Json);   
  
  cJSON_Delete(Datos_Json);

  coap_put("DATA", my_json_string);

  free(my_json_string);
  ChargingGroup.SendNewData = false;     
}

//enviar mi grupo de cargadores
void Send_Chargers(){
  char buffer[500];
  printf("Sending chargers!\n");
  if(ChargingGroup.group_chargers.size< 10){
      sprintf(buffer,"0%i",(char)ChargingGroup.group_chargers.size);
  }
  else{
      sprintf(buffer,"%i",(char)ChargingGroup.group_chargers.size);
  }
  
  for(uint8_t i=0;i< ChargingGroup.group_chargers.size;i++){
      memcpy(&buffer[2+(i*9)],ChargingGroup.group_chargers.charger_table[i].name,8);   
      itoa(ChargingGroup.group_chargers.charger_table[i].Fase,&buffer[10+(i*9)],10);
  }
  coap_put("CHARGERS", buffer);
}

//Enviar parametros
void Send_Params(){ 
  printf("Sending params!\n");
  cJSON *Params_Json;
  Params_Json = cJSON_CreateObject();

  cJSON_AddNumberToObject(Params_Json, "cdp", ChargingGroup.Params.CDP);
  cJSON_AddNumberToObject(Params_Json, "contract", ChargingGroup.Params.ContractPower);
  cJSON_AddNumberToObject(Params_Json, "active", ChargingGroup.Params.GroupActive);
  cJSON_AddNumberToObject(Params_Json, "master", ChargingGroup.Params.GroupMaster);
  cJSON_AddNumberToObject(Params_Json, "inst_max", ChargingGroup.Params.inst_max);
  cJSON_AddNumberToObject(Params_Json, "pot_max", ChargingGroup.Params.potencia_max);
  cJSON_AddNumberToObject(Params_Json, "userID", ChargingGroup.Params.UserID);

  char *my_json_string = cJSON_Print(Params_Json);   
  cJSON_Delete(Params_Json); 
  
  coap_put("PARAMS", my_json_string);

  free(my_json_string);
     
}

//Obtener la password para el grupo
String get_passwd(){
    char* pass = (char*) calloc(8, sizeof(char));

    uint32_t value =0;
    for(uint8_t i=0;i< 4;i++){
        value+=ChargingGroup.MasterIP[i];
    }

    randomSeed(value);
    itoa((int)random(12345678, 99999999), pass,8);

    String password = Encipher(String(pass));

    return password;
}

/*Tarea de emergencia para cuando el maestro se queda muerto mucho tiempo*/
void MasterPanicTask(void *args){
    TickType_t xStart = xTaskGetTickCount();
    uint8 reintentos = 1;
    ChargingGroup.Params.GroupActive = false;
    int delai = 30000;
    Serial.println("Necesitamos un nuevo maestro!");
    while(!ChargingGroup.Conected){
        if(pdTICKS_TO_MS(xTaskGetTickCount() - xStart) > delai){ //si pasan 30 segundos, elegir un nuevo maestro
            delai=10000;           

            if(!memcmp(ChargingGroup.group_chargers.charger_table[reintentos].name,ConfigFirebase.Device_Id,8)){
                ChargingGroup.Params.GroupActive = true;
                ChargingGroup.Params.GroupMaster = true;
                printf("Soy el nuevo maestro!\n");
                break;
            }
            else{
                xStart = xTaskGetTickCount();
                reintentos++;
                if(reintentos == ChargingGroup.group_chargers.size){
                    //Ultima opcion, mirar si yo era el maestro
                    if(!memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id,8)){
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.Params.GroupMaster = true;
                        printf("Soy el nuevo maestro2!\n");
                    }
                    else{
                      printf("Ya he mirado todos los equipos y no estoy!\n");
                    }
                    
                    break;
                }
            }
        }
        if(!Coms.ETH.conectado){
            break;
        }
        delay(delai);
    }
    vTaskDelete(NULL);
}

void start_server(){
    if(xServerHandle == NULL){
        xServerHandle = xTaskCreateStatic(coap_server, "coap_server", 4096*4, NULL, 5, xServerStack, &xServerBuffer);
    }
    
}

void start_client(){
    if(xClientHandle == NULL){
        xClientHandle = xTaskCreateStatic(coap_client, "coap_client", 4096*4, NULL, 5, xClientStack,&xClientBuffer);
    }
}

/*******************************************************
 * Funciones para llamar desde otros puntos del codigo
 * ****************************************************/
void coap_start_server(){

  ChargingGroup.Conected = true;
  if(ChargingGroup.Params.GroupMaster){

    printf("Arrancando servidor COAP\n");

    //Preguntar si hay maestro en el grupo
    for(uint8_t i =0; i<10;i++){
        broadcast_a_grupo("Hay maestro?", 12);
        delay(5);
        if(!ChargingGroup.Params.GroupMaster)break;
    }
    if(ChargingGroup.Params.GroupMaster){
        broadcast_a_grupo("Start client", 12);

        //Ponerme el primero en el grupo para indicar que soy el maestro
        if(ChargingGroup.group_chargers.size>0 && check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) != 255){
            while(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                carac_charger OldMaster=ChargingGroup.group_chargers.charger_table[0];
                remove_from_group(OldMaster.name, &ChargingGroup.group_chargers);
                add_to_group(OldMaster.name, OldMaster.IP, &ChargingGroup.group_chargers);
                ChargingGroup.group_chargers.charger_table[ChargingGroup.group_chargers.size-1].Fase=OldMaster.Fase;
            }
            store_group_in_mem(&ChargingGroup.group_chargers);
        }
        else{
            //Si el grupo está vacio o el cargador no está en el grupo,
            printf("No tengo cargadores en el grupo o no estoy en mi propio grupo!\n");
            ChargingGroup.Params.GroupMaster = false;
            ChargingGroup.Params.GroupActive = false;
            ChargingGroup.Conected = false;
            return;
            
        }
        
        ChargingGroup.MasterIP.fromString(String(ip4addr_ntoa(&Coms.ETH.IP)));
        start_server();
    }
  }

}

void coap_start_client(){
  start_client();
}
