#include "MQTT_servidor.h"
#include "ESPmDNS.h"
#include "AsyncUDP.h"

AsyncUDP udp;
void mqtt_server(void *pvParameters);

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;

static StackType_t xSERVERStack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xSERVERBuffer ;

static StackType_t xPUBStack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xPUBBuffer ;

typedef struct{
    carac_charger charger_table[10];
    int size = 0;
}carac_chargers;

carac_chargers group;

void stop_MQTT(){
    SetStopMQTT(true);
    delay(1000);
    SetStopMQTT(false);
}

void send_alive(){
    //Arrancar el servidor udp para escuchar cuando el maestro nos lo ordene
    if(udp.listen(1234)) {
        udp.onPacket([](AsyncUDPPacket packet) {         
            int size = packet.length();
            if(size<=8){
                if(packet.isBroadcast()){
                    packet.print((char*)ConfigFirebase.Device_Id);
                }
                
                char buffer[size] ;
                memcpy(buffer, packet.data(), size);

                for(int i =0; i < group.size;i++){
                    if(memcmp(group.charger_table[i].name, buffer, size)==0){
                        return;
                    }
                }
                Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista\n", buffer, packet.remoteIP().toString().c_str());

                ip4addr_aton(packet.remoteIP().toString().c_str(),&group.charger_table[group.size].IP);
                memcpy(group.charger_table[group.size].name, buffer, size);
                group.size++;              

                for(int i =0; i< group.size;i++){                  
                    Serial.printf("%s --> %s\n",group.charger_table[i].name, ip4addr_ntoa(&group.charger_table[i].IP));
                }
            }            
        });
    }

    //Avisar al resto de equipos de que estamos aqui!
    udp.broadcast((char*)ConfigFirebase.Device_Id);
}

void start_MQTT_server()
{
    send_alive();

	/* Start MQTT Server using tcp transport */
    xTaskCreateStatic(mqtt_server,"BROKER",1024*6,NULL,PRIORIDAD_MQTT,xSERVERStack,&xSERVERBuffer); 
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

    xTaskCreateStatic(mqtt_publisher,"PUBLISH",1024*6,&publisher,PRIORIDAD_MQTT,xPUBStack,&xPUBBuffer); 
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

