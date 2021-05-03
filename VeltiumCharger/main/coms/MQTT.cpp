#include "../control.h"
#include "AsyncUDP.h"
#include "cJSON.h"

extern "C" {
#include "mongoose.h"
}

//Funciones del MQTT
#include "mqtt_server.h"
#include "../group_control.h"

struct sub *s_subs EXT_RAM_ATTR;

static StackType_t xPOLLstack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xPOLLBuffer ;
struct mg_connection *mgc;
struct mg_mgr client_mgr;
TaskHandle_t PollerHandle = NULL;
TickType_t xStart_Server;

bool StopMQTT = false;
uint8_t check_in_group(const char* ID, carac_chargers* group);
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);

AsyncUDP udp EXT_RAM_ATTR;

String Decipher(String input);
String Encipher(String input);

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Params Params;
extern carac_group  ChargingGroup;
extern carac_Comands  Comands ;

static StackType_t xSERVERStack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xSERVERBuffer ;

static StackType_t xPUBLISHERStack [4096]     EXT_RAM_ATTR;
StaticTask_t xPUBLISHERBuffer ;
carac_chargers net_group EXT_RAM_ATTR;

//Prototipos de funciones externas
void mqtt_server(void *pvParameters);

//Prototipos de funciones internas
void start_MQTT_server();
void start_MQTT_client(IPAddress remoteIP);


// Event handler function
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) { 

  printf("Paso 0 :%i\n", esp_get_free_internal_heap_size());
  if (ev == MG_EV_MQTT_CMD) {
	struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
	switch (mm->cmd) {
	  case MQTT_CMD_CONNECT: {
		uint8_t response[] = {0, 0};
		mg_mqtt_send_header(c, MQTT_CMD_CONNACK, 0, sizeof(response));
		mg_send(c, response, sizeof(response));
		break;
	  }
	  case MQTT_CMD_SUBSCRIBE: {
		int pos = 4;  // Initial topic offset, where ID ends
		uint8_t qos;
		struct mg_str topic;
		while ((pos = mg_mqtt_next_sub(mm, &topic, &qos, pos)) > 0) {
		  struct sub *sub = (struct sub *) calloc(1, sizeof(*sub));
		  sub->c = c;
		  sub->topic = mg_strdup(topic);
		  sub->qos = qos;
		  LIST_ADD_HEAD(struct sub, &s_subs, sub);
		}
		ChargingGroup.SendNewParams = true;
        ChargingGroup.SendNewGroup  = true;
		break;
	  }
	  case MQTT_CMD_UNSUBSCRIBE: {
		int pos = 4;  // Initial topic offset, where ID ends
		struct mg_str topic;
		while ((pos = mg_mqtt_next_unsub(mm, &topic, pos)) > 0) {
			for (struct sub *next, *sub = s_subs; sub != NULL; sub = next) {
				next = sub->next;
				if (c != sub->c) continue;
				if (strncmp(topic.ptr, sub->topic.ptr, topic.len) != 0) continue;
				LIST_DELETE(struct sub, &s_subs, sub);
				free(sub);
			}
		}
        break;
	  }
	  case MQTT_CMD_PUBLISH: {
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		  if (strcmp(mm->topic.ptr, sub->topic.ptr) != 0) continue;
		  mg_mqtt_pub(sub->c, &mm->topic, &mm->data);
		}
	  }
	  case MQTT_CMD_PINGREQ: {
		mg_mqtt_pong(c); // Send PINGRESP
		break;
	  }
	}
  } else if (ev == MG_EV_CLOSE) {
	// Client disconnects. Remove from the subscription list
	for (struct sub *next, *sub = s_subs; sub != NULL; sub = next) {
		next = sub->next;
		if (c != sub->c) continue;
		printf("Borrando de la lista de subscripcion %s\n", sub->topic.ptr);
		LIST_DELETE(struct sub, &s_subs, sub);
		free(sub);
        printf("Free mem %i\n", esp_get_free_internal_heap_size() );
	}		
  }
  printf("Paso 100 :%i\n", esp_get_free_internal_heap_size());
}

