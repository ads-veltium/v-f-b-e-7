#include "contador.h"
#ifdef CONNECTED

extern carac_Status Status;
extern carac_Coms Coms;
extern carac_Contador   ContadorExt;

Meter_HTTP_Client MeterClient("192.168.1.1", 1000);

//Contador de Iskra
/*********** Clase Contador ************/

//Buscar el contador
void Contador::find(){
    #ifdef DEBUG_MEDIDOR
    Serial.println("Iniciando fase busqueda ");
    #endif
    xTaskCreate( BuscarContador_Task, "BuscarContador", 4096*4, NULL, 5, NULL);   
}

void Contador::begin(String Host){
    MeterUrl = "http://";
    MeterUrl+=Host+"/get_command?command=get_measurements";

    MeterClient.set_url(MeterUrl);
    MeterClient.begin();
    Inicializado = true;
}

void Contador::end(){
    MeterClient.end();
    Inicializado = false;
}

bool Contador::read(){
    bool result_read;
    result_read = MeterClient.Send_Command(MeterUrl,LEER);
#ifdef DEBUG_MEDIDOR
    Serial.printf("contador - read(): Resultado de lectura: %i\n", result_read);
#endif
/*    if (!MeterClient.Send_Command(MeterUrl,LEER)) {
        #ifdef DEVELOPMENT
        Serial.printf("Counter reading error\n");
        #endif
        ContadorExt.ConexionPerdida = true;
        Update_Status_Coms(MED_CONECTION_LOST);
        Coms.ETH.restart = true;
        return false;
    } 
*/  // ADS Me paso por el forro los errores de lectura ¿?
    Measurements.clear();
    deserializeJson(Measurements,MeterClient.ObtenerRespuesta());
    return true;
}
/*********** Convertir los datos del json ************/
void Contador::parse(){
    //Podemos medir lo que nos salga, pero de momento solo queremos intensidades
    String medida , time;
    static String last_time = " ";
    static uint8_t old = 0, Same_time_count = 0;

    //comprobar que no se ha desconectado el medidor
    time = Measurements["header"]["local_time"].as<String>();
    if(time == last_time){
        if(++Same_time_count > 10 && !ContadorExt.ConexionPerdida){
            ContadorExt.ConexionPerdida = true;
            Update_Status_Coms(MED_CONECTION_LOST);
            if (!Coms.ETH.ON){
                Coms.ETH.restart = true;
            }
            return;
        }
    }
    else{
        ContadorExt.ConexionPerdida = false;
        Same_time_count = 0;
        Update_Status_Coms(MED_LEYENDO_MEDIDOR);
    }
    last_time = time;
    
    //Leer corrientes
    /*medida = Measurements["measurements"]["I1"].as<String>();
    ContadorExt.CurrentA = medida.toFloat() *100;
    medida = Measurements["measurements"]["I2"].as<String>();
    ContadorExt.CurrentB = medida.toFloat() *100;
    medida = Measurements["measurements"]["I3"].as<String>();
    ContadorExt.CurrentC = medida.toFloat() *100;*/

    //Leer potencias 
    medida = Measurements["measurements"]["P0"].as<String>();
#ifdef DEBUG_MEDIDOR
    Serial.printf("contador - parse: Medida = %s\n",medida.c_str()); 
#endif

    ContadorExt.MedidorConectado = medida != "null";
    if(ContadorExt.MedidorConectado != old){
        /* if(old){ //Si ya estaba leyendo y perdemos comunicacion,
            if(!ContadorExt.ConexionPerdida){
                ContadorExt.ConexionPerdida = true;
                Update_Status_Coms(MED_CONECTION_LOST);
                //Reiniciamos el eth, esto hace que el medidor al recibir una nueva ip se reinicie
                Coms.ETH.restart = true;
            }
        }
        else{
            ContadorExt.ConexionPerdida = false;
        }*/
        old = ContadorExt.MedidorConectado;
        Update_Status_Coms(MED_LEYENDO_MEDIDOR);
    }

    if(ContadorExt.ConexionPerdida){
        ContadorExt.Power = 0;
    }

    int32_t medida_power = 0;
    if(medida.indexOf("k W")>0){
        medida_power = medida.toFloat()*1000;
    }
    else{
        medida_power= medida.toFloat();
        if(medida_power > 60000){
            medida_power = 0;
        }
    }
    ContadorExt.Power = medida_power;

#ifdef DEMO_MODE_METER
    extern uint8_t consumo_total;
    ContadorExt.Power = 100 * medida_power + 230 * consumo_total; // Valor ficticio del contador = 100*valor medido(P) + Potencia de los equipos en carga. 
#endif

#ifdef DEBUG_MEDIDOR
#ifdef DEMO_MODE_METER
    Serial.printf("contador - parse: Medida real de medidor: Power = %i W. Consumo total = %i W. Medida FAKE = %i W\n", medida_power, 230 * consumo_total, ContadorExt.Power);
#else
    Serial.printf("contador - parse: ContadorExt.Power = %i \n", ContadorExt.Power);
#endif
#endif
}

