#include "MQTT_servidor.h"
#include "ESPmDNS.h"
#include "AsyncUDP.h"

AsyncUDP udp;
void mqtt_server(void *pvParameters);
void start_MQTT_client();
String Decipher(String input);
String Encipher(String input);

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;

static StackType_t xSERVERStack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xSERVERBuffer ;

static StackType_t xPUBStack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xPUBBuffer ;

static StackType_t xPUBStack2 [1024*6]     EXT_RAM_ATTR;
StaticTask_t xPUBBuffer2 ;

typedef struct{
    carac_charger charger_table[10];
    int size = 0;
}carac_chargers;

carac_chargers group;



void stop_MQTT(){
    udp.close();
    SetStopMQTT(true);
    delay(1000);
    SetStopMQTT(false);
}

void start_udp(){
    //Arrancar el servidor udp para escuchar cuando el maestro nos lo ordene
    if(udp.listen(1234)) {
        udp.onPacket([](AsyncUDPPacket packet) {         
            int size = packet.length();
            char buffer[size] ;
            memcpy(buffer, packet.data(), size);

            String Desencriptado;
            Desencriptado = Decipher(String(buffer));

            if(size<=8){
                if(packet.isBroadcast()){                   
                    packet.print(Encipher(String(ConfigFirebase.Device_Id)).c_str());
                }

                for(int i =0; i < group.size;i++){
                    if(memcmp(group.charger_table[i].name, Desencriptado.c_str(), size)==0){
                        return;
                    }
                }
                Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista\n", Desencriptado.c_str(), packet.remoteIP().toString().c_str());

                ip4addr_aton(packet.remoteIP().toString().c_str(),&group.charger_table[group.size].IP);
                memcpy(group.charger_table[group.size].name, Desencriptado.c_str(), size);
                group.size++;              

                for(int i =0; i< group.size;i++){                  
                    Serial.printf("%s --> %s\n",String(group.charger_table[i].name).c_str(), ip4addr_ntoa(&group.charger_table[i].IP));
                }
            }
            else{
                if(!strcmp(Desencriptado.c_str(), "Start client")){
                    start_MQTT_client();
                }
            }            
        });
    }

    //Avisar al resto de equipos de que estamos aqui!
    udp.broadcast(Encipher(String(ConfigFirebase.Device_Id)).c_str());
}

void start_MQTT_server(){

    udp.broadcast((char*)ConfigFirebase.Device_Id);
    delay(250);

    udp.broadcast(Encipher("Start client").c_str());

	/* Start MQTT Server using tcp transport */
    xTaskCreateStatic(mqtt_server,"BROKER",1024*6,NULL,PRIORIDAD_MQTT,xSERVERStack,&xSERVERBuffer); 
	delay(10);	// You need to wait until the task launch is complete.
    Serial.println("Servidor creado, avisando al resto de equipos");

    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP1));
    publisher.Client_ID = "VCD12345";
    publisher.Will_Topic = "NODE_DEAD";
    publisher.Will_Message = "VCD12345";

    mqtt_connect(&publisher);

    struct mg_str topic = mg_str("Koxka");
	struct mg_str data = mg_str("Elur");

    mqtt_subscribe("Lada");

    xTaskCreateStatic(mqtt_publisher,"PUBLISH",1024*6,NULL,PRIORIDAD_MQTT,xPUBStack,&xPUBBuffer); 
}

void start_MQTT_client(){
    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP1));
    strcpy(publisher.Client_ID,ConfigFirebase.Device_Id);
    publisher.Will_Topic = "NODE_DEAD";
    strcpy(publisher.Will_Message,ConfigFirebase.Device_Id);

    mqtt_connect(&publisher);

    mqtt_subscribe("Lada");
}
