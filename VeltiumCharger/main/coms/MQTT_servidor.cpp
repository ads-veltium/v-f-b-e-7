#include "MQTT_servidor.h"

void mqtt_server(void *pvParameters);

extern carac_Coms  Coms;

void start_MQTT_server()
{

	/* Start MQTT Server using tcp transport */
	xTaskCreate(mqtt_server, "BROKER", 1024*4, NULL, 2, NULL);
	vTaskDelay(10);	// You need to wait until the task launch is complete.
    Serial.println("Servidor creado");

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

    mqtt_sub_pub_opts publisher2;

	/* Start Publisher */
	sprintf(publisher2.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP1));
    publisher2.Client_ID = "VCD12345";
    publisher2.Will_Topic = "NODE_DEAD";
    publisher2.Will_Message = "VCD12345";
    publisher2.Pub_Sub_Topic = "Koxka2";
    publisher2.Topic_Message = "Elur2";

	xTaskCreate(mqtt_publisher, "PUBLISH2", 1024*4, &publisher2, 2, NULL);
	vTaskDelay(500);	// You need to wait until the task launch is complete.

    mqtt_sub_pub_opts subscriber;

	/* Start Publisher */
	sprintf(subscriber.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP1));
    subscriber.Client_ID = "VCD12345";
    subscriber.Will_Topic = "NODE_DEAD";
    subscriber.Will_Message = "VCD12345";
    subscriber.Pub_Sub_Topic = "Koxka";
    subscriber.Topic_Message = "Elur";

	xTaskCreate(mqtt_subscriber, "Susbcriber", 1024*4, &subscriber, 2, NULL);
	vTaskDelay(500);	// You need to wait until the task launch is complete.

}

