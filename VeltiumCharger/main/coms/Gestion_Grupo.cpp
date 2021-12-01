#include "../control.h"
#if (defined CONNECTED && defined CONNECTED)

#include "AsyncUDP.h"
#include "cJSON.h"

#include "../group_control.h"

#define GROUP_PARAMS   1
#define GROUP_CONTROL  2
#define GROUP_CHARGERS 3
#define SEND_DATA      4


uint8_t check_in_group(const char* ID, carac_charger* group, uint8_t size);
bool add_to_group(const char* ID, IPAddress IP, carac_charger* group, uint8_t *size);

AsyncUDP udp EXT_RAM_ATTR;

String Decipher(String input);
String Encipher(String input);

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Params Params;
extern carac_group  ChargingGroup;
extern carac_charger charger_table[MAX_GROUP_SIZE];
extern carac_Comands  Comands ;

carac_charger net_group[MAX_GROUP_SIZE] EXT_RAM_ATTR;

bool udp_arrancado = false;
uint8_t net_group_size =0;

//Añadir un cargador a un equipo
bool add_to_group(const char* ID, IPAddress IP, carac_charger* group, uint8_t* size){
    if(*size < MAX_GROUP_SIZE){
        if(check_in_group(ID,group, *size)==255){
            memcpy(group[*size].name, ID, 8);
            group[*size].name[8]='\0';
            group[*size].IP = IP;
            memset(group[*size].HPT,'0',2);
            group[*size].HPT[2]='\0';
            group[*size].Current = 0;
            group[*size].Conected = 0;
            group[*size].Delta = 0;
            group[*size].Delta_timer = 0;
            group[*size].Circuito = 0;
            group[*size].Consigna = 0;
            group[*size].Period = 0;
            *size = *size + 1;
            return true;
        }
    }
   
    return false;
}

//Comprobar si un cargador está almacenado en las tablas de equipos
uint8_t check_in_group(const char* ID, carac_charger* group, uint8_t size){
    for(int j=0;j < size;j++){
        if(!memcmp(ID,  group[j].name,8)){
            return j;
        }
    }
    return 255;
}

//Eliminar un cargador de un grupo
bool remove_from_group(const char* ID ,carac_charger* group, uint8_t* size){
    for(int j=0;j <*size;j++){
        if(!memcmp(ID,  group[j].name,8)){
            if(j!=*size-1){ //si no es el ultimo debemos shiftear la lista
                for(uint8_t i=j;i < *size-1;i++){
                    group[i] = group[i+1];
                }
            }
            group[*size-1].IP = INADDR_NONE;       
            memset(group[*size-1].name,0,9);
            *size = *size -1;
            return true;
        }
    }
    return false;
}

