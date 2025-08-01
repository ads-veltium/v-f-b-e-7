#include "../control.h"
#if (defined CONNECTED && defined CONNECTED)
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include "../group_control.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "nvs_flash.h"


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
extern carac_charger charger_table[MAX_GROUP_SIZE];
extern carac_Comands  Comands ;
extern carac_circuito Circuitos[MAX_GROUP_SIZE];
extern carac_Contador ContadorExt;

extern uint8 Bloqueo_de_carga;

static StackType_t xServerStack [4096*4]     EXT_RAM_ATTR;
static StackType_t xLimitStack [4096*4]     EXT_RAM_ATTR;
static StackType_t xClientStack [4096*4]     EXT_RAM_ATTR;
StaticTask_t xServerBuffer;
StaticTask_t xClientBuffer;
StaticTask_t xLimitBuffer;

TaskHandle_t xServerHandle = NULL;
TaskHandle_t xClientHandle = NULL;
TaskHandle_t xLimitHandle = NULL;

TickType_t   xMasterTimer  = 0;

coap_context_t  *ctx      EXT_RAM_ATTR;
coap_session_t  *session  EXT_RAM_ATTR;
coap_optlist_t  *optlist  EXT_RAM_ATTR;

coap_resource_t *DATA     EXT_RAM_ATTR;
coap_resource_t *PARAMS   EXT_RAM_ATTR;
coap_resource_t *CONTROL  EXT_RAM_ATTR;
coap_resource_t *CHARGERS EXT_RAM_ATTR; 
coap_resource_t *TURN       EXT_RAM_ATTR; 
coap_resource_t *CIRCUITS   EXT_RAM_ATTR; 

const static char *TAG = "CoAP";
uint8_t Esperando_datos =0;
uint8_t FallosEnvio =0;
uint8_t turno =0;
char LastControl[50] EXT_RAM_ATTR;
char LastData[100]   EXT_RAM_ATTR;
static char FullData[4096] EXT_RAM_ATTR;
bool hello_verification = false;
uint16_t corrienteDeseada = 600;

extern bool PARAR_CLIENTE;



/*****************************
 * Funciones externas 
 * **************************/
IPAddress get_IP(const char* ID);
String Encipher(String input);
uint8_t check_in_group(const char* ID, carac_charger* group, uint8_t size);

bool add_to_group(const char* ID, IPAddress IP, carac_charger* group, uint8_t *size);
bool remove_from_group(const char* ID ,carac_charger* group, uint8_t *size);

void store_group_in_mem(carac_charger* group, uint8_t size);
void store_params_in_mem();
void broadcast_a_grupo(char* Mensaje, uint16_t size);
void send_to(IPAddress IP,  char* Mensaje);

/*****************************
 * Prototipos de funciones 
 * **************************/
void Send_Params();
void Send_Data();
void Send_Chargers();
void Send_Circuits();

String get_passwd();
void coap_put( char* Topic, char* Message);
void coap_get( char* Topic);
void MasterPanicTask(void *args);
void coap_start_client();

static void
hnd_get(coap_context_t *ctx, coap_resource_t *resource,coap_session_t *session,coap_pdu_t *request, coap_binary_t *token,coap_string_t *query, coap_pdu_t *response){
    char buffer[500];
#ifdef DEBUG_COAP
    Serial.printf("COAP_Nativo - hnd_get: %s\n", resource->uri_path->s);
#endif
    if(!memcmp(resource->uri_path->s, "CHARGERS", resource->uri_path->length)){
        itoa(GROUP_CHARGERS, buffer, 10);
        
        if(ChargingGroup.Charger_number< 10){
            sprintf(&buffer[1],"0%i",(char)ChargingGroup.Charger_number);
        }
        else{
            sprintf(&buffer[1],"%i",(char)ChargingGroup.Charger_number);
        }
        
        for(uint8_t i=0;i< ChargingGroup.Charger_number;i++){
            memcpy(&buffer[3+(i*11)],charger_table[i].name,8);  

            uint8_t value = (charger_table[i].Circuito << 2) + charger_table[i].Fase;

            if(value< 10){
                sprintf(&buffer[11+(i*11)],"00%i",(char)value);
            }
            else if(value< 100){
                sprintf(&buffer[11+(i*11)],"0%i",(char)value);
            }
            else{
                sprintf(&buffer[11+(i*11)],"%i",(char)value);
            }
        }
    }
    else if(!memcmp(resource->uri_path->s, "CIRCUITS", resource->uri_path->length)){
        
        itoa(GROUP_CIRCUITS, buffer, 10);

        if(ChargingGroup.Circuit_number< 10){
            sprintf(&buffer[1],"0%i",(char)ChargingGroup.Circuit_number);
        }
        else{
            sprintf(&buffer[1],"%i",(char)ChargingGroup.Circuit_number);
        }

        for(uint8_t i=0;i< ChargingGroup.Circuit_number;i++){ 
            buffer[i+3] = (char)Circuitos[i].limite_corriente;
        }

    }
    else if(!memcmp(resource->uri_path->s, "PARAMS", resource->uri_path->length)){
        
        itoa(GROUP_PARAMS, buffer, 10);
        cJSON *Params_Json;
        Params_Json = cJSON_CreateObject();

        cJSON_AddNumberToObject(Params_Json, "cdp", ChargingGroup.Params.CDP);
        cJSON_AddNumberToObject(Params_Json, "contract", ChargingGroup.Params.ContractPower);
        cJSON_AddNumberToObject(Params_Json, "active", ChargingGroup.Params.GroupActive ? 1 : 0);
        cJSON_AddNumberToObject(Params_Json, "master", ChargingGroup.Params.GroupMaster ? 1 : 0);
        cJSON_AddNumberToObject(Params_Json, "pot_max", ChargingGroup.Params.potencia_max);

        char *my_json_string = cJSON_Print(Params_Json);   
        cJSON_Delete(Params_Json); 

        int size = strlen(my_json_string);
        memcpy(&buffer[1], my_json_string, size);
        buffer[size+1]='\0';
        free(my_json_string);
    }
    else if(!memcmp(resource->uri_path->s, "CONTROL", resource->uri_path->length)){
        itoa(GROUP_CONTROL, buffer, 10);
        int size = strlen(LastControl);
        memcpy(&buffer[1], LastControl, size);
        buffer[size+1]='\0';
    }
    else if(!memcmp(resource->uri_path->s, "TURN", resource->uri_path->length)){
        sprintf(buffer,"%i%i",TURNO, turno);
    }
    else if(!memcmp(resource->uri_path->s, "DATA", resource->uri_path->length)){
        itoa(CURRENT_COMMAND, buffer, 10);
        int size = strlen(LastData);
        memcpy(&buffer[1], LastData, size);
        buffer[size+1]='\0';   
    }

    response->code = COAP_RESPONSE_CODE(205);
    if(strlen((char*)buffer) >0){
        coap_add_data_blocked_response(resource, session, request, response, token,COAP_MEDIATYPE_APPLICATION_JSON, 2,(size_t)strlen((char*)buffer),(const u_char*)buffer);
    }

}

