#include "../control.h"
#if (defined CONNECTED && defined USE_GROUPS)

#include "AsyncUDP.h"
#include "cJSON.h"

#include "../group_control.h"

#define GROUP_PARAMS   1
#define GROUP_CONTROL  2
#define GROUP_CHARGERS 3
#define SEND_DATA      4

bool StopMQTT = false;
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

carac_chargers net_group EXT_RAM_ATTR;
extern carac_chargers FaseChargers;

bool udp_arrancado = false;

//Añadir un cargador a un equipo
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group){
    if(group->size < MAX_GROUP_SIZE){
        if(check_in_group(ID,group)==255){
            memcpy(group->charger_table[group->size].name, ID, 8);
            group->charger_table[group->size].name[8]='\0';
            group->charger_table[group->size].IP = IP;
            memset(group->charger_table[group->size].HPT,'0',2);
            group->charger_table[group->size].HPT[2]='\0';
            group->charger_table[group->size].Current = 0;
            group->charger_table[group->size].Conected = 0;
            group->charger_table[group->size].Delta = 0;
            group->charger_table[group->size].Delta_timer = 0;
            group->charger_table[group->size].Circuito = 0;
            group->charger_table[group->size].Consigna = 0;
            group->charger_table[group->size].Period = 0;
            group->size++;
            return true;
        }
    }
   
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
            return true;
        }
    }
    return false;
}

//Almacenar grupo en la flash
//Si el tamaño del grupo es mayor de 25 cargadores, enviarlo en dos trozos
void store_group_in_mem(carac_chargers* group){
    char sendBuffer[252];
    if(group->size< 10){
        sprintf(&sendBuffer[0],"0%i",(char)group->size);
    }
    else{
        sprintf(&sendBuffer[0],"%i",(char)group->size);
    }

    if(group->size>0){
        if(group->size>25){
            //Envio de la primera parte
            for(uint8_t i = 0;i < 25; i++){
                memcpy(&sendBuffer[2+(i*9)],group->charger_table[i].name,8);
                itoa(group->charger_table[i].Fase,&sendBuffer[10+(i*9)],10);
            }

            SendToPSOC5(sendBuffer,227,GROUPS_DEVICES_PART_1); 
            delay(250);

            //Envio de la segunda mitad
            if(group->size - 25< 10){
                sprintf(&sendBuffer[0],"0%i",(char)group->size-25);
            }
            else{
                sprintf(&sendBuffer[0],"%i",(char)group->size-25);
            }
            for(uint8_t i=0;i<(group->size - 25);i++){
                memcpy(&sendBuffer[2+(i*9)],group->charger_table[i+25].name,8);
                itoa(group->charger_table[i+25].Fase,&sendBuffer[10+(i*9)],10);
            }

            SendToPSOC5(sendBuffer,(group->size - 25)*9+2,GROUPS_DEVICES_PART_2); 
            delay(100);
        }
        else{

            //El grupo entra en una parte, asique solo mandamos una
            for(uint8_t i=0;i<group->size;i++){
                memcpy(&sendBuffer[2+(i*9)],group->charger_table[i].name,8);
                itoa(group->charger_table[i].Fase,&sendBuffer[10+(i*9)],10);
            }
            SendToPSOC5(sendBuffer,ChargingGroup.group_chargers.size*9+2,GROUPS_DEVICES_PART_1); 
        }
    }
    else{
        for(int i=0;i<=250;i++){
            sendBuffer[i]=(char)0;
        }
        SendToPSOC5(sendBuffer,250,GROUPS_DEVICES_PART_1); 
        SendToPSOC5(sendBuffer,250,GROUPS_DEVICES_PART_2); 
        
    }

    //si llega un grupo en el que no estoy, significa que me han sacado de el
    //cierro el coap y borro el grupo
    if(check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) == 255){
        if(ChargingGroup.Conected){
            printf("¡No estoy en el grupo!!!\n");
            ChargingGroup.DeleteOrder = true;
        }
    }
    
    //Ponerme el primero en el grupo para indicar que soy el maestro
    if(ChargingGroup.Params.GroupMaster){
        if(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
            if(ChargingGroup.group_chargers.size > 0 && check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) != 255){
                while(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                    carac_charger OldMaster = ChargingGroup.group_chargers.charger_table[0];
                    remove_from_group(OldMaster.name, &ChargingGroup.group_chargers);
                    add_to_group(OldMaster.name, OldMaster.IP, &ChargingGroup.group_chargers);
                    ChargingGroup.group_chargers.charger_table[ChargingGroup.group_chargers.size-1].Fase=OldMaster.Fase;
                    ChargingGroup.group_chargers.charger_table[ChargingGroup.group_chargers.size-1] = OldMaster;
                }
                ChargingGroup.SendNewGroup = true;
            }
        }
    }

    return;
    
    printf("Error al almacenar el grupo en la memoria\n");
}

