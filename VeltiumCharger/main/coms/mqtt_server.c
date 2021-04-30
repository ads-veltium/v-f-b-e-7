#include "mqtt_server.h"
#include "../group_control.h"

extern uint8_t new_charger;
// A list of subscription, held in memory
struct sub *s_subs EXT_RAM_ATTR;

bool StopMQTT = false;

// Wildcard(#/+) support version
int _mg_strcmp(const struct mg_str str1, const struct mg_str str2) {
	size_t i1 = 0;
	size_t i2 = 0;
	while (i1 < str1.len && i2 < str2.len) {
		int c1 = str1.ptr[i1];
		int c2 = str2.ptr[i2];
		////ESP_LOGE(pcTaskGetTaskName(NULL), "c1=%c c2=%c\n",c1, c2);

		// str2=[/hoge/#]
		if (c2 == '#') return 0;

		// str2=[/hoge/+/123]
		// Search next slash
		if (c2 == '+') {
			// str1=[/hoge//123]
			// str2=[/hoge/+/123]
			if (c1 == '/') {
				i2++;
			// str1=[/hoge/123/123]
			// str2=[/hoge/+/123]
			} else {
				for (i1=i1;i1+1<str1.len;i1++) {
					int c3 = str1.ptr[i1+1];
					////ESP_LOGE(pcTaskGetTaskName(NULL), "i1=%ld c3=%c\n", i1, c3);
					if (c3 == '/') break;
				}
				i1++;
				i2++;
			}
		} else {
			if (c1 < c2) return -1;
			if (c1 > c2) return 1;
			i1++;
			i2++;
		}
	}
	if (i1 < str1.len) return 1;
	if (i2 < str2.len) return -1;
	return 0;
}

bool GetStopMQTT(){
	return StopMQTT;
}

void SetStopMQTT(bool value){
	StopMQTT = value;
}

// Event handler function
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) { 
  if (ev == MG_EV_MQTT_CMD) {
	struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
	switch (mm->cmd) {
	  case MQTT_CMD_CONNECT: {
		uint8_t response[] = {0, 0};
		mg_mqtt_send_header(c, MQTT_CMD_CONNACK, 0, sizeof(response));
		mg_send(c, response, sizeof(response));
		//printf("Nuevo cargador almacenado, \n\tram total = %i\n\tram interna = %i\n", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
		break;
	  }
	  case MQTT_CMD_SUBSCRIBE: {
		// Client subscribe. Add to the subscription list
		//_mg_mqtt_dump("SUBSCRIBE", mm);
		int pos = 4;  // Initial topic offset, where ID ends
		uint8_t qos;
		struct mg_str topic;
		while ((pos = mg_mqtt_next_sub(mm, &topic, &qos, pos)) > 0) {
		  struct sub *sub = calloc(1, sizeof(*sub));
		  sub->c = c;
		  sub->topic = mg_strdup(topic);
		  sub->qos = qos;
		  LIST_ADD_HEAD(struct sub, &s_subs, sub);
		}
		new_charger = 1;
		break;
	  }
	  case MQTT_CMD_UNSUBSCRIBE: {
		int pos = 4;  // Initial topic offset, where ID ends
		struct mg_str topic;
		while ((pos = mg_mqtt_next_unsub(mm, &topic, pos)) > 0) {
			// Remove from the subscription list

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
		  if (_mg_strcmp(mm->topic, sub->topic) != 0) continue;
		  mg_mqtt_pub(sub->c, &mm->topic, &mm->data);
		}
	  }
	  case MQTT_CMD_PINGREQ: {
		//ESP_LOGE(pcTaskGetTaskName(NULL), "PINGREQ %p", c->fd);
		mg_mqtt_pong(c); // Send PINGRESP
		break;
	  }
	}
  } else if (ev == MG_EV_CLOSE) {
	printf("Desconectando cargador, \n\tram total = %i\n\tram interna = %i\n", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
	// Client disconnects. Remove from the subscription list
	for (struct sub *next, *sub = s_subs; sub != NULL; sub = next) {
		next = sub->next;
		if (c != sub->c) continue;
		printf("Borrando de la lista de subscripcion %s\n", sub->topic.ptr);
		LIST_DELETE(struct sub, &s_subs, sub);
		free(sub);
	}		
	printf("Cargador desconectado, \n\tram total = %i\n\tram interna = %i\n", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
  }
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

static StackType_t xPOLLstack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xPOLLBuffer ;
struct mg_connection *mgc;
struct mg_mgr mgr;
TaskHandle_t PollerHandle = NULL;
TickType_t xStart;

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

		case MG_EV_MQTT_OPEN:
			//ESP_LOGE(pcTaskGetTaskName(NULL), "MG_EV_OPEN");
			// MQTT connect is successful
			break;
		case MG_EV_MQTT_MSG:{
			// When we get echo response, print it
			struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
			if(!memcmp(mm->topic.ptr, "Device_Status", mm->topic.len)){
				New_Data(mm->data.ptr, mm->data.len);
				xStart = xTaskGetTickCount();
			}
			else if(!memcmp(mm->topic.ptr, "Ping", mm->topic.len)){
				mqtt_publish("Pong", "ABCD");
				Ping_Req(mm->data.ptr);
			}
			else if(!memcmp(mm->topic.ptr, "Pong", mm->topic.len)){
				xStart = xTaskGetTickCount();
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
				xStart = xTaskGetTickCount();	
			}
			//printf("Recibido %.*s <- %.*s \n", (int) mm->data.len, mm->data.ptr, (int) mm->topic.len, mm->topic.ptr);
			break;
		}
	}
}

//Bucle principal del cliente mqtt
void mqtt_polling(void *params){
	xStart = xTaskGetTickCount();
	while (1) {
		
		if(!StopMQTT)mg_mgr_poll(&mgr, 10);
		vTaskDelay(5);
		uint32_t transcurrido = pdTICKS_TO_MS(xTaskGetTickCount() - xStart);
		
		if(transcurrido > 2500){
			StopMQTT = true;
			break;
		}
		
		if(StopMQTT){
			break;
		}
	}
	mg_mqtt_disconnect(mgc);
	mg_mgr_free(&mgr);
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
	mg_mgr_init(&mgr);
	struct mg_mqtt_opts opts;  // MQTT connection options

	memset(&opts, 0, sizeof(opts));					// Set MQTT options
	opts.client_id = mg_str(pub_opts->Client_ID);   // Set Client ID
	opts.qos = 1;									// Set QoS to 1

	mgc = mg_mqtt_connect(&mgr, pub_opts->url, &opts, publisher_fn, &pub_opts->url);	// Create client connection
	
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
	if(!StopMQTT)mg_mqtt_sub(mgc, &topic);							
}