static void 
message_handler(coap_context_t *ctx, coap_session_t *session,coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id){
    uint8_t *data = NULL;
    size_t data_len;
    coap_pdu_t *pdu = NULL;
    coap_opt_t *block_opt;
    coap_opt_iterator_t opt_iter;
    unsigned char buf[4];
    coap_optlist_t *option;
    coap_tid_t tid;
    static int FullSize =0;
    xMasterTimer = xTaskGetTickCount();

    if (COAP_RESPONSE_CLASS(received->code) == 2) {
        /* Need to see if blocked response */
        block_opt = coap_check_option(received, COAP_OPTION_BLOCK2, &opt_iter);
        if (block_opt) {
            uint16_t blktype = opt_iter.type;

            if (coap_opt_block_num(block_opt) == 0) {
                FullSize = 0;
            }
            if (coap_get_data(received, &data_len, &data)) {
                memcpy(&FullData[FullSize], data, data_len);
                FullSize += data_len;
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
                        coap_add_option(pdu, option->number, option->length,option->data);
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
            else{
                Serial.printf("Datos 2 %.*s\n", (int)FullSize, FullData);
                //Parse_Data_JSON(FullData, (int)FullSize);
            }
        }
        else{
            if (coap_get_data(received, &data_len, &data)){
                if (data != NULL){
#ifdef DEBUG_COAP
                    Serial.printf("COAP_Nativo - message_handler: data = %i, %i\n", data[0] - '0', data[1]);
#endif
                    switch (data[0] - '0'){
                    case GROUP_PARAMS:
#ifdef DEBUG_COAP
                        Serial.printf("COAP_Nativo - message_handler: GROUP_PARAMS\n");
#endif
                        New_Params(&data[1], data_len - 1);
                        break;
                    case GROUP_CONTROL:
#ifdef DEBUG_COAP
                        Serial.printf("COAP_Nativo - message_handler: GROUP_CONTROL\n");
#endif
                        New_Control(&data[1], data_len - 1);
                        break;
                    case GROUP_CHARGERS:
#ifdef DEBUG_COAP
                        Serial.printf("COAP_Nativo - message_handler: GROUP_CHARGERS\n");
#endif
                        New_Group(&data[1], data_len - 1);
                        break;
                    case TURNO:
#ifdef DEBUG_COAP
                        Serial.printf("COAP_Nativo - message_handler: TURNO\n");
#endif
                        char turno[2];
                        memcpy(turno, &data[1], 2);
                        if (!memcmp(charger_table[atoi(turno)].name, ConfigFirebase.Device_Id, 8)){
                            Send_Data();
                        }
                        break;
                    case CURRENT_COMMAND:
#ifdef DEBUG_COAP
                        Serial.printf("COAP_Nativo - message_handler: CURRENT_COMMAND\n");
#endif
                        New_Current(&data[1], data_len - 1);
                        break;

                    case GROUP_CIRCUITS:
#ifdef DEBUG_COAP
                        Serial.printf("COAP_Nativo - message_handler: GROUP_CIRCUITS\n");
#endif
                        New_Circuit(&data[1], data_len - 1);
                        break;

                    default:
#ifdef DEBUG_COAP
                        Serial.printf("COAP_Nativo - message_handler: default . ¿? %s\n", data);
#endif
                        break;
                    }
                }
            }
        }
    }  
    else{
        ESP_LOGE(TAG, "codigo de coap %i \n", COAP_RESPONSE_CLASS(received->code));
    } 
    Esperando_datos = 0;
}

//Handler para las request a los puts que puedan hacer los esclavos
static void hnd_espressif_put(coap_context_t *ctx,coap_resource_t *resource,coap_session_t *session,coap_pdu_t *request,coap_binary_t *token,coap_string_t *query, coap_pdu_t *response){
    size_t size;
    unsigned char *data;

    (void)coap_get_data(request, &size, &data);
#ifdef DEBUG_COAP
    Serial.printf("COAP_Nativo - hnd_espressif_put: resource->uri_path->s = %s \n", resource->uri_path->s);
#endif
    if(!memcmp(resource->uri_path->s, "CHARGERS", resource->uri_path->length)){
        Serial.printf("New group1 \n");
        New_Group(data,  size);
        delay(100);
        coap_resource_notify_observers(resource, NULL);
    }
    else if(!memcmp(resource->uri_path->s, "PARAMS", resource->uri_path->length)){
        New_Params(data, size);
        delay(100);
        coap_resource_notify_observers(resource, NULL);        
    }
    else if(!memcmp(resource->uri_path->s, "CONTROL", resource->uri_path->length)){

        if(size <=0){
            return;
        }

        char* Data = (char*) calloc(size, '0');
        memcpy(Data, data, size);

        if(strstr(Data,"Delete")){
            memcpy(LastControl, "Delete", 6);
            coap_resource_notify_observers(CONTROL, NULL);
            delay(1000);
            New_Control(LastControl,  6);       
        }
        else if(strstr(Data,"Pause")){
            memcpy(LastControl, "Pause", 5);
            coap_resource_notify_observers(CONTROL, NULL); 
            delay(1000);
            New_Control(LastControl,  5);      
        }
        free(Data);             
    }
    else if(!memcmp(resource->uri_path->s, "CIRCUITS", resource->uri_path->length)){
        New_Circuit((uint8_t*)data,  size);
        coap_resource_notify_observers(resource, NULL);
    }
    else if(!memcmp(resource->uri_path->s, "DATA", resource->uri_path->length)){
        char buffer[50];
        carac_charger Cargador = New_Data(data,  size);
         
        itoa(CURRENT_COMMAND, buffer, 10);
        cJSON *COMMAND_Json;
        COMMAND_Json = cJSON_CreateObject();
        cJSON_AddStringToObject(COMMAND_Json, "N", Cargador.name);
        cJSON_AddNumberToObject(COMMAND_Json, "DC", Cargador.Consigna);
        
        if(Cargador.Consigna > 0 && memcmp(Cargador.HPT, "C1",2)){
            if(!memcmp(Cargador.HPT, "B1",2) && ! Cargador.ChargeReq){
                cJSON_AddNumberToObject(COMMAND_Json, "P", 0);
            }
            else{
                cJSON_AddNumberToObject(COMMAND_Json, "P", 1);
            }
        }
        else{
            cJSON_AddNumberToObject(COMMAND_Json, "P", 0);
        }
        char *my_json_string = cJSON_Print(COMMAND_Json);   
        cJSON_Delete(COMMAND_Json); 
        int size = strlen(my_json_string);
        memcpy(&buffer[1], my_json_string, size);
        memcpy(LastData, my_json_string, size);
        buffer[size+1]='\0';
#ifdef DEBUG_COAP
        Serial.printf("COAP_Nativo - hnd_espressif_put: JSON Enviado:%s\n",my_json_string);
#endif   
        free(my_json_string);
        coap_add_data_blocked_response(resource, session, request, response, token,COAP_MEDIATYPE_APPLICATION_JSON, 2,(size_t)strlen((char*)buffer),(const u_char*)buffer);
    }
    response->code = COAP_RESPONSE_CODE(205);
}

static bool POLL(int timeout){
    while (Esperando_datos) {
        int result = coap_run_once(ctx, timeout);
        if (result >= 0) {
            if (result >= timeout) {
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
#ifdef DEBUG_GROUPS
    Serial.printf("COAP_Nativo - Authenticate: Autenticando contra el servidor\n");
    ChargingGroup.DeleteOrder = false;
    ChargingGroup.StopOrder = false;
#endif

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
    POLL(2000); //ADS - CAMBIADO DE 20.000 A 2.000
    if(Esperando_datos){
         if (session) {
            coap_session_release(session);
        }
        return false;
    }

#ifdef DEBUG_GROUPS
    Serial.printf("COAP_Nativo - Authenticate: Autenticados!!!\n");
#endif

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
    ChargingGroup.Putting = true;
    if(ChargingGroup.Params.GroupMaster){
        if(!memcmp("DATA", Topic, 4)){    
            carac_charger Cargador = New_Data(Message,  strlen(Message));
            
            ChargingGroup.ChargPerm = Cargador.Consigna > 0;
            uint8_t bloqueo_carga = 0;
            if(ChargingGroup.AskPerm && ChargingGroup.ChargPerm){
                ChargingGroup.AskPerm = false;
                if(ContadorConfigurado()){
                    if(!ContadorExt.MedidorConectado){
                        ChargingGroup.AskPerm = true;
                        ChargingGroup.ChargPerm = false;
                        bloqueo_carga =1;
                    }
                }
                SendToPSOC5(bloqueo_carga, BLOQUEO_CARGA);
#ifdef DEBUG_COAP
                Serial.printf("coap_put: Enviado al PSoC bloqueo_carga = %i\n", bloqueo_carga);
#endif
            }            
            
            //TODOJ: Cambiar lo de corriente d
            if((uint8_t)Cargador.Consigna != Comands.desired_current && Cargador.Consigna!=0){
                //corrienteDeseada = Cargador.Consigna * 100;
#ifdef DEBUG_COAP
                Serial.printf("Enviando nueva consigna! %i %i\n", Cargador.Consigna, Comands.desired_current);
#endif
                SendToPSOC5((uint8_t)Cargador.Consigna, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
#ifdef DEBUG_COAP
                Serial.printf("Enviado al PSoC Consigna = %i\n", Cargador.Consigna);
#endif
            }

        }
        else if(!memcmp("PARAMS", Topic, 6)){
            New_Params(Message,  strlen(Message));
            coap_resource_notify_observers(PARAMS, NULL);
        }
        else if(!memcmp("CONTROL", Topic, 7)){
            memcpy(LastControl, Message, strlen(Message));
            coap_resource_notify_observers(CONTROL, NULL);
            delay(1000);
            New_Control(Message,  strlen(Message));
            
        }
        else if(!memcmp("CHARGERS", Topic, 8)){
            New_Group(Message,  strlen(Message));
            coap_resource_notify_observers(CHARGERS, NULL);
        }
        else if(!memcmp("CIRCUITS", Topic, 8)){
            New_Circuit((uint8_t*) Message,  strlen(Message));
            coap_resource_notify_observers(CIRCUITS, NULL);
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
    ChargingGroup.Putting = false;
}

static void coap_client(void *p){
    ChargingGroup.Conected = true;
    ChargingGroup.StopOrder = false;
    struct hostent *hp;
    coap_address_t    dst_addr, src_addr;
    static coap_uri_t uri;
    char server_uri[100];
    char *phostname = NULL;

    sprintf(server_uri, "coaps://%s", ChargingGroup.MasterIP.toString().c_str());

#ifdef DEBUG_GROUPS
    coap_set_log_level(LOG_ERR);
    Serial.printf("COAP_Nativo - coap_client: Arrancando cliente coaps://%s\n",ChargingGroup.MasterIP.toString().c_str());
#else
    coap_set_log_level(LOG_EMERG);
#endif
    ChargingGroup.Params.GroupMaster = false;
    while (1) {
        session = NULL;
        ctx     = NULL;
        optlist = NULL;

        if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1) {
            ESP_LOGE(TAG, "CoAP server uri error");
            Serial.printf("ERRROR  1");
            break;
        }

        if (uri.scheme == COAP_URI_SCHEME_COAPS && !coap_dtls_is_supported()) {
            ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");
            Serial.printf("ERRROR  2");
            break;
        }

        phostname = (char *)calloc(1, uri.host.length + 1);
        if (phostname == NULL) {
            ESP_LOGE(TAG, "calloc failed");
            Serial.printf("ERRROR  3");
            break;
        }

        memcpy(phostname, uri.host.s, uri.host.length);
        hp = gethostbyname(phostname);
        free(phostname);

        if (hp == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed");
            Serial.printf("ERRROR  4");
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

        ctx = coap_new_context(&src_addr);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed"); 
            Serial.printf("ERRROR  5");
            goto client_clean_up;
        }
        coap_register_response_handler(ctx, message_handler);
        //Autenticarnos mediante DTLS
        if (uri.scheme == COAP_URI_SCHEME_COAPS && coap_dtls_is_supported()){
            uint8_t intentos = 0;
            do{
                session = coap_new_client_session_psk(ctx, &src_addr, &dst_addr,COAP_PROTO_DTLS , ConfigFirebase.Device_Id, (const uint8_t *)get_passwd().c_str(), 8);
            }while( !Authenticate() && ++intentos < 4);
            if(intentos >= 4){
#ifdef DEBUG_GROUPS
                Serial.printf("COAP_Nativo - coap_client: Authenticate() fallido\n");
#endif
                break;
            }
        }

        //Subscribirnos
        Subscribe("PARAMS");
        Subscribe("CHARGERS");
        Subscribe("TURN");
        Subscribe("CONTROL");
        Subscribe("CIRCUITS");

        //Tras autenticarnos solicitamos los cargadores del grupo y los parametros
        /*coap_get("CHARGERS");
        POLL(100);
        coap_get("PARAMS");
        POLL(100);
        coap_get("CIRCUITS");
        POLL(100);*/
        
        //Bucle del grupo
        xMasterTimer = xTaskGetTickCount();
        while(1){
            coap_run_once(ctx, 100);
            
            //Enviar nuevos parametros para el grupo
            if( ChargingGroup.SendNewParams){
                Send_Params();
                ChargingGroup.SendNewParams = false;
                if(!ChargingGroup.Params.GroupActive){
                    ChargingGroup.StopOrder = true;
                }
            }

            //Enviar los cargadores de nuestro grupo
            else if(ChargingGroup.SendNewGroup){
                Send_Chargers();
                ChargingGroup.SendNewGroup = false;
            }

            //Enviar los circuitos de nuestro grupo
            else if(ChargingGroup.SendNewCircuits){
                Send_Circuits();
                ChargingGroup.SendNewCircuits = false;
            }
            
            if(pdTICKS_TO_MS(xTaskGetTickCount()- xMasterTimer) > 30000){  // Reducido el check del Servidor a 30 segundos, sin fallos de envío
            // if(FallosEnvio > 10 || pdTICKS_TO_MS(xTaskGetTickCount()- xMasterTimer) > 180000){
                xMasterTimer = xTaskGetTickCount();
#ifdef DEBUG_GROUPS
                Serial.printf("Servidor desconectado !\n");
#endif
                //Reinicio del cliente cuando el servidor no responde. También sucede si el servidor se ha reiniciado.
                if (PARAR_CLIENTE){
                    coap_start_client();
                    PARAR_CLIENTE = false;
                }
                else{
                    PARAR_CLIENTE = true;
                }
                // xTaskCreate(MasterPanicTask, "Master Panic", 4096, NULL, 2, NULL); // ADS Eliminado el Reseteo por tiempo del MAESTRO
                // break;
            }

            //Esto son ordenes internas, por lo que no las enviamos al resto
            else if(ChargingGroup.StopOrder){
                /*char buffer[20];
                ChargingGroup.StopOrder = false;
                memcpy(buffer,"Pause",5);
                coap_put("CONTROL", buffer);*/
                New_Control("Pause", 5);
                ChargingGroup.StopOrder = false;
#ifdef DEBUG_GROUPS
                Serial.printf("Parada de groupo. Salida por StopOrder\n");
#endif
                break;
            }

            else if(ChargingGroup.DeleteOrder){
                /*char buffer[20];
                memcpy(buffer,"Delete",6);
                coap_put("CONTROL", buffer);
                delay(250);*/
                New_Control("Delete", 6);
                ChargingGroup.DeleteOrder = false;
#ifdef DEBUG_GROUPS
                Serial.printf("Parada de groupo. Salida por DeleteOrder\n");
#endif
                break;
            }
            if (PARAR_CLIENTE){
                PARAR_CLIENTE = false;
#ifdef DEBUG_COAP
                Serial.printf("COAP_Nativo - PARAR_CLIENTE\n");
#endif
                goto client_clean_up;
            }
        }

client_clean_up:
#ifdef DEBUG_COAP
    Serial.printf("COAP_Nativo - coap_client: client_clean_up\n");
#endif
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
#ifdef DEBUG_GROUPS
    Serial.printf("Cerrando cliente coap!\n");
#endif
    ChargingGroup.Conected = false;
    ChargingGroup.StartClient = false;
    xClientHandle = NULL;
    vTaskDelete(NULL);
}

static void coap_server(void *p){
    ChargingGroup.NewData = false;
    ChargingGroup.Conected = true;
    ChargingGroup.StopOrder = false;
    ctx = NULL;
    coap_address_t serv_addr;

#ifdef DEBUG_GROUPS
    Serial.printf("Arrancando servidor COAP\n");
    coap_set_log_level(LOG_ERR);
#else
    coap_set_log_level(LOG_EMERG);
#endif

    DATA = NULL;
    PARAMS = NULL;
    CONTROL = NULL;
    CHARGERS = NULL;
    TURN    = NULL;
    CIRCUITS = NULL;

    SendToPSOC5(1, BLOQUEO_CARGA); //Bloquear la carga
#ifdef DEBUG_GROUPS
	Serial.printf("coap_server: Envío Bloqueo de carga = 1 al PSoC\n");
#endif

    memcpy(LastControl, "NOTHING",7);
    while (1) {
        coap_endpoint_t *ep = NULL;

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin.sin_family      = AF_INET;
        inet_addr_from_ip4addr(&serv_addr.addr.sin.sin_addr, &Coms.ETH.IP);
        serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);

        ctx = coap_new_context(&serv_addr);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed");
            continue;
        }

        /* Need PSK setup before we set up endpoints */     
        coap_context_set_psk(ctx, "CoAP",(const uint8_t *)get_passwd().c_str(),8);
        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_UDP);

        if (coap_dtls_is_supported()) {
            serv_addr.addr.sin.sin_port        = htons(COAPS_DEFAULT_PORT);
            ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_DTLS);
            if (!ep) {
                ESP_LOGE(TAG, "dtls: coap_new_endpoint() failed");
                goto server_clean_up;
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

        coap_str_const_t Turn_Name;
        Turn_Name.length = sizeof("TURN")-1;
        Turn_Name.s = reinterpret_cast<const uint8_t *>("TURN");

        coap_str_const_t Circuitos_Name;
        Circuitos_Name.length = sizeof("CIRCUITS")-1;
        Circuitos_Name.s = reinterpret_cast<const uint8_t *>("CIRCUITS");

        DATA = coap_resource_init(&Data_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);
        PARAMS = coap_resource_init(&Params_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);
        CONTROL = coap_resource_init(&Control_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);
        CHARGERS = coap_resource_init(&Chargers_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);
        TURN = coap_resource_init(&Turn_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);
        CIRCUITS = coap_resource_init(&Circuitos_Name, COAP_RESOURCE_FLAGS_RELEASE_URI);

        if (!DATA || !PARAMS || !CONTROL || !CHARGERS || !TURN || !CIRCUITS) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto server_clean_up;
        }

        // coap_register_handler(DATA, COAP_REQUEST_GET, hnd_espressif_get);
        coap_register_handler(DATA, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(PARAMS, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(CONTROL, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(CHARGERS, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(TURN, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(CIRCUITS, COAP_REQUEST_PUT, hnd_espressif_put);

        coap_register_handler(PARAMS, COAP_REQUEST_GET, hnd_get);
        coap_register_handler(CHARGERS, COAP_REQUEST_GET, hnd_get);
        coap_register_handler(CONTROL, COAP_REQUEST_GET, hnd_get);
        coap_register_handler(DATA, COAP_REQUEST_GET, hnd_get);
        coap_register_handler(TURN, COAP_REQUEST_GET, hnd_get);
        coap_register_handler(CIRCUITS, COAP_REQUEST_GET, hnd_get);

        /* We possibly want to Observe the GETs */
        coap_resource_set_get_observable(TURN, 1);
        coap_resource_set_get_observable(PARAMS, 1);
        coap_resource_set_get_observable(CONTROL, 1);
        coap_resource_set_get_observable(CHARGERS, 1);
        coap_resource_set_get_observable(CIRCUITS, 1);

        coap_add_resource(ctx, DATA);
        coap_add_resource(ctx, PARAMS);
        coap_add_resource(ctx, CONTROL);
        coap_add_resource(ctx, TURN);
        coap_add_resource(ctx, CHARGERS);
        coap_add_resource(ctx, CIRCUITS);

        int wait_ms = 250;
        TickType_t xStart = xTaskGetTickCount();
        TickType_t xTimerTurno = xTaskGetTickCount();
        while (1) {
            if(hello_verification){
                wait_ms = 10000;
            }
            int result = coap_run_once(ctx, wait_ms);
            
            if (result < 0) {
                break;
            } else if (result && (unsigned)result < wait_ms) {
                /* decrement if there is a result wait time returned */
                wait_ms -= result;
            }
            if (result) {
                /* result must have been >= wait_ms, so reset wait_ms */
                wait_ms = 250;
                //Serial.printf("%i\n", esp_get_free_internal_heap_size());
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
            //Enviar los circuitos de nuestro grupo
            else if(ChargingGroup.SendNewCircuits){
                Send_Circuits();
                ChargingGroup.SendNewCircuits = false;
            }

            //Esto son ordenes internas, por lo que no las enviamos al resto
            else if(ChargingGroup.StopOrder){
                /*char buffer[20];
                ChargingGroup.StopOrder = false;
                memcpy(buffer,"Pause",5);
                coap_put("CONTROL", buffer);*/
                New_Control("Pause", 5);
                ChargingGroup.StopOrder = false;
                goto server_clean_up;
                break;
            }

            else if(ChargingGroup.DeleteOrder){
                //char buffer[20];
				//memcpy(buffer,"Delete",6);
				/*coap_put("CONTROL", "Delete");
                delay(500);*/
                New_Control("Delete", 6);
                ChargingGroup.DeleteOrder = false;
                goto server_clean_up;
                break;
            }
            
            //Pedir datos a los esclavos para que no envíen todos a la vez
            if(pdTICKS_TO_MS(xTaskGetTickCount() - xTimerTurno) >= 250){
                turno++;        
                if(turno == ChargingGroup.Charger_number){
                    turno=1;
                    Send_Data(); //Mandar mis datos
                }
                xTimerTurno = xTaskGetTickCount();
                coap_resource_notify_observers(TURN,NULL);
            }


            //Comprobar si los esclavos se desconectan cada 15 segundos
            int Transcurrido = pdTICKS_TO_MS(xTaskGetTickCount() - xStart);
            if(Transcurrido > 15000){
                xStart = xTaskGetTickCount();
                for(uint8_t i=0 ;i<ChargingGroup.Charger_number;i++){
                    charger_table[i].Period += Transcurrido;
                    //si un equipo lleva mucho sin contestar, lo intentamos despertar
                    if(charger_table[i].Period >32000){
                        send_to(get_IP(charger_table[i].name), "Start client");
#ifdef DEBUG_UDP
                        Serial.printf("COAP_Nativo - coap_server: sent_to \"Start client\" to %s - %s por UDP\n",get_IP(charger_table[i].name).toString().c_str(), charger_table[i].name);
#endif
                    }
                    
                    //si un equipo lleva muchisimo sin contestar, lo damos por muerto y lo eponemos como inactivo
 /*                 // ADS - ELIMINADA LA DESACTIVACIÓN DE UN CLIENTE
                    if(charger_table[i].Period >=60000 && charger_table[i].Period <= 65000){
                        if(memcmp(charger_table[i].name, ConfigFirebase.Device_Id,8)){
                            memcpy(charger_table[i].HPT, "0V", 3);
                            charger_table[i].Current     = 0;
                            charger_table[i].CurrentB    = 0;
                            charger_table[i].CurrentC    = 0;
                            charger_table[i].Consigna    = 0;
                            charger_table[i].Delta       = 0;
                            charger_table[i].Delta_timer = 0;
                            charger_table[i].Conected    = 0;
                        }
                    }
*/
                }
            }
        }
    }
server_clean_up:
    if (ctx && ctx != NULL){
        coap_free_context(ctx);
    }
    coap_cleanup();
#ifdef DEBUG_GROUPS
    Serial.printf("Cerrando servidor coap!\n");
#endif
    ChargingGroup.Conected = false;
    ChargingGroup.StartClient = false;
    vTaskDelete(xLimitHandle);
    xLimitHandle = NULL;
    xServerHandle = NULL;
    vTaskDelete(NULL);
}

/*****************************************************************************
 *                              HELPERS
*****************************************************************************/
//Enviar mis datos de carga
void Send_Data()
{

    cJSON *Datos_Json;
    Datos_Json = cJSON_CreateObject();

    cJSON_AddStringToObject(Datos_Json, "device_id", ConfigFirebase.Device_Id);
    cJSON_AddNumberToObject(Datos_Json, "current", Status.Measures.instant_current);
    //TODOJ: Cambiar esto!!!
    /*if(!memcmp("C", Status.HPT_status, 1)){
        cJSON_AddNumberToObject(Datos_Json, "current", corrienteDeseada);
    }
    else{
        cJSON_AddNumberToObject(Datos_Json, "current", Status.Measures.instant_current);
    }*/

    //si es trifasico, enviar informacion de todas las fases
    if(Status.Trifasico){
        cJSON_AddNumberToObject(Datos_Json, "currentB", Status.MeasuresB.instant_current);
        cJSON_AddNumberToObject(Datos_Json, "currentC", Status.MeasuresC.instant_current);
    }
    cJSON_AddStringToObject(Datos_Json, "HPT", Status.HPT_status);

    if(ChargingGroup.AskPerm){
        cJSON_AddNumberToObject(Datos_Json, "Perm",1);
    }
    else{
        cJSON_AddNumberToObject(Datos_Json, "Perm",0);
    }

    char *my_json_string = cJSON_Print(Datos_Json);

    cJSON_Delete(Datos_Json);

    coap_put("DATA", my_json_string);

#ifdef DEBUG_COAP
    Serial.printf("COAP_Nativo - Send_Data: JSON ENVIADO (coap_put): %s\n", my_json_string);
#endif

    free(my_json_string);
}

//enviar mi grupo de cargadores
void Send_Chargers(){
  char buffer[500];

  if(ChargingGroup.Charger_number< 10){
      sprintf(buffer,"0%i",(char)ChargingGroup.Charger_number);
  }
  else{
      sprintf(buffer,"%i",(char)ChargingGroup.Charger_number);
  }
  
  for(uint8_t i=0;i< ChargingGroup.Charger_number;i++){
      memcpy(&buffer[2+(i*11)],charger_table[i].name,8);  

      uint8_t value = (charger_table[i].Circuito << 2) + charger_table[i].Fase;

      if(value< 10){
        sprintf(&buffer[10+(i*11)],"00%i",(char)value);
      }
      else if(value< 100){
        sprintf(&buffer[10+(i*11)],"0%i",(char)value);
      }
      else{
        sprintf(&buffer[10+(i*11)],"%i",(char)value);
      }
  }
#ifdef DEBUG_COAP
    Serial.printf("COAP_Nativo - Send_Chargers: CHARGERS enviado (coap_put): %s\n", buffer);
#endif
  coap_put("CHARGERS", buffer);
}

//enviar circuitos
void Send_Circuits(){
    char buffer[500];

    if(ChargingGroup.Circuit_number< 10){
        sprintf(buffer,"0%i",(char)ChargingGroup.Circuit_number);
    }
    else{
        sprintf(buffer,"%i",(char)ChargingGroup.Circuit_number);
    }
    
    for(uint8_t i=0;i< ChargingGroup.Circuit_number;i++){ 
        buffer[i+2] = (char)Circuitos[i].limite_corriente;
    }
#ifdef DEBUG_COAP
    Serial.printf("COAP_Nativo - Send_Circuits: CHARGERS enviado (coap_put): %s\n", buffer);
#endif
   coap_put("CIRCUITS", buffer);
}

//Enviar parametros
void Send_Params(){

    cJSON *Params_Json;
    Params_Json = cJSON_CreateObject();

    cJSON_AddNumberToObject(Params_Json, "cdp", ChargingGroup.Params.CDP);
    cJSON_AddNumberToObject(Params_Json, "contract", ChargingGroup.Params.ContractPower);
    cJSON_AddNumberToObject(Params_Json, "active", ChargingGroup.Params.GroupActive ? 1 : 0);
    cJSON_AddNumberToObject(Params_Json, "master", ChargingGroup.Params.GroupMaster ? 1 : 0);
    cJSON_AddNumberToObject(Params_Json, "pot_max", ChargingGroup.Params.potencia_max);

    char *my_json_string = cJSON_Print(Params_Json);
    cJSON_Delete(Params_Json);

    coap_put("PARAMS", my_json_string);
#ifdef DEBUG_COAP
    Serial.printf("COAP_Nativo - Send_Params: PARAMS JSON enviado (coap_put): %s\n", my_json_string);
#endif
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
    ChargingGroup.Conected = false;
    ChargingGroup.StartClient = false;
    ChargingGroup.Finding = true;
    int delai = 5000;
#ifdef DEBUG_GROUPS
    Serial.println("Necesitamos un nuevo maestro!");
#endif
    while(!ChargingGroup.Conected){
        if(pdTICKS_TO_MS(xTaskGetTickCount() - xStart) > delai){ //si pasan 30 segundos, elegir un nuevo maestro
            delai=500;           

            if(!memcmp(charger_table[reintentos].name,ConfigFirebase.Device_Id,8)){
                ChargingGroup.Params.GroupActive = true;
                ChargingGroup.Params.GroupMaster = true;
                break;
            }
            else{
                xStart = xTaskGetTickCount();
                reintentos++;
                if(reintentos == ChargingGroup.Charger_number){
                    //Ultima opcion, mirar si yo era el maestro
                    if(!memcmp(charger_table[0].name,ConfigFirebase.Device_Id,8)){
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.Params.GroupMaster = true;
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
#ifdef DEBUG_GROUPS
    Serial.printf("Fin tarea busqueda maestro\n");
#endif
    store_params_in_mem();
    ChargingGroup.Finding = false;
    vTaskDelete(NULL);
}

void start_server(){
    if(xServerHandle != NULL){
        vTaskDelete(xServerHandle);
        xServerHandle = NULL;
    } 
    if(xLimitHandle != NULL){
        vTaskDelete(xLimitHandle);
        xLimitHandle = NULL;
    }
    xServerHandle = xTaskCreateStatic(coap_server, "coap_server", 4096*4, NULL, 1, xServerStack, &xServerBuffer);
    xLimitHandle  = xTaskCreateStatic(LimiteConsumo, "limite_consumo", 4096*4, NULL, 1, xLimitStack, &xLimitBuffer);
}

void start_client(){
 
    if(ChargingGroup.Params.GroupMaster){
        ChargingGroup.Params.GroupMaster = false;
        store_params_in_mem();
    }

    if (xClientHandle != NULL && eTaskGetState(xClientHandle) != eDeleted) {
        ESP_LOGI(TAG,"COAP Client task already running - Deleting...");
        vTaskDelete(xClientHandle);  
        xClientHandle = NULL;
    }
    xClientHandle = xTaskCreateStatic(coap_client, "coap_client", 4096 * 4, NULL, 1, xClientStack, &xClientBuffer);
    ESP_LOGI(TAG,"COAP Client task started");
}


/*******************************************************
 * Funciones para llamar desde otros puntos del codigo
 * ****************************************************/
void coap_start_server(){
    if(!ChargingGroup.Params.GroupMaster){
        ChargingGroup.Params.GroupMaster = true;
        store_params_in_mem();
    }
    ChargingGroup.Conected = true;

    //Preguntar si hay maestro en el grupo
    for(uint8_t i =0; i<10;i++){
        broadcast_a_grupo("Master here?", 12);

        delay(20);
        if(!ChargingGroup.Params.GroupMaster){
            ChargingGroup.Conected = false;
            store_params_in_mem();
            break;
        }
    }
    if(ChargingGroup.Params.GroupMaster){
        //Ponerme el primero en el grupo para indicar que soy el maestro
        if(ChargingGroup.Charger_number>0 && check_in_group(ConfigFirebase.Device_Id,charger_table, ChargingGroup.Charger_number ) != 255){
            
            while(memcmp(charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                carac_charger OldMaster=charger_table[0];
                remove_from_group(OldMaster.name, charger_table, &ChargingGroup.Charger_number);
                add_to_group(OldMaster.name, OldMaster.IP, charger_table, &ChargingGroup.Charger_number);
                charger_table[ChargingGroup.Charger_number-1].Fase=OldMaster.Fase;
                charger_table[ChargingGroup.Charger_number-1].Circuito =OldMaster.Circuito;
            }
            //store_group_in_mem(charger_table, ChargingGroup.Charger_number);
        }
        else{
            //Si el grupo está vacio o el cargador no está en el grupo,
#ifdef DEBUG_GROUPS
            Serial.printf("No estoy en el grupo!!\n");
#endif
            ChargingGroup.Params.GroupMaster = false;
            ChargingGroup.Params.GroupActive = false;
            ChargingGroup.Conected = false;
            store_params_in_mem();
            return;
        }
        broadcast_a_grupo("Start client", 12);

        ChargingGroup.MasterIP.fromString(String(ip4addr_ntoa(&Coms.ETH.IP)));
        start_server();
    }

}

void coap_start_client(){
  start_client();
}
#endif