void mqtt_server(void *pvParameters){

	mg_log_set("1"); // Set to log level to LL_ERROR
	const char *s_listen_on = (char*)pvParameters;
	struct mg_mgr mgr;
	mg_mgr_init(&mgr);
	mg_mqtt_listen(&mgr, s_listen_on, fn, NULL);  // Create MQTT listener

	/* Processing events */
	while (1) {
		mg_mgr_poll(&mgr, 10);
		vTaskDelay(5);
		if(StopMQTT){
			break;
		}
	}
	// Never reach here
	printf("Server Stopped\n");
	mg_mgr_free(&mgr);
	vTaskDelete(NULL);
}

static void publisher_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
	switch(ev){
		case MG_EV_ERROR:
			ESP_LOGE(pcTaskGetTaskName(NULL), "MG_EV_ERROR %p %s", c->fd, (char *) ev_data);
			StopMQTT = true;
		break;

		case MG_EV_CONNECT:
			if (mg_url_is_ssl((char *)fn_data)) {
				struct mg_tls_opts opts = {.ca = "ca.pem"};
				mg_tls_init(c, &opts);
			}
			break;

		case MG_EV_MQTT_MSG:{
			// When we get echo response, print it
			struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
			if(!memcmp(mm->topic.ptr, "Data", mm->topic.len)){
				New_Data(mm->data.ptr, mm->data.len);
				xStart_Server = xTaskGetTickCount();
			}
			else if(!memcmp(mm->topic.ptr, "Ping", mm->topic.len)){
				mqtt_publish("Pong", "ABCD");
				Ping_Req(mm->data.ptr);
			}
			else if(!memcmp(mm->topic.ptr, "Pong", mm->topic.len)){
				xStart_Server = xTaskGetTickCount();
			}
			else if(!memcmp(mm->topic.ptr, "RTS", mm->topic.len)){
				switch(mm->data.ptr[0]-'0'){
					case 1: //Group_Params
						New_Params(&mm->data.ptr[1], mm->data.len);
						break;
					case 2: //Group Control
						New_Control(&mm->data.ptr[1], mm->data.len);
						break;
					case 3: //Group Chargers
						New_Group(&mm->data.ptr[1], mm->data.len);
						break;
					default:
						printf("Me ha llegado algo indefinido\n");
						break;
				}
				xStart_Server = xTaskGetTickCount();	
			}
			break;
		}
	}
}

//Bucle principal del cliente mqtt
void mqtt_polling(void *params){
	xStart_Server = xTaskGetTickCount();
	while (1) {	
		if(!StopMQTT)mg_mgr_poll(&client_mgr, 10);
		vTaskDelay(pdMS_TO_TICKS(5));
		uint32_t transcurrido = pdTICKS_TO_MS(xTaskGetTickCount() - xStart_Server);
		
		if(transcurrido > 2500){
			StopMQTT = true;
			break;
		}
		
		if(StopMQTT){
			break;
		}
	}
	mg_mqtt_disconnect(mgc);
	mg_mgr_free(&client_mgr);
	mgc = NULL;
	PollerHandle = NULL;
	vTaskDelete(NULL);
}

//conectar al broker
bool mqtt_connect(mqtt_sub_pub_opts *pub_opts){

	if(PollerHandle != NULL || StopMQTT){
		return false;
	}

	/* Arrancar la conexion*/	
	mg_mgr_init(&client_mgr);
	struct mg_mqtt_opts opts;  // MQTT connection options

	memset(&opts, 0, sizeof(opts));					// Set MQTT options
	opts.client_id = mg_str(pub_opts->Client_ID);   // Set Client ID
	opts.qos = 0;									// Set QoS to 1

    mgc = mg_mqtt_connect(&client_mgr, pub_opts->url, &opts, publisher_fn, &pub_opts->url);	// Create client connection
	
	PollerHandle = xTaskCreateStatic(mqtt_polling,"POLLER",1024*6,NULL,2,xPOLLstack,&xPOLLBuffer); 

	return true;
}

void mqtt_publish(char* Topic, char* Data){
	struct mg_str topic = mg_str(Topic);
	struct mg_str data = mg_str(Data);
	if(!StopMQTT)mg_mqtt_pub(mgc, &topic, &data);
}