/********** Cliente http generico *************/
bool _lectura_finalizada = false;
int  _leidos = 0;
char *_respuesta_total = new char[2048];
esp_err_t _generic_http_event_handle(esp_http_client_event_t *evt){

    char *response  = new char[1024];
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            _lectura_finalizada = false;
            _leidos = 0;
            free(_respuesta_total);
            _respuesta_total = new char[2048];
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            if(evt->data_len>0 && evt->data_len<1024 && _leidos +evt->data_len < 2048){
                esp_http_client_read(evt->client, response, evt->data_len);
                memcpy(&_respuesta_total[_leidos],response,evt->data_len);
                _leidos+=evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            
            _lectura_finalizada = true;
            break;
        case HTTP_EVENT_DISCONNECTED:
            //printf("END");
            //esp_http_client_cleanup(evt->client);
            break;
    }
    if(response != NULL){
        free(response);
        response = NULL;
    }
    

    return ESP_OK;
}
void Meter_HTTP_Client::set_url(String url){
    _url=url;
}

void Meter_HTTP_Client::begin(){
    
    esp_http_client_config_t config = {
        .url = _url.c_str(),
        .timeout_ms = 5000,
        .max_redirection_count =3,
        .event_handler = _generic_http_event_handle,
        .buffer_size_tx = 2048,
        .is_async = false,
    };
    _client = esp_http_client_init(&config);
    esp_http_client_set_header(_client,"Content-Type", "application/json");
}

void Meter_HTTP_Client::end(){

    esp_http_client_cleanup(_client);
}

String Meter_HTTP_Client::ObtenerRespuesta(){
    return String(_respuesta_total);
}

bool Meter_HTTP_Client::Send_Command(String url, uint8_t Command){   
    uint8_t tiempo_lectura =0;

    esp_http_client_set_url(_client, url.c_str());
    switch(Command){
        case ESCRIBIR:        
            esp_http_client_set_method(_client, HTTP_METHOD_POST);
            //esp_http_client_set_post_field(_client, SerializedData.c_str(), SerializedData.length());
            break;
        case UPDATE:
            esp_http_client_set_method(_client, HTTP_METHOD_PATCH);
            //esp_http_client_set_post_field(_client, SerializedData.c_str(), SerializedData.length());
            break;
        case TIMESTAMP:     
            esp_http_client_set_method(_client, HTTP_METHOD_PUT);
            esp_http_client_set_post_field(_client, "{\".sv\": \"timestamp\"}", strlen("{\".sv\": \"timestamp\"}"));
            break;
        case LEER:
            free(_respuesta_total);
            _respuesta_total = new char[2048];
            esp_http_client_set_method(_client, HTTP_METHOD_GET);
            break;

        default:
            Serial.println("Accion no implementada!");
            return false;
            break;
    }

    int err = esp_http_client_perform(_client);
    if (err != ESP_OK ) {
#ifdef DEVELOPMENT
        Serial.printf("VeltFirebase - Meter_HTTP_Client::Send_Command: HTTP request failed: %s\n", esp_err_to_name(err));
#endif
        return false;
    }

    if(Command < 5){
        _lectura_finalizada = false;
        _leidos = 0;
        return true;
    }

    while(!_lectura_finalizada ){
        delay(50);
        tiempo_lectura ++;
        if(tiempo_lectura > 30){
            return false;
        }
    }

    if(tiempo_lectura >= 30){
        return false;
    }

    _lectura_finalizada = false;
    _leidos = 0;

    return true;
}

#endif