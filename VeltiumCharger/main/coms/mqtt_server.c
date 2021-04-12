#include "mqtt_server.h"
#include "../group_control.h"

static const char *s_listen_on = "mqtt://0.0.0.0:1883";


// A list of client, held in memory
struct client *s_clients = NULL;

// A list of subscription, held in memory
struct sub *s_subs = NULL;

// A list of will topic & message, held in memory
struct will *s_wills = NULL;

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

void _mg_mqtt_dump(char * tag, struct mg_mqtt_message *msg) {
	unsigned char *buf = (unsigned char *) msg->dgram.ptr;
	ESP_LOGI(pcTaskGetTaskName(NULL),"%s=%x %x", tag, buf[0], buf[1]);
	int length = buf[1] + 2;
	ESP_LOG_BUFFER_HEXDUMP(tag, buf, length, ESP_LOG_INFO);
}

#define	WILL_FLAG	0x04
#define WILL_QOS	0x18
#define WILL_RETAIN	0x20

int _mg_mqtt_parse_header(struct mg_mqtt_message *msg, struct mg_str *client, struct mg_str *topic, 
		struct mg_str *payload, uint8_t *qos, uint8_t *retain) {
	client->len = 0;
	topic->len = 0;
	payload->len = 0;
	unsigned char *buf = (unsigned char *) msg->dgram.ptr;
	int Protocol_Name_length =	buf[2] << 8 | buf[3];
	int Connect_Flags_position = Protocol_Name_length + 5;
	uint8_t Connect_Flags = buf[Connect_Flags_position];
	ESP_LOGD("_mg_mqtt_parse_header", "Connect_Flags=%x", Connect_Flags);
	uint8_t Will_Flag = (Connect_Flags & WILL_FLAG) >> 2;
	*qos = (Connect_Flags & WILL_QOS) >> 3;
	*retain = (Connect_Flags & WILL_RETAIN) >> 5;
	ESP_LOGD("_mg_mqtt_parse_header", "Will_Flag=%d *qos=%x *retain=%x", Will_Flag, *qos, *retain);
	client->len = buf[Connect_Flags_position+3] << 8 | buf[Connect_Flags_position+4];
	client->ptr = (char *)&buf[Connect_Flags_position+5];
	ESP_LOGI("_mg_mqtt_parse_header", "client->len=%d", client->len);
	if (Will_Flag == 0) return 0;

#if 0
	int Client_Identifier_length = buf[Connect_Flags_position+3] << 8 | buf[Connect_Flags_position+4];
	ESP_LOGD("_mg_mqtt_parse_header", "Client_Identifier_length=%d", Client_Identifier_length);
	int Will_Topic_position = Protocol_Name_length + Client_Identifier_length + 10;
#endif

	int Will_Topic_position = Protocol_Name_length + client->len + 10;
	topic->len = buf[Will_Topic_position] << 8 | buf[Will_Topic_position+1];
	topic->ptr = (char *)&(buf[Will_Topic_position]) + 2;
	ESP_LOGD("_mg_mqtt_parse_header", "topic->len=%d topic->ptr=[%.*s]", topic->len, topic->len, topic->ptr);
	int Will_Payload_position = Will_Topic_position + topic->len + 2;
	payload->len = buf[Will_Payload_position] << 8 | buf[Will_Payload_position+1];
	payload->ptr = (char *)&(buf[Will_Payload_position]) + 2;
	ESP_LOGD("_mg_mqtt_parse_header", "payload->len=%d payload->ptr=[%.*s]", payload->len, payload->len, payload->ptr);
	return 1;
}