void mqtt_subscribe(char* Topic){
	struct mg_str topic = mg_str(Topic);
	mg_mqtt_sub(mgc, &topic);							
}

void stop_MQTT(){
    ChargingGroup.Conected   = false;
    ChargingGroup.StartClient = false;
    StopMQTT = true;
    printf("Stopping MQTT\n");
}

//Añadir un cargador a un equipo
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group){
    if(group->size < MAX_GROUP_SIZE){
        if(check_in_group(ID,group)==255){
            memcpy(group->charger_table[group->size].name, ID, 8);
            group->charger_table[group->size].name[8]='\0';
            group->charger_table[group->size].IP = IP;
            group->size++;
            return true;
        }
    }
    printf("Error al añadir al grupo \n");
    return false;
}

//Comprobar si un cargador está almacenado en las tablas de equipos
uint8_t check_in_group(const char* ID, carac_chargers* group){
    for(int j=0;j < group->size;j++){
        if(!memcmp(ID,  group->charger_table[j].name,8)){
            return j;
        }
    }
    return 255;
}

//Eliminar un cargador de un grupo
bool remove_from_group(const char* ID ,carac_chargers* group){
    for(int j=0;j < group->size;j++){
        if(!memcmp(ID,  group->charger_table[j].name,8)){
            if(j!=group->size-1){ //si no es el ultimo debemos shiftear la lista
                for(uint8_t i=j;i < group->size-1;i++){
                    group->charger_table[i] = group->charger_table[i+1];
                }
            }
            group->charger_table[group->size-1].IP = INADDR_NONE;       
            memset(group->charger_table[group->size-1].name,0,9);
            --group->size;
            printf("Cargador %s eliminado\n", ID);
            return true;
        }
    }
    printf("El cargador no está en el grupo!\n");
    return false;
}

//Almacenar grupo en la flash
void store_group_in_mem(carac_chargers* group){
    char sendBuffer[252];
    if(group->size< 10){
        sprintf(&sendBuffer[0],"0%i",(char)group->size);
    }
    else{
        sprintf(&sendBuffer[0],"%i",(char)group->size);
    }

    if(group->size < 25){
        if(group->size>0){
            for(uint8_t i=0;i<group->size;i++){
                memcpy(&sendBuffer[2+(i*9)],group->charger_table[i].name,8);
                itoa(group->charger_table[i].Fase,&sendBuffer[10+(i*9)],10);
            }
            SendToPSOC5(sendBuffer,ChargingGroup.group_chargers.size*9+2,GROUPS_DEVICES); 
        }
        else{
            for(int i=0;i<=250;i++){
                sendBuffer[i]=(char)0;
            }
            SendToPSOC5(sendBuffer,250,GROUPS_DEVICES); 
        }
        
        delay(100);
        return;
    }
    printf("Error al almacenar el grupo en la memoria\n");
}

//Eliminar grupo
void remove_group(carac_chargers* group){
    if(group->size >0){
        for(int j=0;j < group->size;j++){
            group->charger_table[j].IP = INADDR_NONE;       
            group->charger_table[j].Fase = 0;       
            memset(group->charger_table[j].name,0,9);
        }
        group->size = 0;
        return;
    }
    printf("Error al eliminar el grupo\n");  
}

//Obtener la ip de un equipo dado su ID
IPAddress get_IP(const char* ID){
    for(int j=0;j < net_group.size;j++){
        if(!memcmp(ID,  net_group.charger_table[j].name,8)){
            return net_group.charger_table[j].IP;
        }
    }
    return INADDR_NONE;
}

//Enviar mensajes solo a los miembros del grupo
void broadcast_a_grupo(char* Mensaje){
    AsyncUDPMessage mensaje (13);
    mensaje.write((uint8_t*)(Encipher(Mensaje).c_str()), 13);

    for(int i =0; i < net_group.size;i++){
        for(int j=0;j < ChargingGroup.group_chargers.size;j++){
            if(check_in_group(net_group.charger_table[i].name, &ChargingGroup.group_chargers)<255){
                udp.sendTo(mensaje, net_group.charger_table[i].IP,1234);
                ChargingGroup.group_chargers.charger_table[j].IP = net_group.charger_table[i].IP; 
            }
        }
    }
}