//Almacenar grupo en la flash
//Si el tamaño del grupo es mayor de 25 cargadores, enviarlo en dos trozos
void store_group_in_mem(carac_charger* group, uint8_t size){
    char sendBuffer[252];
    if(size< 10){
        sprintf(&sendBuffer[0],"0%i",(char)size);
    }

    else{
        sprintf(&sendBuffer[0],"%i",(char)size);
    }

    if(size>0){
        if(size>25){
            //Envio de la primera parte
            for(uint8_t i = 0;i < 25; i++){
                memcpy(&sendBuffer[2+(i*9)],group[i].name,8);
                sendBuffer[10+(i*9)] = (char)((group[i].Circuito << 2) + group[i].Fase);
            }

            SendToPSOC5(sendBuffer,227,GROUPS_DEVICES_PART_1); 
            delay(250);

            //Envio de la segunda mitad
            if(size - 25< 10){
                sprintf(&sendBuffer[0],"0%i",(char)size-25);
            }
            else{
                sprintf(&sendBuffer[0],"%i",(char)size-25);
            }
            for(uint8_t i=0;i<(size - 25);i++){
                memcpy(&sendBuffer[2+(i*9)],group[i+25].name,8);
                sendBuffer[10+(i*9)] = (char)((group[i+25].Circuito << 2) + group[i+25].Fase);
                
            }

            SendToPSOC5(sendBuffer,(size - 25)*9+2,GROUPS_DEVICES_PART_2); 
            delay(100);
        }
        else{

            //El grupo entra en una parte, asique solo mandamos una
            for(uint8_t i=0;i<size;i++){
                memcpy(&sendBuffer[2+(i*9)],group[i].name,8);
                sendBuffer[10+(i*9)] = (char)((group[i].Circuito << 2) + group[i].Fase);
            }
            SendToPSOC5(sendBuffer,ChargingGroup.Charger_number*9+2,GROUPS_DEVICES_PART_1); 
        }

        //si llega un grupo en el que no estoy, significa que me han sacado de el
        //cierro el coap y borro el grupo
        if(check_in_group(ConfigFirebase.Device_Id,charger_table, ChargingGroup.Charger_number ) == 255){
            if(ChargingGroup.Conected){
                #ifdef DEBUG_GROUPS
                printf("¡No estoy en el grupo!!!\n");
                print_table(charger_table,"No en grupo table", ChargingGroup.Charger_number);
                #endif
                ChargingGroup.DeleteOrder = true;
            }
        }
        
        //Ponerme el primero en el grupo para indicar que soy el maestro
        if(ChargingGroup.Params.GroupMaster){
            if(memcmp(charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                if(ChargingGroup.Charger_number > 0 && check_in_group(ConfigFirebase.Device_Id, charger_table,ChargingGroup.Charger_number ) != 255){
                    while(memcmp(charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                        carac_charger OldMaster = charger_table[0];
                        remove_from_group(OldMaster.name, charger_table,  &ChargingGroup.Charger_number);
                        add_to_group(OldMaster.name, OldMaster.IP, charger_table, &ChargingGroup.Charger_number);
                        charger_table[ChargingGroup.Charger_number-1].Fase=OldMaster.Fase;
                        charger_table[ChargingGroup.Charger_number-1].Circuito =OldMaster.Circuito;
                        charger_table[ChargingGroup.Charger_number-1] = OldMaster;
                    }
                    ChargingGroup.SendNewGroup = true;
                }
            }
        }

    }
    else{
        for(int i=0;i<=250;i++){
            sendBuffer[i]=(char)0;
        }
        SendToPSOC5(sendBuffer,250,GROUPS_DEVICES_PART_1); 
        SendToPSOC5(sendBuffer,250,GROUPS_DEVICES_PART_2); 
        
    }
    return;
}

void store_params_in_mem(){
    #ifdef DEBUG_GROUPS
    Serial.println("Almacenando datos en la memoria\n");
    #endif
    char buffer[7];
    buffer[0] = ChargingGroup.Params.GroupMaster;
    buffer[1] = ChargingGroup.Params.GroupActive;
    buffer[2] =  ChargingGroup.Params.CDP;
    buffer[3] = (uint8_t) (ChargingGroup.Params.ContractPower  & 0x00FF);
    buffer[4] = (uint8_t) ((ChargingGroup.Params.ContractPower >> 8)  & 0x00FF);
    buffer[5] = (uint8_t) (ChargingGroup.Params.potencia_max  & 0x00FF);
    buffer[6] = (uint8_t) ((ChargingGroup.Params.potencia_max >> 8)  & 0x00FF);
    SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 
}

//Eliminar grupo
void remove_group(carac_charger* group, uint8_t* size){
    if(*size > 0){
        for(int j=0;j < *size;j++){
            charger_table[j].IP = INADDR_NONE;       
            charger_table[j].Fase = 1; 
            charger_table[j].Circuito = 0;       
            memset(charger_table[j].name,0,9);
        }
        *size = 0;
        return;
    }
    #ifdef DEBUG_GROUPS
    printf("Error al eliminar el grupo\n");  
    #endif
}

//Obtener la ip de un equipo dado su ID
IPAddress get_IP(const char* ID){
    for(int j=0;j < net_group_size;j++){
        if(!memcmp(ID,  net_group[j].name,8)){
            return net_group[j].IP;
        }
    }
    return INADDR_NONE;
}

//Enviar mensajes solo a los miembros del grupo
void broadcast_a_grupo(char* Mensaje, uint16_t size){
    AsyncUDPMessage mensaje (size);
    mensaje.write((uint8_t*)(Encipher(Mensaje).c_str()), size);

    for(int i =0; i < net_group_size;i++){
        for(int j=0;j < ChargingGroup.Charger_number;j++){
            if(check_in_group(net_group[i].name, charger_table, ChargingGroup.Charger_number) != 255){
                if(net_group[i].IP != IPADDR_NONE && net_group[i].IP != IPADDR_ANY){
                    udp.sendTo(mensaje, net_group[i].IP,2702);
                }
                charger_table[j].IP = net_group[i].IP; 
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

                uint8 index = check_in_group(Desencriptado.c_str(), net_group, net_group_size);

                if (index != 255){
                    remove_from_group(Desencriptado.c_str(),net_group, &net_group_size);
                }

                if(packet.remoteIP()[0] == 0 && packet.remoteIP()[1] == 0 && packet.remoteIP()[2] == 0 && packet.remoteIP()[3] == 0 ){
                    udp.broadcast(Encipher(String(ConfigFirebase.Device_Id)).c_str());
                    return;
                }
                #ifdef DEBUG_GROUPS
                Serial.printf("El cargador VCD%s con ip %s se ha añadido a la lista de red\n", Desencriptado.c_str(), packet.remoteIP().toString().c_str());  
                #endif
                add_to_group(Desencriptado.c_str(), packet.remoteIP(), net_group, &net_group_size);

                //Actualizar net devices
                uint8_t group_buffer[452];
                group_buffer[0] = net_group_size +1;
                memcpy(&group_buffer[1], ConfigFirebase.Device_Id, 8);
                group_buffer[9] = 0;
                for(int i =0;i< net_group_size; i++){
                    memcpy(&group_buffer[i*9+10], net_group[i].name,8);
                    group_buffer[i*9+18]=0;
                }
                serverbleNotCharacteristic(group_buffer,net_group_size*9 +10, CHARGING_GROUP_BLE_NET_DEVICES);
                #ifdef DEBUG_GROUPS
                print_table(net_group, "Net group", net_group_size);
                #endif
                

                //Si el cargador está en el grupo de carga, le decimos que es un esclavo
                if(ChargingGroup.Params.GroupMaster && ChargingGroup.Conected){
                    if(check_in_group(Desencriptado.c_str(), charger_table, net_group_size) != 255){
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
                    #ifdef DEBUG_GROUPS
                    printf("ME ha llegado start client!\n");
                    #endif
                    if(!ChargingGroup.Conected && !ChargingGroup.StartClient){
                        #ifdef DEBUG_GROUPS
                        printf("Soy parte de un grupo !!\n");
                        #endif

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
                else if(!memcmp(Desencriptado.c_str(), "Ezabatu taldea", 14)){
                    if(ChargingGroup.Conected){
                        #ifdef DEBUG_GROUPS
                        Serial.println("el maestro me pide que borre el grupo!");
                        #endif
                        ChargingGroup.DeleteOrder = true;
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Geldituzazu taldea", 18)){
                    if(ChargingGroup.Conected){
                        #ifdef DEBUG_GROUPS
                        Serial.println("el maestro me pide que pause el grupo!");
                        #endif
                        ChargingGroup.StopOrder = true;
                    }
                }
            }            
        });
    }
    
    //Avisar al resto de equipos de que estamos aqui!
    udp.broadcast(Encipher(String(ConfigFirebase.Device_Id)).c_str());
}

void close_udp(){
    remove_group(net_group,&net_group_size);
    net_group_size = 0;
    udp.close();
    udp_arrancado = false;
}

#endif