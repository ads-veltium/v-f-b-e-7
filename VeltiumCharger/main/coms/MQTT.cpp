#include "../control.h"
#include "AsyncUDP.h"

extern "C" {
#include "mongoose.h"
#include "mqtt_server.h"
}
uint8_t check_in_group(const char* ID, carac_chargers* group);
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);

AsyncUDP udp EXT_RAM_ATTR;

String Decipher(String input);
String Encipher(String input);

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Params Params;
extern carac_group  ChargingGroup;
extern carac_Comands  Comands ;

static StackType_t xSERVERStack [1024*6]     EXT_RAM_ATTR;
StaticTask_t xSERVERBuffer ;

static StackType_t xPUBLISHERStack [4096]     EXT_RAM_ATTR;
StaticTask_t xPUBLISHERBuffer ;

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
        if(check_in_group(ID,group)==255){
            memcpy(group->charger_table[group->size].name, ID, 8);
            group->charger_table[group->size].name[8]='\0';
            group->charger_table[group->size].IP = IP;
            group->size++;
            return true;
        }
    }
    printf("Error al añadir al grupo \n");
    return false;
}

//comprobar si un cargador está almacenado en las tablas de equipos
uint8_t check_in_group(const char* ID, carac_chargers* group){
    for(int j=0;j < group->size;j++){
        if(!memcmp(ID,  group->charger_table[j].name,8)){
            return j;
        }
    }
    return 255;
}

//Eliminar un cargador de un grupo
bool remove_from_group(const char* ID ,carac_chargers* group){

    for(int j=0;j < group->size;j++){
        if(!memcmp(ID,  group->charger_table[j].name,8)){
            if(j!=group->size-1){ //si no es el ultimo debemos shiftear la lista
                for(uint8_t i=j;i < group->size-1;i++){
                    group->charger_table[i] = group->charger_table[i+1];
                }
            }
            group->charger_table[group->size-1].IP = INADDR_NONE;
            
            memset(group->charger_table[group->size-1].name,0,9);
            --group->size;
            printf("Cargador %s eliminado\n", ID);
            return true;
        }
    }
    printf("El cargador no está en el grupo!\n");
    return false;
}

//obtener la ip de un equipo dado su ID
IPAddress get_IP(const char* ID){
    for(int j=0;j < net_group.size;j++){
        if(!memcmp(ID,  net_group.charger_table[j].name,8)){
            return net_group.charger_table->IP;
        }
    }
    return INADDR_NONE;
}