//Arrancar la comunicacion udp para escuchar cuando el maestro nos lo ordene
void start_udp(){
    if(udp.listen(1234)) {
        udp.onPacket([](AsyncUDPPacket packet) {         
            int size = packet.length();
            char buffer[size+1] ;
            memcpy(buffer, packet.data(), size);
            buffer[size] = '\0';

            String Desencriptado;
            Desencriptado = Decipher(String(buffer));

            if(size<=8){
                if(packet.isBroadcast()){                   
                    packet.print(Encipher(String(ConfigFirebase.Device_Id)).c_str());
                }

                //Si el cargador está en el grupo de carga, le decimos que es un esclavo
                if(ChargingGroup.Params.GroupMaster && ChargingGroup.Conected){
                    if(check_in_group(Desencriptado.c_str(), &ChargingGroup.group_chargers)!=255){
                        Serial.printf("El cargador VCD%s está en el grupo de carga\n", Desencriptado.c_str());  
                        AsyncUDPMessage mensaje (13);
                        mensaje.write((uint8_t*)(Encipher("Start client").c_str()), 13);
                        udp.sendTo(mensaje,packet.remoteIP(),1234);
                    }
                }
          
                if(check_in_group(Desencriptado.c_str(), &net_group)==255){
                    Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista de red\n", Desencriptado.c_str(), packet.remoteIP().toString().c_str());  
                    add_to_group(Desencriptado.c_str(), packet.remoteIP(), &net_group);
                } 
            }
            else{
                if(!memcmp(Desencriptado.c_str(), "Start client", 13)){
                    if(!ChargingGroup.Conected && !ChargingGroup.StartClient){
                        printf("Soy parte de un grupo !!\n");
                        StopMQTT = false;
                        ChargingGroup.StartClient = true;
                        ChargingGroup.Params.GroupMaster = false;
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.MasterIP =packet.remoteIP();
                        
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Hay maestro?", 13)){
                    if(ChargingGroup.Conected && ChargingGroup.Params.GroupMaster){
                        packet.print(Encipher(String("Bai, hemen nago")).c_str());
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Bai, hemen nago", 13)){
                    if(!ChargingGroup.Conected && ChargingGroup.Params.GroupMaster){
                        ChargingGroup.StartClient = true;
                        ChargingGroup.Params.GroupMaster = false;
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.MasterIP =packet.remoteIP();
                    }
                }
            }            
        });
    }

    //Avisar al resto de equipos de que estamos aqui!
    udp.broadcast(Encipher(String(ConfigFirebase.Device_Id)).c_str());
}

/*Tarea de emergencia para cuando el maestro se queda muerto mucho tiempo*/
void MasterPanicTask(void *args){
    TickType_t xStart = xTaskGetTickCount();
    uint8 reintentos = 1;
    ChargingGroup.StartClient = false;
    int delai = 30000;
    while(!ChargingGroup.Conected){
        if(pdTICKS_TO_MS(xTaskGetTickCount() - xStart) > delai){ //si pasan 30 segundos, elegir un nuevo maestro
            delai=15000;
            Serial.println("Necesitamos un nuevo maestro!");

            if(!memcmp(ChargingGroup.group_chargers.charger_table[reintentos].name,ConfigFirebase.Device_Id,8)){
                Serial.println("Soy el nuevo maestro!!");
                ChargingGroup.Params.GroupActive = true;
                break;
            }
            else{
                //Ultima opcion, mirar si yo era el maestro
                if(!memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id,8)){
                    Serial.println("Soy el nuevo maestro!!");
                    ChargingGroup.Params.GroupActive = true;
                    break;
                }
                Serial.println("No soy el nuevo maestro, alguien se pondrá :)");
                xStart = xTaskGetTickCount();
                reintentos++;
                if(reintentos == ChargingGroup.group_chargers.size){
                    printf("Ya he mirado todos los equipos y no estoy!\n");
                    break;
                }
            }
        }
        delay(1000);
    }
    vTaskDelete(NULL);
}

/*Tarea para publicar los datos del equipo cada segundo*/
void Publisher(void* args){
    printf("Arrancando publicador\n");
    char buffer[500];
    Params.Fase = 1;
    TickType_t xStart = xTaskGetTickCount();

    while(1){
        uint8_t paso=0;
        //Enviar nuestros datos de carga al grupo
        if(ChargingGroup.SendNewData){   
            paso=1;
            printf("Paso %i :%i\n", paso, esp_get_free_internal_heap_size());

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

            paso=2;
            printf("Paso %i :%i\n", paso, esp_get_free_internal_heap_size()); //-640
            char *my_json_string = cJSON_Print(Datos_Json);   

            paso=3;
            printf("Paso %i :%i\n", paso, esp_get_free_internal_heap_size());  //   -124
            
            cJSON_Delete(Datos_Json);


            paso=4;
            printf("Paso %i :%i\n", paso, esp_get_free_internal_heap_size()); //+644

            mqtt_publish("Data", my_json_string);
            free(my_json_string);
            ChargingGroup.SendNewData = false;


            paso=5;
            printf("Paso %i :%i\n", paso, esp_get_free_internal_heap_size()); //-532
        }
        
        //Enviar nuevos parametros para el grupo
        if( ChargingGroup.SendNewParams){
            buffer[0] = '1';
            
            memcpy(&buffer[1], &ChargingGroup.Params,7);
            mqtt_publish("RTS", buffer);
            ChargingGroup.SendNewParams = false;
        }

        //Enviar los cargadores de nuestro grupo
        if(ChargingGroup.SendNewGroup){
            buffer[0] = '3';
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
            mqtt_publish("RTS", buffer);
            ChargingGroup.SendNewGroup = false;
        }

        //Avisar al maestro de que seguimos aqui
        if(!ChargingGroup.Params.GroupMaster){ 
            mqtt_publish("Ping", ConfigFirebase.Device_Id);
            delay(500);
            if(ChargingGroup.Params.GroupActive){
                if(!ChargingGroup.Conected || StopMQTT){
                    printf("Maestro desconectado!!!\n");
                    ChargingGroup.Params.GroupActive = false;
                    stop_MQTT();
                    xTaskCreate(MasterPanicTask,"MasterPanicTask",4096,NULL,2,NULL);
                    vTaskDelete(NULL);
                }
            }

        }

        //Si soy el maestro, debo comprobar que los esclavos siguen conectados
        else{
            TickType_t Transcurrido = xTaskGetTickCount() - xStart;
            xStart = xTaskGetTickCount();
            for(uint8_t i=0 ;i<ChargingGroup.group_chargers.size;i++){
                ChargingGroup.group_chargers.charger_table[i].Period += Transcurrido;
                //si un equipo lleva mucho sin contestar, lo intentamos despertar
                if(ChargingGroup.group_chargers.charger_table[i].Period >=30000 && ChargingGroup.group_chargers.charger_table[i].Period <=60000){ 
                    AsyncUDPMessage mensaje (13);
                    mensaje.write((uint8_t*)(Encipher("Start client").c_str()), 13);
                    udp.sendTo(mensaje,ChargingGroup.group_chargers.charger_table[i].IP,1234);
                }
                
                //si un equipo lleva muchisimo sin contestar, lo damos por muerto y reseteamos sus valores
                if(ChargingGroup.group_chargers.charger_table[i].Period >=60000 && ChargingGroup.group_chargers.charger_table[i].Period <=65000){
                    if(memcmp(ChargingGroup.group_chargers.charger_table[i].name, ConfigFirebase.Device_Id,8)){
                        paso=3;
                        printf("Paso %i :%i\n", paso, esp_get_free_internal_heap_size());
                        cJSON *Datos_Json;
	                    Datos_Json = cJSON_CreateObject();

                        cJSON_AddStringToObject(Datos_Json, "device_id", ChargingGroup.group_chargers.charger_table[i].name);
                        cJSON_AddNumberToObject(Datos_Json, "fase", ChargingGroup.group_chargers.charger_table[i].Fase);
                        cJSON_AddNumberToObject(Datos_Json, "Delta", 0);
                        cJSON_AddStringToObject(Datos_Json, "HPT", "0V");
                        cJSON_AddNumberToObject(Datos_Json, "limite_fase",0);
                        cJSON_AddNumberToObject(Datos_Json, "current", 0);
                        
                        char *my_json_string = cJSON_Print(Datos_Json);                
                        cJSON_Delete(Datos_Json);
            
                        mqtt_publish("Data", my_json_string);
                        free(my_json_string);
                        paso=4;
                        printf("Paso %i :%i\n", paso, esp_get_free_internal_heap_size());
                    }
                }
            }
        }

        //Si se pausa el grupo, avisar al resto y pausar todo
        if(!ChargingGroup.Params.GroupActive){
            if(!ChargingGroup.Conected || StopMQTT){
                vTaskDelete(NULL);
            }

            buffer[0] = '2';
            memcpy(&buffer[1],"Pause",6);
            mqtt_publish("RTS", buffer);
        }

        //Si llega la orden de borrar, debemos eliminar el grupo de la memoria
        if(ChargingGroup.DeleteOrder){
            if(!ChargingGroup.Conected || StopMQTT){
                vTaskDelete(NULL);
            }

            buffer[0] = '2';
            memcpy(&buffer[1],"Delete",6);
            mqtt_publish("RTS", buffer);
        }
        
        delay(1000);        
    }
    vTaskDelete(NULL);
}

void start_MQTT_server(){
    ChargingGroup.Params.GroupMaster = true;
    printf("Arrancando servidor MQTT\n");
    StopMQTT = false;
    //Preguntar si hay maestro en el grupo
    for(uint8_t i =0; i<10;i++){
        broadcast_a_grupo("Hay maestro?");
        delay(5);
        if(!ChargingGroup.Params.GroupMaster)break;
    }
    
    if(ChargingGroup.Params.GroupMaster){
        ChargingGroup.Conected = true;
        /* Start MQTT Server using tcp transport */
        char path[64];
        sprintf(path, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP));
        xTaskCreateStatic(mqtt_server,"BROKER",1024*6,&path,PRIORIDAD_MQTT,xSERVERStack,&xSERVERBuffer); 
        delay(5000);

        broadcast_a_grupo("Start client");
        mqtt_sub_pub_opts publisher;

        //Ponerme el primero en el grupo para indicar que soy el maestro
        if(ChargingGroup.group_chargers.size>0 && check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) != 255){
            while(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                carac_charger OldMaster=ChargingGroup.group_chargers.charger_table[0];
                remove_from_group(OldMaster.name, &ChargingGroup.group_chargers);
                add_to_group(OldMaster.name, OldMaster.IP, &ChargingGroup.group_chargers);
            }
            store_group_in_mem(&ChargingGroup.group_chargers);
        }
        else{
            //Si el grupo está vacio o el cargador no está en el grupo,
            printf("No tengo cargadores en el grupo o no estoy en mi propio grupo!\n");
            ChargingGroup.Params.GroupMaster = false;
            ChargingGroup.Params.GroupActive = false;
            ChargingGroup.Conected = false;
            stop_MQTT();
            return;
        }
        
        sprintf(publisher.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP));
        memcpy(publisher.Client_ID,ConfigFirebase.Device_Id, 8);

        if(mqtt_connect(&publisher) && !StopMQTT){
            mqtt_subscribe("Data");
            mqtt_subscribe("Ping");
            mqtt_subscribe("RTS");
            xTaskCreateStatic(Publisher,"Publisher",4096,NULL,PRIORIDAD_MQTT,xPUBLISHERStack,&xPUBLISHERBuffer); 
        }
    }
    else{
        Serial.println("Ya hay un maestro en el grupo, espero a que me ordene conectarme!");
    }
}

void start_MQTT_client(IPAddress remoteIP){    
    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", remoteIP.toString().c_str());
    strcpy(publisher.Client_ID,ConfigFirebase.Device_Id);

    if(mqtt_connect(&publisher) && !StopMQTT){
        ChargingGroup.Params.GroupMaster = false;
        ChargingGroup.Conected = true;
        mqtt_subscribe("Data");
        mqtt_subscribe("Pong");
        mqtt_subscribe("RTS");
        xTaskCreateStatic(Publisher,"Publisher",4096,NULL,PRIORIDAD_MQTT,xPUBLISHERStack,&xPUBLISHERBuffer); 
    }
}
