#include "../control.h"
#include "AsyncUDP.h"
#include "cJSON.h"

//Funciones del MQTT
#include "../group_control.h"

#define START_GROUP    1
#define STOP_GROUP     2
#define DELETE_GROUP   3
#define GROUP_PARAMS   4
#define GROUP_CHARGERS 5
#define NEW_DATA       6

#define NO_ENCONTRADO  255
struct sub *s_subs EXT_RAM_ATTR;

uint8 check_in_group(const char* ID, carac_chargers* group);
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);

AsyncUDP udp EXT_RAM_ATTR;
AsyncUDPMessage mensaje EXT_RAM_ATTR;

String Encipher(String input);
String Decipher(String input);

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Params Params;
extern carac_group  ChargingGroup;
extern carac_Comands  Comands ;

static StackType_t xPUBLISHERStack [4096]     EXT_RAM_ATTR;
StaticTask_t xPUBLISHERBuffer ;
TaskHandle_t Publisher_Handle = NULL;
carac_chargers net_group EXT_RAM_ATTR;
carac_chargers net_active_group EXT_RAM_ATTR;
extern carac_chargers FaseChargers;

//Añadir un cargador a un equipo
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group){
    if(group->size < MAX_GROUP_SIZE){
        if(check_in_group(ID,group)==255){
            memcpy(group->charger_table[group->size].name, ID, 8);
            group->charger_table[group->size].name[8]='\0';
            group->charger_table[group->size].IP = IP;
            group->size++;
            print_table(*group, "Añadido a !");
            return true;
        }
    }
    printf("Error al añadir al grupo \n");
    return false;
}

//Comprobar si un cargador está almacenado en las tablas de equipos
uint8 check_in_group(const char* ID, carac_chargers* group){
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
                for(uint8 i=j;i < group->size-1;i++){
                    group->charger_table[i] = group->charger_table[i+1];
                }
            }
            group->charger_table[group->size-1].IP = INADDR_NONE;       
            memset(group->charger_table[group->size-1].name,0,9);
            --group->size;
            print_table(*group, "Borrado de !");
            return true;
        }
    }
    printf("El cargador no está en el grupo!\n");
    return false;
}