//Enviar mensajes solo a los miembros del grupo
void broadcast_a_grupo(char* Mensaje){
    AsyncUDPMessage mensaje (13);
    mensaje.write((uint8_t*)(Encipher(Mensaje).c_str()), 13);

    //QUITAR!!!!!!!!!!!!!!!!!
    memcpy(ChargingGroup.group_chargers.charger_table[0].name, "31B70630",8);
    memcpy(ChargingGroup.group_chargers.charger_table[1].name, "626965F5",8);  
    //memcpy(ChargingGroup.group_chargers.charger_table[2].name, "1SDVD734",8);
    //memcpy(ChargingGroup.group_chargers.charger_table[4].name, "FT63D732",8);
    //memcpy(ChargingGroup.group_chargers.charger_table[5].name, "J3P10DNR",8);

    ChargingGroup.group_chargers.size = 2;

    for(int i =0; i < net_group.size;i++){
        for(int j=0;j < ChargingGroup.group_chargers.size;j++){
            if(check_in_group(net_group.charger_table[i].name, &ChargingGroup.group_chargers)<255){
                udp.sendTo(mensaje, net_group.charger_table[i].IP,1234);
                ChargingGroup.group_chargers.charger_table[j].IP = net_group.charger_table[i].IP; 
            }
        }
    }
    delay(500);
    ChargingGroup.SendNewParams = true;
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

                //Si el cargador está en el grupo de carga, le decimos que es un esclavo
                if(ChargingGroup.GroupMaster){
                    if(check_in_group(Desencriptado.c_str(), &ChargingGroup.group_chargers)!=255){
                        Serial.printf("El cargador VCD%s está en el grupo de carga\n", Desencriptado.c_str());  
                        AsyncUDPMessage mensaje (13);
                        mensaje.write((uint8_t*)(Encipher("Start client").c_str()), 13);
                        udp.sendTo(mensaje,packet.remoteIP(),1234);
                        delay(500);
                        ChargingGroup.SendNewParams = true;
                    }
                }
          
                if(check_in_group(Desencriptado.c_str(), &net_group)==255){
                    Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista de red\n", Desencriptado.c_str(), packet.remoteIP().toString().c_str());  
                    add_to_group(Desencriptado.c_str(), packet.remoteIP(), &net_group);
                } 
            }
            else{
                if(!memcmp(Desencriptado.c_str(), "Start client", 13)){
                    if(!ChargingGroup.GroupActive){
                        printf("Soy parte de un grupo !!\n");
                        start_MQTT_client(packet.remoteIP());
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Hay maestro?", 13)){
                    if(ChargingGroup.GroupActive && ChargingGroup.GroupMaster){
                        packet.print(Encipher(String("Bai, hemen nago")).c_str());
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Bai, hemen nago", 13)){
                    if(!ChargingGroup.GroupActive && ChargingGroup.GroupMaster){
                        ChargingGroup.GroupMaster = false;
                        SendToPSOC5(ChargingGroup.GroupMaster, GROUPS_GROUP_MASTER);
                    }
                }
            }            
        });
    }

    //Avisar al resto de equipos de que estamos aqui!
    udp.broadcast(Encipher(String(ConfigFirebase.Device_Id)).c_str());
}

/*Tarea de emergencia para cuando el maestro se queda muerto mucho tiempo*/
void MasterPanicTask(void *args){
    TickType_t xStart = xTaskGetTickCount();

    while(!ChargingGroup.GroupActive){
        if(pdTICKS_TO_MS(xTaskGetTickCount() - xStart) > 60000){ //si pasa 1 minuto, elegir un nuevo maestro
            Serial.println("Necesitamos un nuevo maestro!");

            if(!memcmp(ChargingGroup.group_chargers.charger_table[1].name,ConfigFirebase.Device_Id,8)){
                Serial.println("Soy el nuevo maestro!!");
                ChargingGroup.GroupMaster = true;
                SendToPSOC5(ChargingGroup.GroupMaster,GROUPS_GROUP_MASTER);
            }
            
            else{
                Serial.println("No soy el nuevo maestro, alguien se pondrá :)");
            }
            break;
        }
        delay(1000);
    }
    vTaskDelete(NULL);
}

/*Tarea para publicar los datos del equipo cada segundo*/
void Publisher(void* args){

    char buffer[100];
    Params.Fase = 1;
    while(1){
        if(ChargingGroup.SendNewData){           

            //si es trifasico, enviar informacion de todas las fases
            if(Status.Trifasico){
                sprintf(buffer, "%s1%s%i%i", ConfigFirebase.Device_Id,Status.HPT_status,Status.Measures.instant_current,Comands.desired_current -Status.Measures.instant_current);
                mqtt_publish("Device_Status", (buffer));
                delay(50);
                sprintf(buffer, "%s2%s%i%i", ConfigFirebase.Device_Id,Status.HPT_status,Status.MeasuresB.instant_current,Comands.desired_current -Status.MeasuresB.instant_current);
                mqtt_publish("Device_Status", (buffer));
                delay(50);
                sprintf(buffer, "%s3%s%i%i", ConfigFirebase.Device_Id,Status.HPT_status,Status.MeasuresC.instant_current,Comands.desired_current -Status.MeasuresC.instant_current);
                mqtt_publish("Device_Status", (buffer));
            }
            else{
                char Delta[2]={'0'};
                uint8_t DeltaInt = Comands.desired_current -Status.Measures.instant_current;
                if(DeltaInt >=10){
                    itoa(DeltaInt,&Delta[0],10);
                }
                else{
                    itoa(DeltaInt,&Delta[1],10);
                }

                sprintf(buffer, "%s%i%s%s%i", ConfigFirebase.Device_Id,Params.Fase,Status.HPT_status,Delta,Status.Measures.instant_voltage);  
                mqtt_publish("Device_Status", (buffer));
            }
            ChargingGroup.SendNewData = false;
        }
        if( ChargingGroup.SendNewParams){
            printf("Publicando nuevos parametros\n");
            for(int i=0;i< ChargingGroup.group_chargers.size;i++){
                memcpy(&buffer[(i*8)],ChargingGroup.group_chargers.charger_table[i].name,8);   
            }

            mqtt_publish("Params", buffer);
            ChargingGroup.SendNewParams = false;
        }

        //Avisar al maestro de que seguimos aqui
       
        if(!ChargingGroup.GroupMaster){ 
             mqtt_publish("Ping", ConfigFirebase.Device_Id);

            delay(1000);
            if(!ChargingGroup.GroupActive || GetStopMQTT()){
                printf("Maestro desconectado!!!\n");
                stop_MQTT();
                xTaskCreate(MasterPanicTask,"MasterPanicTask",4096,NULL,2,NULL);
                break;
            }
        }
        else{
            delay(1000);
        }
        
    }
    vTaskDelete(NULL);
}

