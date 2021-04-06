#include "MQTT_servidor.h"

void mqtt_server(void *pvParameters);

extern carac_Coms  Coms;

void start_MQTT_server()
{

	/* Start MQTT Server using tcp transport */
	xTaskCreate(mqtt_server, "BROKER", 1024*4, NULL, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.
    Serial.println("Servidor creado");

#if CONFIG_SUBSCRIBE
	/* Start Subscriber */
	char cparam1[64];
	sprintf(cparam1, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP1));
	xTaskCreate(mqtt_subscriber, "SUBSCRIBE", 1024*4, (void *)cparam1, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.
#endif

    mqtt_sub_pub_opts publisher;

	/* Start Publisher */
	sprintf(publisher.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP1));
    publisher.Client_ID = "VCD12345";
    publisher.Will_Topic = "NODE_DEAD";
    publisher.Will_Message = "VCD12345";
    publisher.Pub_Sub_Topic = "Koxka";
    publisher.Topic_Message = "Elur";

	xTaskCreate(mqtt_publisher, "PUBLISH", 1024*4, &publisher, 2, NULL);
	vTaskDelay(500);	// You need to wait until the task launch is complete.

}