int _mg_mqtt_status() {
	for (struct client *client = s_clients; client != NULL; client = client->next) {
		ESP_LOGI(pcTaskGetTaskName(NULL), "CLIENT(ALL) %p [%.*s]", client->c->fd, (int) client->cid.len, client->cid.ptr);
		for (struct will *will = s_wills; will != NULL; will = will->next) {
			if (client->c != will->c) continue;
			ESP_LOGI(pcTaskGetTaskName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d", 
			will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			if (client->c != sub->c) continue;
			ESP_LOGI(pcTaskGetTaskName(NULL), "SUB(ALL) %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		}
	}

#if 0
	for (struct will *will = s_wills; will != NULL; will = will->next) {
		ESP_LOGI(pcTaskGetTaskName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d", 
		will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
	}
#endif

#if 0
	for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		ESP_LOGI(pcTaskGetTaskName(NULL), "SUB(ALL) %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
	}
#endif

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
  /*if (ev != 1){
	  ESP_LOGE(pcTaskGetTaskName(NULL),"cmd %d", ev);
  }*/
  
  if (ev == MG_EV_MQTT_CMD) {
	struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
	ESP_LOGD(pcTaskGetTaskName(NULL),"cmd %d qos %d", mm->cmd, mm->qos);
	switch (mm->cmd) {
	  case MQTT_CMD_CONNECT: {
		//printf("Nuevo cargador conectado, \n\tram total = %i\n\tram interna = %i \n", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
		//ESP_LOGE(pcTaskGetTaskName(NULL), "CONNECT");

		// Parse the header to retrieve will information.
		//_mg_mqtt_dump("CONNECT", mm);
		struct mg_str cid;
		struct mg_str topic;
		struct mg_str payload;
		uint8_t qos;
		uint8_t retain;
		int willFlag = _mg_mqtt_parse_header(mm, &cid, &topic, &payload, &qos, &retain);
		//ESP_LOGE(pcTaskGetTaskName(NULL), "cid=[%.*s] willFlag=%d", cid.len, cid.ptr, willFlag);

		// Client connects. Add to the client-id list
		struct client *client = calloc(1, sizeof(*client));
		client->c = c;
		client->cid = mg_strdup(cid);
		LIST_ADD_HEAD(struct client, &s_clients, client);
		//ESP_LOGE(pcTaskGetTaskName(NULL), "CLIENT ADD %p [%.*s]", c->fd, (int) client->cid.len, client->cid.ptr);
#if 0
		for (struct client *client = s_clients; client != NULL; client = client->next) {
			ESP_LOGI(pcTaskGetTaskName(NULL), "CLIENT(ALL) %p [%.*s]", client->c->fd, (int) client->cid.len, client->cid.ptr);
		}
#endif

		// Client connects. Add to the will list
		if (willFlag == 1) {
		  struct will *will = calloc(1, sizeof(*will));
		  will->c = c;
		  will->topic = mg_strdup(topic);
		  will->payload = mg_strdup(payload);
		  will->qos = qos;
		  will->retain = retain;
		  LIST_ADD_HEAD(struct will, &s_wills, will);
		  //ESP_LOGE(pcTaskGetTaskName(NULL), "WILL ADD %p [%.*s] [%.*s] %d %d", 
			//c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
		_mg_mqtt_status();
#if 0
		for (struct will *will = s_wills; will != NULL; will = will->next) {
			ESP_LOGI(pcTaskGetTaskName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d", 
			will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
#endif

		// Client connects. Return success, do not check user/password
		uint8_t response[] = {0, 0};
		mg_mqtt_send_header(c, MQTT_CMD_CONNACK, 0, sizeof(response));
		mg_send(c, response, sizeof(response));
		//printf("Nuevo cargador almacenado, \n\tram total = %i\n\tram interna = %i\n", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
		break;
	  }
	  case MQTT_CMD_SUBSCRIBE: {
		//printf("Nueva subscripcion, \n\tram total = %i\n\tram interna = %i\n", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
		// Client subscribe. Add to the subscription list
		//ESP_LOGE(pcTaskGetTaskName(NULL), "MQTT_CMD_SUBSCRIBE");
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
		  //ESP_LOGE(pcTaskGetTaskName(NULL), "SUB ADD %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);

#if 0
		  for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			ESP_LOGI(pcTaskGetTaskName(NULL), "SUB[a] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		  }
#endif
		}
		_mg_mqtt_status();
		//printf("Nueva subscripcion guardad, \n\tram total = %i\n\tram interna = %i\n", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
		break;
	  }
	  case MQTT_CMD_UNSUBSCRIBE: {
		// Client unsubscribes. Remove from the subscription list
		//ESP_LOGE(pcTaskGetTaskName(NULL), "MQTT_CMD_UNSUBSCRIBE");
		//_mg_mqtt_dump("UNSUBSCRIBE", mm);
		int pos = 4;  // Initial topic offset, where ID ends
		struct mg_str topic;
		while ((pos = mg_mqtt_next_unsub(mm, &topic, pos)) > 0) {
		  //ESP_LOGE(pcTaskGetTaskName(NULL), "UNSUB %p [%.*s]", c->fd, (int) topic.len, topic.ptr);
		  // Remove from the subscription list
		  for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			ESP_LOGI(pcTaskGetTaskName(NULL), "SUB[b] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		  }
		  for (struct sub *next, *sub = s_subs; sub != NULL; sub = next) {
			next = sub->next;
			//ESP_LOGE(pcTaskGetTaskName(NULL), "c->fd=%p sub->c->fd=%p", c->fd, sub->c->fd);
			if (c != sub->c) continue;
			if (strncmp(topic.ptr, sub->topic.ptr, topic.len) != 0) continue;
			ESP_LOGI(pcTaskGetTaskName(NULL), "DELETE SUB %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);
			LIST_DELETE(struct sub, &s_subs, sub);
		  }
		  for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
			ESP_LOGI(pcTaskGetTaskName(NULL), "SUB[a] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		  }
		}
		_mg_mqtt_status();
		break;
	  }
	  case MQTT_CMD_PUBLISH: {
		// Client published message. Push to all subscribed channels
		//ESP_LOGE(pcTaskGetTaskName(NULL), "PUB %p [%.*s] -> [%.*s]", c->fd, (int) mm->data.len,
					  //mm->data.ptr, (int) mm->topic.len, mm->topic.ptr);
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		  //if (mg_strcmp(mm->topic, sub->topic) != 0) continue;
		  if (_mg_strcmp(mm->topic, sub->topic) != 0) continue;
		  mg_mqtt_pub(sub->c, &mm->topic, &mm->data);
		}
		break;
	  }
	  case MQTT_CMD_PINGREQ: {
		//ESP_LOGE(pcTaskGetTaskName(NULL), "PINGREQ %p", c->fd);
		mg_mqtt_pong(c); // Send PINGRESP
		break;
	  }
	}
  } else if (ev == MG_EV_CLOSE) {
	printf("Cargador desconectado, \n\tram total = %i\n\tram interna = %i\n", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
		
	ESP_LOGI(pcTaskGetTaskName(NULL), "MG_EV_CLOSE %p", c->fd);
	// Client disconnects. Remove from the client-id list
	for (struct client *client = s_clients; client != NULL; client = client->next) {
		//ESP_LOGE(pcTaskGetTaskName(NULL), "CLIENT(b) %p [%.*s]", client->c->fd, (int) client->cid.len, client->cid.ptr);
	}
	for (struct client *next, *client = s_clients; client != NULL; client = next) {
		next = client->next;
		//ESP_LOGE(pcTaskGetTaskName(NULL), "c->fd=%p client->c->fd=%p", c->fd, client->c->fd);
		if (c != client->c) continue;
		//ESP_LOGE(pcTaskGetTaskName(NULL), "CLIENT DEL %p [%.*s]", c->fd, (int) client->cid.len, client->cid.ptr);
		LIST_DELETE(struct client, &s_clients, client);
	}
	for (struct client *client = s_clients; client != NULL; client = client->next) {
		//ESP_LOGE(pcTaskGetTaskName(NULL), "CLIENT(a) %p [%.*s]", client->c->fd, (int) client->cid.len, client->cid.ptr);
	}

	// Client disconnects. Remove from the subscription list
	for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		//ESP_LOGE(pcTaskGetTaskName(NULL), "SUB[b] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
	}
	for (struct sub *next, *sub = s_subs; sub != NULL; sub = next) {
		next = sub->next;
		//ESP_LOGE(pcTaskGetTaskName(NULL), "c->fd=%p sub->c->fd=%p", c->fd, sub->c->fd);
		if (c != sub->c) continue;
		//ESP_LOGE(pcTaskGetTaskName(NULL), "SUB DEL %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr);
		LIST_DELETE(struct sub, &s_subs, sub);
	}

	// Judgment to send will
	for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		//ESP_LOGE(pcTaskGetTaskName(NULL), "SUB[a] %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
		for (struct will *will = s_wills; will != NULL; will = will->next) {
			//ESP_LOGE(pcTaskGetTaskName(NULL), "WILL(ALL) %p [%.*s] [%.*s] %d %d", 
				//will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
			//if (c == will->c) continue;
			if (sub->c == will->c) continue;
			//ESP_LOGE(pcTaskGetTaskName(NULL), "WILL(CMP) %p [%.*s] [%.*s] %d %d", 
				//will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
			if (_mg_strcmp(will->topic, sub->topic) != 0) continue;
			mg_mqtt_pub(sub->c, &will->topic, &will->payload);
		}
	}

	// Client disconnects. Remove from the will list
	for (struct will *will = s_wills; will != NULL; will = will->next) {
		//ESP_LOGE(pcTaskGetTaskName(NULL), "WILL[b] %p [%.*s] [%.*s] %d %d", 
			//will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
	}
	for (struct will *next, *will = s_wills; will != NULL; will = next) {
		next = will->next;
		//ESP_LOGE(pcTaskGetTaskName(NULL), "WILL %p [%.*s] [%.*s] %d %d", 
			//c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		if (c != will->c) continue;
		//ESP_LOGE(pcTaskGetTaskName(NULL), "WILL DEL %p [%.*s] [%.*s] %d %d", 
			//c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
		LIST_DELETE(struct will, &s_wills, will);
	}
	for (struct will *will = s_wills; will != NULL; will = will->next) {
		//ESP_LOGE(pcTaskGetTaskName(NULL), "WILL[a] %p [%.*s] [%.*s] %d %d", 
			//will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
	}
	_mg_mqtt_status();
	//printf("Nuevo cargador eliminado, \n\tram total = %i\n\tram interna = %i ", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
		
  }
  (void) fn_data;
}

void mqtt_server(void *pvParameters){

	mg_log_set("1"); // Set to log level to LL_ERROR

	struct mg_mgr mgr;
	mg_mgr_init(&mgr);
	mg_mqtt_listen(&mgr, s_listen_on, fn, NULL);  // Create MQTT listener

	/* Processing events */
	while (1) {
		mg_mgr_poll(&mgr, 0);
		vTaskDelay(1);
		if(StopMQTT){
			break;
		}
	}

	// Never reach here
	ESP_LOGI(pcTaskGetTaskName(NULL), "finish");
	mg_mgr_free(&mgr);
	vTaskDelete(NULL);
}

//Controlar que equipos siguen con vida y cuales no
static void Ping_Control(char* Data){
	/*for(int i =0; i < net_group.size;i++){
		if(memcmp(net_group.charger_table[i].name, Desencriptado.c_str(), 9)==0){
			//si ya lo tenemos en la lista pero nos envía una llamada, es que se ha reiniciado, le hacemos entrar en el grupo
			if(ChargingGroup.GroupMaster){
				AsyncUDPMessage mensaje (13);
				mensaje.write((uint8_t*)(Encipher("Start client").c_str()), 13);
				udp.sendTo(mensaje,packet.remoteIP(),1234);
			}
			return;
		}
	}*/

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
			}
			else if(!memcmp(mm->topic.ptr, "Pong", mm->topic.len)){
				xStart = xTaskGetTickCount();
			}
			else if(!memcmp(mm->topic.ptr, "Params", mm->topic.len)){
				New_Params(mm->data.ptr, mm->data.len);
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
	//struct mg_connection *mgc = (struct mg_connection*)params;
	while (1) {
		mg_mgr_poll(&mgr, 5);
		vTaskDelay(1);
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

	if(PollerHandle != NULL){
		return false;
	}
	/* Arrancar la conexion*/	
	mg_mgr_init(&mgr);
	struct mg_mqtt_opts opts;  // MQTT connection options

	memset(&opts, 0, sizeof(opts));					// Set MQTT options
	opts.client_id = mg_str(pub_opts->Client_ID);   // Set Client ID
	opts.qos = 1;									// Set QoS to 1
	opts.will_topic = mg_str(pub_opts->Will_Topic);			// Set last will topic
	opts.will_message = mg_str(pub_opts->Will_Message);			// And last will message

	mgc = mg_mqtt_connect(&mgr, pub_opts->url, &opts, publisher_fn, &pub_opts->url);	// Create client connection
	
	PollerHandle = xTaskCreateStatic(mqtt_polling,"POLLER",1024*6,NULL,2,xPOLLstack,&xPOLLBuffer); 

	return true;
}

void mqtt_publish(char* Topic, char* Data){
	struct mg_str topic = mg_str(Topic);
	struct mg_str data = mg_str(Data);
	mg_mqtt_pub(mgc, &topic, &data);
}

void mqtt_subscribe(char* Topic){
	struct mg_str topic = mg_str(Topic);
	mg_mqtt_sub(mgc, &topic);							
}