//Eliminar grupo
void remove_group(carac_chargers* group){
    if(group->size >0){
        for(int j=0;j < group->size;j++){
            group->charger_table[j].IP = INADDR_NONE;       
            group->charger_table[j].Fase = 1;       
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
void broadcast_a_grupo(char* Mensaje, uint16_t size){
    AsyncUDPMessage mensaje (size);
    mensaje.write((uint8_t*)(Encipher(Mensaje).c_str()), size);

    for(int i =0; i < net_group.size;i++){
        for(int j=0;j < ChargingGroup.group_chargers.size;j++){
            if(check_in_group(net_group.charger_table[i].name, &ChargingGroup.group_chargers) != 255){
                if(net_group.charger_table[i].IP != IPADDR_NONE && net_group.charger_table[i].IP != IPADDR_ANY){
                    udp.sendTo(mensaje, net_group.charger_table[i].IP,2702);
                }
                ChargingGroup.group_chargers.charger_table[j].IP = net_group.charger_table[i].IP; 
                break;
            }
        }
    }
}

void send_to(IPAddress IP,  char* Mensaje){
    AsyncUDPMessage _mensaje (strlen(Mensaje));
    _mensaje.write((uint8_t*)(Encipher(Mensaje).c_str()), 13);
    if(IP != IPADDR_NONE && IP != IPADDR_ANY){
        udp.sendTo(_mensaje,IP,2702);
    }
}

//Arrancar la comunicacion udp para escuchar cuando el maestro nos lo ordene
void start_udp(){
    if(udp_arrancado){
        return;
    }
    udp_arrancado = true;

    if(udp.listen(2702)) {
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

                if(check_in_group(Desencriptado.c_str(), &net_group) == 255){
                    #ifdef DEBUG_GROUPS
                    Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista de red\n", Desencriptado.c_str(), packet.remoteIP().toString().c_str());  
                    #endif
                    add_to_group(Desencriptado.c_str(), packet.remoteIP(), &net_group);
                } 

                //Si el cargador está en el grupo de carga, le decimos que es un esclavo
                if(ChargingGroup.Params.GroupMaster && ChargingGroup.Conected){
                    if(check_in_group(Desencriptado.c_str(), &ChargingGroup.group_chargers) != 255){
                        #ifdef DEBUG_GROUPS
                        Serial.printf("El cargador VCD%s está en el grupo de carga\n", Desencriptado.c_str());  
                        #endif
                        broadcast_a_grupo("Start client", 12);
                        AsyncUDPMessage mensaje (13);
                        mensaje.write((uint8_t*)(Encipher("Start client").c_str()), 13);
                        udp.sendTo(mensaje,packet.remoteIP(),2702);
                    }
                }
            }
            else{
                if(!memcmp(Desencriptado.c_str(), "Start client", 12)){
                    if(!ChargingGroup.Conected && !ChargingGroup.StartClient){
                        #ifdef DEBUG_GROUPS
                        printf("Soy parte de un grupo !!\n");
                        #endif
                        StopMQTT = false;
                        ChargingGroup.StartClient = true;
                        ChargingGroup.Params.GroupMaster = false;
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.MasterIP =packet.remoteIP();
                        
                    }
      
                }
                else if(!memcmp(Desencriptado.c_str(), "Hay maestro?", 12)){
                    if(ChargingGroup.Conected && ChargingGroup.Params.GroupMaster){
                        packet.print(Encipher(String("Bai, hemen nago")).c_str());
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Bai, hemen nago", 15)){
                    if(ChargingGroup.Conected && ChargingGroup.Params.GroupMaster){
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

void close_udp(){
    udp.close();
}

#endif