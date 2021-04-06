#include "MQTT_servidor.h"
#include "ESPmDNS.h"
#include "AsyncUDP.h"

AsyncUDP udp;
void mqtt_server(void *pvParameters);

extern carac_Coms  Coms;

void stop_MQTT(){
    SetStopMQTT(true);
    delay(1000);
    SetStopMQTT(false);
}

void send_alive(){
    //Arrancar el servidor udp para escuchar cuando el maestro nos lo ordene
    if(udp.listen(1234)) {
        udp.onPacket([](AsyncUDPPacket packet) {
            Serial.print("UDP Packet Type: ");
            Serial.print(packet.isBroadcast()?"Broadcast":packet.isMulticast()?"Multicast":"Unicast");
            Serial.print(", From: ");
            Serial.print(packet.remoteIP());
            Serial.print(":");
            Serial.print(packet.remotePort());
            Serial.print(", To: ");
            Serial.print(packet.localIP());
            Serial.print(":");
            Serial.print(packet.localPort());
            Serial.print(", Length: ");
            Serial.print(packet.length());
            Serial.print(", Data: ");
            Serial.write(packet.data(), packet.length());
            Serial.println();
            //reply to the client
            packet.printf("Got %u bytes of data", packet.length());
        });
    }

    //Avisar al resto de equipos de que estamos aqui!
    udp.broadcast("Anyone here?");
}

void start_MQTT_server()
{
    send_alive();

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
}

