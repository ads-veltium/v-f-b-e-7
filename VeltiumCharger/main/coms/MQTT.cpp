#include "../control.h"
#include "AsyncUDP.h"

extern "C" {
#include "mongoose.h"
#include "mqtt_server.h"
}

AsyncUDP udp EXT_RAM_ATTR;


String Decipher(String input);
String Encipher(String input);

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Params Params;
extern carac_group  ChargingGroup;

static StackType_t xSERVERStack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xSERVERBuffer ;

carac_chargers net_group;
carac_chargers group;

//Prototipos de funciones externas
void mqtt_server(void *pvParameters);

//Prototipos de funciones internas
void start_MQTT_server();
void start_MQTT_client(IPAddress remoteIP);


void stop_MQTT(){
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
                            AsyncUDPMessage mensaje (13);
                            mensaje.write((uint8_t*)(Encipher("Start client").c_str()), 13);
                            udp.sendTo(mensaje,packet.remoteIP(),1234);
                        }
                        return;
                    }
                }
                
                Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista\n", Desencriptado.c_str(), packet.remoteIP().toString().c_str());
                memcpy(net_group.charger_table[net_group.size].name, Desencriptado.c_str(), 9);
                net_group.charger_table[net_group.size].IP = packet.remoteIP();
                net_group.size++;              

                for(int i =0; i< net_group.size;i++){                  
                    Serial.printf("%s --> %s\n",String(net_group.charger_table[i].name).c_str(), net_group.charger_table[i].IP.toString().c_str());
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


/*Tarea para publicar los datos del equipo cada segundo*/
void Publisher(void* args){
    char TopicName[14] = "Device_Status";
    char buffer[250];
    Params.Fase = 1;
    while(1){
        if(ChargingGroup.SendNewData){
            //Preparar data
            sprintf(buffer, "%s%i%s%i", ConfigFirebase.Device_Id,Params.Fase,Status.HPT_status,Status.Measures.instant_voltage);
            
            mqtt_publish(TopicName, (buffer));
            ChargingGroup.SendNewData = false;

        }

        delay(1000);
    }
    vTaskDelete(NULL);
}

void start_MQTT_server(){
    
    ChargingGroup.GroupMaster = true;
    ChargingGroup.GroupActive = true;
    
	/* Start MQTT Server using tcp transport */
    xTaskCreateStatic(mqtt_server,"BROKER",1024*6,NULL,PRIORIDAD_MQTT,xSERVERStack,&xSERVERBuffer); 
	delay(500);	// You need to wait until the task launch is complete.
    Serial.println("Servidor creado, avisando al resto de equipos");

    udp.broadcast(Encipher("Start client").c_str());
    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP1));
    
    memcpy(publisher.Client_ID,ConfigFirebase.Device_Id, 8);
    publisher.Will_Topic = "NODE_DEAD";
    memcpy(publisher.Will_Message,ConfigFirebase.Device_Id, 8);

    if(mqtt_connect(&publisher)){
        mqtt_subscribe("Device_Status");
        xTaskCreate(Publisher,"Publisger",4096,NULL,2,NULL);
    }
}

void start_MQTT_client(IPAddress remoteIP){
    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", remoteIP.toString().c_str());
    strcpy(publisher.Client_ID,ConfigFirebase.Device_Id);
    publisher.Will_Topic = "NODE_DEAD";
    strcpy(publisher.Will_Message,ConfigFirebase.Device_Id);

    if(mqtt_connect(&publisher)){
        mqtt_subscribe("Device_Status");
        xTaskCreate(Publisher,"Publisher",4096,NULL,2,NULL);
    }
}