void start_MQTT_server(){
    Serial.println("Arrancando servidor MQTT");
    //Preguntar si hay maestro en el grupo
    for(uint8_t i =0; i<10;i++){
        broadcast_a_grupo("Hay maestro?");
        delay(5);
        if(!ChargingGroup.GroupMaster)break;
    }
    
    
    if(ChargingGroup.GroupMaster){
        ChargingGroup.GroupActive = true;
        /* Start MQTT Server using tcp transport */
        xTaskCreateStatic(mqtt_server,"BROKER",1024*6,NULL,PRIORIDAD_MQTT,xSERVERStack,&xSERVERBuffer); 
        delay(5000);

        broadcast_a_grupo("Start client");
        mqtt_sub_pub_opts publisher;

        //Ponerme el primero en el grupo para indicar que soy el maestro
        while(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
            carac_charger OldMaster=ChargingGroup.group_chargers.charger_table[0];
            remove_from_group(OldMaster.name, &ChargingGroup.group_chargers);
            add_to_group(OldMaster.name, OldMaster.IP, &ChargingGroup.group_chargers);
        }
        
        sprintf(publisher.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP));
        memcpy(publisher.Client_ID,ConfigFirebase.Device_Id, 8);
        memcpy(publisher.Will_Message,ConfigFirebase.Device_Id, 8);
        publisher.Will_Topic = "NODE_DEAD";

        if(mqtt_connect(&publisher)){
            mqtt_subscribe("Device_Status");
            mqtt_subscribe("Ping");
            mqtt_subscribe("Params");
            xTaskCreateStatic(Publisher,"Publisher",4096,NULL,PRIORIDAD_MQTT,xPUBLISHERStack,&xPUBLISHERBuffer); 
        }
    }
    else{
        Serial.println("Ya hay un maestro en el grupo, me hago esclavo!");
    }
}

void start_MQTT_client(IPAddress remoteIP){
    ChargingGroup.GroupMaster = false;
    SendToPSOC5(ChargingGroup.GroupMaster, GROUPS_GROUP_MASTER);
    ChargingGroup.GroupActive = true;
    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", remoteIP.toString().c_str());
    strcpy(publisher.Client_ID,ConfigFirebase.Device_Id);
    strcpy(publisher.Will_Message,ConfigFirebase.Device_Id);
    publisher.Will_Topic = "NODE_DEAD";

    if(mqtt_connect(&publisher)){
        mqtt_subscribe("Device_Status");
        mqtt_subscribe("Pong");
        mqtt_subscribe("Params");
        xTaskCreateStatic(Publisher,"Publisher",4096,NULL,PRIORIDAD_MQTT,xPUBLISHERStack,&xPUBLISHERBuffer); 
    }
}