//Enviar grupo al PSOC5 para su almacenamiento
void store_group_in_mem(carac_chargers* group){
    char sendBuffer[252];
    if(group->size< 10){
        sprintf(&sendBuffer[0],"0%i",(char)group->size);
    }
    else{
        sprintf(&sendBuffer[0],"%i",(char)group->size);
    }

    //Si el grupo entra en un mensaje
    if(group->size < 25){
        if(group->size>0){
            for(uint8 i=0;i<group->size;i++){
                memcpy(&sendBuffer[2+(i*9)],group->charger_table[i].name,8);
                itoa(group->charger_table[i].Fase,&sendBuffer[10+(i*9)],10);
            }
            SendToPSOC5(sendBuffer,ChargingGroup.group_chargers.size*9+2,GROUPS_DEVICES); 
        }
        else{
            //Enviar el grupo vacío
            for(int i=0;i<=250;i++){
                sendBuffer[i]=(char)0;
            }
            SendToPSOC5(sendBuffer,250,GROUPS_DEVICES); 
        }
        
        delay(100);
        return;
    }

    //Si no entra en un mensaje
    else if(group->size> 25 && group->size < MAX_GROUP_SIZE){
        uint8 i=0;
        for(;i<25;i++){
            memcpy(&sendBuffer[2+(i*9)],group->charger_table[i].name,8);
            itoa(group->charger_table[i].Fase,&sendBuffer[10+(i*9)],10);
        }
        SendToPSOC5(sendBuffer,227,GROUPS_DEVICES); 

        if(group->size - 25 < 10){
            sprintf(&sendBuffer[0],"0%i",(char)(group->size -25));
        }
        else{
            sprintf(&sendBuffer[0],"%i",(char)(group->size - 25));
        }

        for(uint8_t j=0;j<group->size-25;j++){
            memcpy(&sendBuffer[2+(j*9)],group->charger_table[i].name,8);
            itoa(group->charger_table[i].Fase,&sendBuffer[10+(j*9)],10);
            i++;
        }
        SendToPSOC5(sendBuffer,227,GROUPS_DEVICES); 
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

//Enviar mensajes solo a los miembros del grupo, sin topic
void broadcast_a_grupo(AsyncUDPMessage* _mensaje, carac_chargers* group){
    for(int i =0; i < net_group.size;i++){
        for(int j=0;j < group->size;j++){
            if(check_in_group(net_group.charger_table[i].name, group)!=255){
                udp.sendTo(*_mensaje, net_group.charger_table[i].IP,2702);
                group->charger_table[j].IP = net_group.charger_table[i].IP; 
            }
        }
    }
}

//Funcion para enviar datos a través del protocolo a un grupo
void VGP_Send(uint8 topic, char* _mensaje, uint8 size, carac_chargers* group){
    char _topic;
    mensaje.flush();
    mensaje.write((uint8*)Encipher("RTS").c_str(), 3);
    itoa(topic,&_topic ,10);
    mensaje.write((uint8*)Encipher(&_topic).c_str(), 1);
    if(size>0){
        mensaje.write((uint8*)Encipher(_mensaje).c_str(),size);
    }
    broadcast_a_grupo(&mensaje, group);
}

/*Tarea para publicar los datos del equipo cada segundo*/
void Publisher(void* args){
    printf("Arrancando publicador\n");
    ChargingGroup.Conected = true;
    char buffer[500];
    TickType_t timer = xTaskGetTickCount();
    while(1){
        
        //Enviar nuevos parametros para el grupo
        if( ChargingGroup.SendNewParams){
            memcpy(buffer, &ChargingGroup.Params,7);
            VGP_Send(GROUP_PARAMS, buffer, 7, &ChargingGroup.group_chargers);
            ChargingGroup.SendNewParams = false;
        }

        //Enviar los cargadores de nuestro grupo
        else if(ChargingGroup.SendNewGroup){
            if(ChargingGroup.group_chargers.size< 10){
                sprintf(&buffer[0],"0%i",(char)ChargingGroup.group_chargers.size);
            }
            else{
                sprintf(&buffer[0],"%i",(char)ChargingGroup.group_chargers.size);
            }
            
            for(uint8 i=0;i< ChargingGroup.group_chargers.size;i++){
                memcpy(&buffer[2+(i*9)],ChargingGroup.group_chargers.charger_table[i].name,8);   
                itoa(ChargingGroup.group_chargers.charger_table[i].Fase,&buffer[10+(i*9)],10);
            }
            VGP_Send(GROUP_CHARGERS, buffer, ChargingGroup.group_chargers.size*9+2, &ChargingGroup.group_chargers);
            ChargingGroup.SendNewGroup = false;
        }

        //Enviar nuestros datos de carga al grupo
        else if(ChargingGroup.SendNewData){
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
            
            VGP_Send(NEW_DATA, my_json_string, strlen(my_json_string), &ChargingGroup.group_chargers);
            free(my_json_string);
            ChargingGroup.SendNewData = false; 
        }

        else{
            //Comprobar si los esclavos se desconectan cada 15 segundos
            TickType_t Transcurrido = xTaskGetTickCount() - timer;
            if(Transcurrido > 15000){
                timer = xTaskGetTickCount();
                for(uint8 i=0 ;i<ChargingGroup.group_chargers.size;i++){
                    ChargingGroup.group_chargers.charger_table[i].Period += Transcurrido;
                    //si un equipo lleva mucho sin contestar, lo intentamos despertar
                    if(ChargingGroup.group_chargers.charger_table[i].Period >=30000 && ChargingGroup.group_chargers.charger_table[i].Period <=60000){ 
                        mensaje.flush();
                        mensaje.write((uint8*)Encipher("RCS").c_str(), 3);
                        mensaje.write((uint8*)Encipher(String(START_GROUP)).c_str(),1);
                        udp.sendTo(mensaje,ChargingGroup.group_chargers.charger_table[i].IP,2702);
                    }
                    
                    //si un equipo lleva muchisimo sin contestar, lo damos por muerto y lo eliminamos de la tabla
                    if(ChargingGroup.group_chargers.charger_table[i].Period >=60000 && ChargingGroup.group_chargers.charger_table[i].Period <=65000){
                        if(memcmp(ChargingGroup.group_chargers.charger_table[i].name, ConfigFirebase.Device_Id,8)){
                            remove_from_group(ChargingGroup.group_chargers.charger_table[i].name,&net_group);
                            remove_from_group(ChargingGroup.group_chargers.charger_table[i].name,&net_active_group);
                            remove_from_group(ChargingGroup.group_chargers.charger_table[i].name,&FaseChargers);
                        }
                    }
                }
            }
        }
       
        //Ordenes de pausa / borrado
        if(ChargingGroup.StopOrder || ChargingGroup.DeleteOrder){

            printf("Tengo que pausar o eliminar el grupo!\n");
            ChargingGroup.Params.GroupActive = 0;
            //Almacenar el grupo como pausado
            char buffer[7];
            memcpy(&buffer,&ChargingGroup.Params,7);
            SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 

            //Borrar el grupo
            if(ChargingGroup.DeleteOrder){
                remove_group(&ChargingGroup.group_chargers);
                store_group_in_mem(&ChargingGroup.group_chargers);
            }
            ChargingGroup.DeleteOrder = false;
            ChargingGroup.StopOrder   = false;
            break;
        }
        delay(2000);        
    }
    ChargingGroup.Conected = false;
    vTaskDelete(Publisher_Handle);
}

//Callback para la gestion de los datos recibidos por UDP
void Callback(char* data, int size){
    switch(atoi(&data[0])){
        case START_GROUP:
            printf("Soy parte de un grupo !!\n");
            ChargingGroup.Params.GroupActive = true;
            xTaskCreateStatic(Publisher,"Publisher",4096,NULL,PRIORIDAD_MQTT,xPUBLISHERStack,&xPUBLISHERBuffer); 
        break;
        case STOP_GROUP:
            ChargingGroup.StopOrder = true;
        break;
        case DELETE_GROUP:
            ChargingGroup.DeleteOrder = true;
        break;
        case GROUP_CHARGERS:
            New_Group(&data[1], size);
        break;
        case GROUP_PARAMS:
            New_Params(&data[1], size);
        break;
        case NEW_DATA:
            New_Data(&data[1], size);
        break;
        default:
            printf("Me han llegado datos desconocidos!==%s\n", data);
            break;
    }
}

//Arrancar la comunicacion udp para escuchar cuando el maestro nos lo ordene
void udp_group(){
    
    if(udp.listen(2702)) {
        udp.onPacket([](AsyncUDPPacket packet) {         
            int size = packet.length();
            char buffer[size+1] ;
            memcpy(buffer, packet.data(), size);
            buffer[size] = '\0';

            char* Desencriptado = (char*)malloc(size+1);
            
            //Desencriptado = Decipher((char*)buffer).c_str();
            memcpy(Desencriptado, Decipher(String(buffer)).c_str(), size+1);
            printf("Desencriptado %s\n", Desencriptado);

            if(!memcmp(Desencriptado, "Net_Group", 9)){
                //Respopnder al equipo para que sepa que estamos en su red
                if(packet.isBroadcast()){                 
                    mensaje.flush();
                    mensaje.write((uint8*)Encipher("Net_Group").c_str(), 9);
                    mensaje.write((uint8*)Encipher(String(ConfigFirebase.Device_Id)).c_str(),8 );
                    udp.sendTo(mensaje,packet.remoteIP(),2702);
                }

                //Si tenemos un grupo activo y el cargador está en el, se lo decimos
                if(ChargingGroup.Params.GroupActive){
                    if(check_in_group(&Desencriptado[9], &ChargingGroup.group_chargers) != NO_ENCONTRADO){
                        #ifdef DEBUG_GROUPS
                        Serial.printf("El cargador VCD%s está en el grupo de carga\n", &Desencriptado[9]);  
                        #endif
                        mensaje.flush();
                        mensaje.write((uint8*)Encipher("RCS").c_str(), 3);
                        mensaje.write((uint8*)Encipher(String(START_GROUP)).c_str(),1);
                        udp.sendTo(mensaje,packet.remoteIP(),2702);
                    }
                }

                //Si el cargador no está en el grupo, lo añadimos
                if(check_in_group(&Desencriptado[9], &net_group) == NO_ENCONTRADO){
                    #ifdef DEBUG_GROUPS
                    Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista de red\n", &Desencriptado[9], packet.remoteIP().toString().c_str());  
                    #endif
                    add_to_group(&Desencriptado[9], packet.remoteIP(), &net_group);

                    //si el cargador esta tambien en el grupo de carga
                    if(check_in_group(&Desencriptado[9], &net_active_group) == NO_ENCONTRADO){
                        add_to_group(&Desencriptado[9], packet.remoteIP(), &net_active_group);
                    }
                } 
            }
            else if(!memcmp(Desencriptado, "RTS", 3)){
                Callback(&Desencriptado[3], size);
            }
            else{
                printf("Me han llegado datos raros %s\n", Desencriptado);
            }
            free(Desencriptado);
        });
    }

    //Avisar al resto de equipos de que estamos aqui!
    mensaje.flush();
    char SendBuffer[17];

    memcpy(SendBuffer,Encipher("Net_Group").c_str(),9 );
    memcpy(&SendBuffer[9],Encipher(String(ConfigFirebase.Device_Id)).c_str(),8 );

    mensaje.write((uint8_t*)SendBuffer, 17);
    udp.broadcastTo(mensaje, 2702);
}

void start_VGP(){
    printf("Arranco el VGP!!\n");
    if(Publisher_Handle==NULL){
        Publisher_Handle = xTaskCreateStatic(Publisher,"Publisher",4096,NULL,PRIORIDAD_MQTT,xPUBLISHERStack,&xPUBLISHERBuffer); 
    }   
}