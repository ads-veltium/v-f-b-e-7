#include "../control.h"
#include "AsyncUDP.h"
#include "cJSON.h"

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
uint8_t new_charger;
carac_chargers net_group EXT_RAM_ATTR;

//Prototipos de funciones externas
void mqtt_server(void *pvParameters);

//Prototipos de funciones internas
void start_MQTT_server();
void start_MQTT_client(IPAddress remoteIP);

void stop_MQTT(){
    ChargingGroup.Conected   = false;
    ChargingGroup.StartClient = false;
    SetStopMQTT(true);
    printf("Stopping MQTT\n");

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

//Comprobar si un cargador está almacenado en las tablas de equipos
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

//Almacenar grupo en la flash
void store_group_in_mem(carac_chargers* group){
    char sendBuffer[252];
    if(group->size< 10){
        sprintf(&sendBuffer[0],"0%i",(char)group->size);
    }
    else{
        sprintf(&sendBuffer[0],"%i",(char)group->size);
    }

    if(group->size < 25){
        if(group->size>0){
            for(uint8_t i=0;i<group->size;i++){
                memcpy(&sendBuffer[2+(i*9)],group->charger_table[i].name,8);
                itoa(group->charger_table[i].Fase,&sendBuffer[10+(i*9)],10);
            }
            SendToPSOC5(sendBuffer,ChargingGroup.group_chargers.size*9+2,GROUPS_DEVICES); 
        }
        else{
            for(int i=0;i<=250;i++){
                sendBuffer[i]=(char)0;
            }
            SendToPSOC5(sendBuffer,250,GROUPS_DEVICES); 
        }
        
        delay(100);
        return;
    }
    printf("Error al almacenar el grupo en la memoria\n");
}

//Eliminar grupo
void remove_group(carac_chargers* group){
    if(group->size >0){
        for(int j=0;j < group->size;j++){
            group->charger_table[j].IP = INADDR_NONE;       
            group->charger_table[j].Fase = 0;       
            memset(group->charger_table[j].name,0,9);
        }
        group->size = 0;
        return;
    }
    printf("Error al eliminar el grupo\n");  
}

//Obtener la ip de un equipo dado su ID
IPAddress get_IP(const char* ID){
    for(int j=0;j < net_group.size;j++){
        if(!memcmp(ID,  net_group.charger_table[j].name,8)){
            return net_group.charger_table[j].IP;
        }
    }
    return INADDR_NONE;
}

//Enviar mensajes solo a los miembros del grupo
void broadcast_a_grupo(char* Mensaje){
    AsyncUDPMessage mensaje (13);
    mensaje.write((uint8_t*)(Encipher(Mensaje).c_str()), 13);

    for(int i =0; i < net_group.size;i++){
        for(int j=0;j < ChargingGroup.group_chargers.size;j++){
            if(check_in_group(net_group.charger_table[i].name, &ChargingGroup.group_chargers)<255){
                udp.sendTo(mensaje, net_group.charger_table[i].IP,1234);
                ChargingGroup.group_chargers.charger_table[j].IP = net_group.charger_table[i].IP; 
            }
        }
    }
}

//Arrancar la comunicacion udp para escuchar cuando el maestro nos lo ordene
void start_udp(){
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
                if(ChargingGroup.Params.GroupMaster && ChargingGroup.Conected){
                    if(check_in_group(Desencriptado.c_str(), &ChargingGroup.group_chargers)!=255){
                        Serial.printf("El cargador VCD%s está en el grupo de carga\n", Desencriptado.c_str());  
                        AsyncUDPMessage mensaje (13);
                        mensaje.write((uint8_t*)(Encipher("Start client").c_str()), 13);
                        udp.sendTo(mensaje,packet.remoteIP(),1234);
                    }
                }
          
                if(check_in_group(Desencriptado.c_str(), &net_group)==255){
                    Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista de red\n", Desencriptado.c_str(), packet.remoteIP().toString().c_str());  
                    add_to_group(Desencriptado.c_str(), packet.remoteIP(), &net_group);
                } 
            }
            else{
                if(!memcmp(Desencriptado.c_str(), "Start client", 13)){
                    if(!ChargingGroup.Conected && !ChargingGroup.StartClient){
                        printf("Soy parte de un grupo !!\n");
                        SetStopMQTT(false);
                        ChargingGroup.StartClient = true;
                        ChargingGroup.Params.GroupMaster = false;
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.MasterIP =packet.remoteIP();
                        
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Hay maestro?", 13)){
                    if(ChargingGroup.Conected && ChargingGroup.Params.GroupMaster){
                        packet.print(Encipher(String("Bai, hemen nago")).c_str());
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Bai, hemen nago", 13)){
                    if(!ChargingGroup.Conected && ChargingGroup.Params.GroupMaster){
                        ChargingGroup.StartClient = true;
                        ChargingGroup.Params.GroupMaster = false;
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.MasterIP =packet.remoteIP();
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
    uint8 reintentos = 1;
    ChargingGroup.StartClient = false;
    int delai = 30000;
    while(!ChargingGroup.Conected){
        if(pdTICKS_TO_MS(xTaskGetTickCount() - xStart) > delai){ //si pasan 30 segundos, elegir un nuevo maestro
            delai=15000;
            Serial.println("Necesitamos un nuevo maestro!");

            if(!memcmp(ChargingGroup.group_chargers.charger_table[reintentos].name,ConfigFirebase.Device_Id,8)){
                Serial.println("Soy el nuevo maestro!!");
                ChargingGroup.Params.GroupActive = true;
                break;
            }
            else{
                //Ultima opcion, mirar si yo era el maestro
                if(!memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id,8)){
                    Serial.println("Soy el nuevo maestro!!");
                    ChargingGroup.Params.GroupActive = true;
                    break;
                }
                Serial.println("No soy el nuevo maestro, alguien se pondrá :)");
                xStart = xTaskGetTickCount();
                reintentos++;
                if(reintentos == ChargingGroup.group_chargers.size){
                    printf("Ya he mirado todos los equipos y no estoy!\n");
                    break;
                }
            }
        }
        delay(1000);
    }
    vTaskDelete(NULL);
}

/*Tarea para publicar los datos del equipo cada segundo*/
void Publisher(void* args){
    printf("Arrancando publicador\n");
    char buffer[500];
    Params.Fase = 1;
    TickType_t xStart = xTaskGetTickCount();

    while(1){
        //Enviar nuestros datos de carga al grupo
        if(ChargingGroup.SendNewData){   
            cJSON *Datos_Json;
	        Datos_Json = cJSON_CreateObject();

            cJSON_AddStringToObject(Datos_Json, "device_id", ConfigFirebase.Device_Id);
            cJSON_AddNumberToObject(Datos_Json, "fase", Params.Fase);
            cJSON_AddNumberToObject(Datos_Json, "current", Status.Measures.instant_current);
            //si es trifasico, enviar informacion de todas las fases
            if(Status.Trifasico){
                cJSON_AddNumberToObject(Datos_Json, "currentB", Status.MeasuresB.instant_current);
                cJSON_AddNumberToObject(Datos_Json, "currentC", Status.MeasuresC.instant_current);
            }
            cJSON_AddNumberToObject(Datos_Json, "Delta", Status.Delta);
            cJSON_AddStringToObject(Datos_Json, "HPT", Status.HPT_status);
            cJSON_AddNumberToObject(Datos_Json, "limite_fase",Status.limite_Fase);

            char *my_json_string = cJSON_Print(Datos_Json);                
            cJSON_Delete(Datos_Json);
            
            mqtt_publish("Data", my_json_string);
            ChargingGroup.SendNewData = false;
        }
        
        //Enviar nuevos parametros para el grupo
        if( ChargingGroup.SendNewParams || new_charger){
            buffer[0] = '1';
            memcpy(&buffer[1], &ChargingGroup.Params,7);
            printf("Enviando %s\n", buffer);
            mqtt_publish("RTS", buffer);
            ChargingGroup.SendNewParams = false;
        }

        //Enviar los cargadores de nuestro grupo
        if(ChargingGroup.SendNewGroup || new_charger){
            printf("Enviando nuevo grupo\n");
            buffer[0] = '3';
            if(ChargingGroup.group_chargers.size< 10){
                sprintf(&buffer[1],"0%i",(char)ChargingGroup.group_chargers.size);
            }
            else{
                sprintf(&buffer[1],"%i",(char)ChargingGroup.group_chargers.size);
            }
            
            for(uint8_t i=0;i< ChargingGroup.group_chargers.size;i++){
                memcpy(&buffer[3+(i*9)],ChargingGroup.group_chargers.charger_table[i].name,8);   
                itoa(ChargingGroup.group_chargers.charger_table[i].Fase,&buffer[11+(i*9)],10);
            }
            new_charger = 0;
            mqtt_publish("RTS", buffer);
            ChargingGroup.SendNewGroup = false;
        }

        //Avisar al maestro de que seguimos aqui
        if(!ChargingGroup.Params.GroupMaster){ 
            mqtt_publish("Ping", ConfigFirebase.Device_Id);
            delay(500);
            if(ChargingGroup.Params.GroupActive){
                if(!ChargingGroup.Conected || GetStopMQTT()){
                    printf("Maestro desconectado!!!\n");
                    ChargingGroup.Params.GroupActive = false;
                    stop_MQTT();
                    xTaskCreate(MasterPanicTask,"MasterPanicTask",4096,NULL,2,NULL);
                    vTaskDelete(NULL);
                }
            }

        }

        //Si soy el maestro, debo comprobar que los esclavos siguen conectados
        else{
            TickType_t Transcurrido = xTaskGetTickCount() - xStart;
            xStart = xTaskGetTickCount();
            for(uint8_t i=0 ;i<ChargingGroup.group_chargers.size;i++){
                ChargingGroup.group_chargers.charger_table[i].Period += Transcurrido;
                //si un equipo lleva mucho sin contestar, lo intentamos despertar
                if(ChargingGroup.group_chargers.charger_table[i].Period >=30000 && ChargingGroup.group_chargers.charger_table[i].Period <=60000){ 
                    AsyncUDPMessage mensaje (13);
                    mensaje.write((uint8_t*)(Encipher("Start client").c_str()), 13);
                    udp.sendTo(mensaje,ChargingGroup.group_chargers.charger_table[i].IP,1234);
                }
                
                //si un equipo lleva muchisimo sin contestar, lo damos por muerto y reseteamos sus valores
                if(ChargingGroup.group_chargers.charger_table[i].Period >=60000 && ChargingGroup.group_chargers.charger_table[i].Period <=65000){
                    if(memcmp(ChargingGroup.group_chargers.charger_table[i].name, ConfigFirebase.Device_Id,8)){
                        cJSON *Datos_Json;
	                    Datos_Json = cJSON_CreateObject();

                        cJSON_AddStringToObject(Datos_Json, "device_id", ChargingGroup.group_chargers.charger_table[i].name);
                        cJSON_AddNumberToObject(Datos_Json, "fase", ChargingGroup.group_chargers.charger_table[i].Fase);
                        cJSON_AddNumberToObject(Datos_Json, "Delta", 0);
                        cJSON_AddStringToObject(Datos_Json, "HPT", "0V");
                        cJSON_AddNumberToObject(Datos_Json, "limite_fase",0);
                        cJSON_AddNumberToObject(Datos_Json, "current", 0);
                        
                        char *my_json_string = cJSON_Print(Datos_Json);                
                        cJSON_Delete(Datos_Json);
            
                        mqtt_publish("Data", my_json_string);
                    }
                }
            }
        }

        //Si se pausa el grupo, avisar al resto y pausar todo
        if(!ChargingGroup.Params.GroupActive){
            if(!ChargingGroup.Conected || GetStopMQTT()){
                vTaskDelete(NULL);
            }

            buffer[0] = '2';
            memcpy(&buffer[1],"Pause",6);
            mqtt_publish("RTS", buffer);
        }

        //Si llega la orden de borrar, debemos eliminar el grupo de la memoria
        if(ChargingGroup.DeleteOrder){
            if(!ChargingGroup.Conected || GetStopMQTT()){
                vTaskDelete(NULL);
            }

            buffer[0] = '2';
            memcpy(&buffer[1],"Delete",6);
            mqtt_publish("RTS", buffer);
        }
        
        delay(1000);        
    }
    vTaskDelete(NULL);
}

void start_MQTT_server(){
    ChargingGroup.Params.GroupMaster = true;
    printf("Arrancando servidor MQTT\n");
    SetStopMQTT(false);
    //Preguntar si hay maestro en el grupo
    for(uint8_t i =0; i<10;i++){
        broadcast_a_grupo("Hay maestro?");
        delay(5);
        if(!ChargingGroup.Params.GroupMaster)break;
    }
    
    if(ChargingGroup.Params.GroupMaster){
        ChargingGroup.Conected = true;
        /* Start MQTT Server using tcp transport */
        char path[64];
        sprintf(path, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP));
        xTaskCreateStatic(mqtt_server,"BROKER",1024*6,&path,PRIORIDAD_MQTT,xSERVERStack,&xSERVERBuffer); 
        delay(5000);

        broadcast_a_grupo("Start client");
        mqtt_sub_pub_opts publisher;

        //Ponerme el primero en el grupo para indicar que soy el maestro
        if(ChargingGroup.group_chargers.size>0 && check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) != 255){
            while(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                carac_charger OldMaster=ChargingGroup.group_chargers.charger_table[0];
                remove_from_group(OldMaster.name, &ChargingGroup.group_chargers);
                add_to_group(OldMaster.name, OldMaster.IP, &ChargingGroup.group_chargers);
            }
            store_group_in_mem(&ChargingGroup.group_chargers);
        }
        else{
            //Si el grupo está vacio o el cargador no está en el grupo,
            printf("No tengo cargadores en el grupo o no estoy en mi propio grupo!\n");
            ChargingGroup.Params.GroupMaster = false;
            ChargingGroup.Params.GroupActive = false;
            ChargingGroup.Conected = false;
            stop_MQTT();
            return;
        }
        
        sprintf(publisher.url, "mqtt://%s:1883", ip4addr_ntoa(&Coms.ETH.IP));
        memcpy(publisher.Client_ID,ConfigFirebase.Device_Id, 8);

        if(mqtt_connect(&publisher)){
            mqtt_subscribe("Data");
            mqtt_subscribe("Ping");
            mqtt_subscribe("RTS");
            xTaskCreateStatic(Publisher,"Publisher",4096,NULL,PRIORIDAD_MQTT,xPUBLISHERStack,&xPUBLISHERBuffer); 
        }
    }
    else{
        Serial.println("Ya hay un maestro en el grupo, espero a que me ordene conectarme!");
    }
}

void start_MQTT_client(IPAddress remoteIP){    
    mqtt_sub_pub_opts publisher;
    
	sprintf(publisher.url, "mqtt://%s:1883", remoteIP.toString().c_str());
    strcpy(publisher.Client_ID,ConfigFirebase.Device_Id);

    if(mqtt_connect(&publisher)){
        ChargingGroup.Params.GroupMaster = false;
        ChargingGroup.Conected = true;
        mqtt_subscribe("Data");
        mqtt_subscribe("Pong");
        mqtt_subscribe("RTS");
        xTaskCreateStatic(Publisher,"Publisher",4096,NULL,PRIORIDAD_MQTT,xPUBLISHERStack,&xPUBLISHERBuffer); 
    }
}
