#include "../control.h"
#include "AsyncUDP.h"

extern "C" {
#include "mongoose.h"
#include "mqtt_server.h"
}
bool check_in_group(const char* ID, carac_chargers* group);
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);

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

//Prototipos de funciones externas
void mqtt_server(void *pvParameters);

//Prototipos de funciones internas
void start_MQTT_server();
void start_MQTT_client(IPAddress remoteIP);

void stop_MQTT(){
    ChargingGroup.GroupActive = false;
    delay(500);
    SetStopMQTT(true);
    delay(1000);
    SetStopMQTT(false);
}


//Añadir un cargador a un equipo
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group){
    if(group->size < MAX_GROUP_SIZE){
        if(!check_in_group(ID,group)){
            memcpy(group->charger_table[group->size].name, ID, 9);
            group->charger_table[group->size].IP = IP;
            group->size++;
            return true;
        }
    }
    printf("Error al añadir al grupo \n");
    return false;
}

//comprobar si un cargador está almacenado en las tablas de equipos
bool check_in_group(const char* ID, carac_chargers* group){
    for(int j=0;j < group->size;j++){
        if(!memcmp(ID,  group->charger_table[j].name,8)){
            return true;
        }
    }
    return false;
}

//Enviar mensajes solo a los miembros del grupo
void broadcast_a_grupo(char* Mensaje){
    AsyncUDPMessage mensaje (13);
    mensaje.write((uint8_t*)(Encipher(Mensaje).c_str()), 13);

    //QUITAR!!!!!!!!!!!!!!!!!
    memcpy(ChargingGroup.group_chargers.charger_table[0].name, "FT63D732",8);
    memcpy(ChargingGroup.group_chargers.charger_table[1].name, "31B70630",8);
    memcpy(ChargingGroup.group_chargers.charger_table[2].name, "1SDVD734",8);
    memcpy(ChargingGroup.group_chargers.charger_table[3].name, "J4D9M1FT",8);
    memcpy(ChargingGroup.group_chargers.charger_table[4].name, "626965F5",8);
    //memcpy(ChargingGroup.group_chargers.charger_table[5].name, "J3P10DNR",8);

    ChargingGroup.group_chargers.size = 5;

    for(int i =0; i < net_group.size;i++){
        for(int j=0;j < ChargingGroup.group_chargers.size;j++){
            if(check_in_group(net_group.charger_table[i].name, &ChargingGroup.group_chargers)){
                udp.sendTo(mensaje, net_group.charger_table[i].IP,1234);
                ChargingGroup.group_chargers.charger_table[j].IP = net_group.charger_table[i].IP; 
            }
        }
    }
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
          
                if(check_in_group(Desencriptado.c_str(), &net_group)){
                    //si ya lo tenemos en la lista pero nos envía una llamada, es que se ha reiniciado, comprobamos si está en el grupo
                    if(ChargingGroup.GroupMaster){
                        if(check_in_group(Desencriptado.c_str(), &ChargingGroup.group_chargers)){
                            Serial.printf("El cargador VCD%s está en el grupo de carga\n", Desencriptado.c_str());  
                            AsyncUDPMessage mensaje (13);
                            mensaje.write((uint8_t*)(Encipher("Start client").c_str()), 13);
                            udp.sendTo(mensaje,packet.remoteIP(),1234);
                        }
                    }
                    return;
                } 

                Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista\n", Desencriptado.c_str(), packet.remoteIP().toString().c_str());  
                add_to_group(Desencriptado.c_str(), packet.remoteIP(), &net_group);
            }
            else{
                if(!memcmp(Desencriptado.c_str(), "Start client", 13)){
                    if(!ChargingGroup.GroupActive && !ChargingGroup.GroupMaster){
                        printf("Soy parte de un grupo !!\n");
                        start_MQTT_client(packet.remoteIP());
                    }
                }
            }            
        });
    }

    //Avisar al resto de equipos de que estamos aqui!
    udp.broadcast(Encipher(String(ConfigFirebase.Device_Id)).c_str());
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
        if(!ChargingGroup.GroupMaster){ //Avisar al maestro de que seguimos aqui
            mqtt_publish("Ping", ConfigFirebase.Device_Id);
        }
        if(!ChargingGroup.GroupActive){
            break;
        }
    }
    vTaskDelete(NULL);
}

void start_MQTT_server(){
    Serial.println("Arrancando servidor MQTT");
    ChargingGroup.GroupActive = true;
    
	/* Start MQTT Server using tcp transport */
    xTaskCreateStatic(mqtt_server,"BROKER",1024*6,NULL,PRIORIDAD_MQTT,xSERVERStack,&xSERVERBuffer); 
	delay(500);

    broadcast_a_grupo("Start client");
    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP));
    memcpy(publisher.Client_ID,ConfigFirebase.Device_Id, 8);
    memcpy(publisher.Will_Message,ConfigFirebase.Device_Id, 8);
    publisher.Will_Topic = "NODE_DEAD";

    if(mqtt_connect(&publisher)){
        mqtt_subscribe("Device_Status");
        mqtt_subscribe("Ping");
        xTaskCreate(Publisher,"Publisher",4096,NULL,2,NULL);
    }
}

void start_MQTT_client(IPAddress remoteIP){
    ChargingGroup.GroupActive = true;
    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", remoteIP.toString().c_str());
    strcpy(publisher.Client_ID,ConfigFirebase.Device_Id);
    strcpy(publisher.Will_Message,ConfigFirebase.Device_Id);
    publisher.Will_Topic = "NODE_DEAD";

    if(mqtt_connect(&publisher)){
        mqtt_subscribe("Device_Status");
        xTaskCreate(Publisher,"Publisher",4096,NULL,2,NULL);
    }
}
