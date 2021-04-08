#include "MQTT_servidor.h"
#include "ESPmDNS.h"
#include "AsyncUDP.h"

AsyncUDP udp;
void mqtt_server(void *pvParameters);
void start_MQTT_client(IPAddress remoteIP)

String Decipher(String input);
String Encipher(String input);

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;

carac_group ChargingGroup;

static StackType_t xSERVERStack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xSERVERBuffer ;

typedef struct{
    carac_charger charger_table[10];
    int size = 0;
}carac_chargers;

carac_chargers net_group;
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
            char buffer[size+1] ;
            memcpy(buffer, packet.data(), size);
            buffer[size] = '\0';

            String Desencriptado;
            Desencriptado = Decipher(String(buffer));

            if(size<=8){
                if(packet.isBroadcast()){                   
                    packet.print(Encipher(String(ConfigFirebase.Device_Id)).c_str());
                }

                for(int i =0; i < net_group.size;i++){
                    if(memcmp(net_group.charger_table[i].name, Desencriptado.c_str(), 9)==0){
                        //si ya lo tenemos en la lista pero nos envía una llamada, es que se ha reiniciado, le hacemos entrar en el grupo
                        if(ChargingGroup.GroupMaster){
                            udp.sendTo(Encipher("Start client").c_str(),packet.remoteIP(),1234);
                        }
                        return;
                    }
                }
                
                Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista\n", Desencriptado.c_str(), packet.remoteIP().toString().c_str());

                net_group.charger_table[net_group.size].IP = packet.remoteIP();
                net_group.size++;              

                for(int i =0; i< net_group.size;i++){                  
                    Serial.printf("%s --> %s\n",String(net_group.charger_table[i].name).c_str(), ip4addr_ntoa(&net_group.charger_table[i].IP));
                }
            }
            else{
                if(!memcmp(Desencriptado.c_str(), "Start client", 13)){
                    start_MQTT_client(packet.remoteIP());
                }
            }            
        });
    }

    //Avisar al resto de equipos de que estamos aqui!
    udp.broadcast(Encipher(String(ConfigFirebase.Device_Id)).c_str());
    delay(2000);
    start_MQTT_server();
}

void Publisher(void* args){


    while(1){
        mqtt_publish("Topic", "9A");
        delay(500);
    }
}

void start_MQTT_server(){
    
    ChargingGroup.GroupMaster = true;
    ChargingGroup.GroupActive = true;
    udp.broadcast((char*)ConfigFirebase.Device_Id);
    delay(250);
    
	/* Start MQTT Server using tcp transport */
    xTaskCreateStatic(mqtt_server,"BROKER",1024*6,NULL,PRIORIDAD_MQTT,xSERVERStack,&xSERVERBuffer); 
	delay(10);	// You need to wait until the task launch is complete.
    Serial.println("Servidor creado, avisando al resto de equipos");

    udp.broadcast(Encipher("Start client").c_str());
    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP1));
    
    memcpy(publisher.Client_ID,ConfigFirebase.Device_Id, 8);
    publisher.Will_Topic = "NODE_DEAD";
    memcpy(publisher.Will_Message,ConfigFirebase.Device_Id, 8);

    if(mqtt_connect(&publisher)){
        mqtt_subscribe("Topic");
        xTaskCreate(Publisher,"Publisger",1024,NULL,2,NULL);
    }
}

void start_MQTT_client(IPAddress remoteIP){

    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", remoteIP.toString());
    strcpy(publisher.Client_ID,ConfigFirebase.Device_Id);
    publisher.Will_Topic = "NODE_DEAD";
    strcpy(publisher.Will_Message,ConfigFirebase.Device_Id);

    if(mqtt_connect(&publisher)){
        mqtt_subscribe("Topic");
        mqtt_publish("Topic", "9A");
        xTaskCreate(Publisher,"Publisher",1024,NULL,2,NULL);
    }
}
