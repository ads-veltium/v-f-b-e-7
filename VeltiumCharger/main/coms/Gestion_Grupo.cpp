#include "../control.h"

#if (defined CONNECTED && defined CONNECTED)

#include "AsyncUDP.h"
#include "cJSON.h"

#include "../group_control.h"

#define GROUP_PARAMS   1
#define GROUP_CONTROL  2
#define GROUP_CHARGERS 3
#define SEND_DATA      4

extern esp_netif_t *eth_netif;

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

bool PARAR_CLIENTE = false;

bool udp_arrancado = false;
uint8_t net_group_size =0;

void coap_start_client();

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
    if(size < 10){
        sprintf(&Configuracion.group_data.group[0],"0%i",(char)size);
    }
    else{
        sprintf(&Configuracion.group_data.group[0],"%i",(char)size);
    }

    if(size>0){
        uint16_t i;

        //El grupo entra en una parte, asique solo mandamos una
        for(i=0;i<size;i++){
            memcpy(&Configuracion.group_data.group[2+(i*9)],group[i].name,8);
            Configuracion.group_data.group[10+(i*9)] = (char)((group[i].Circuito << 2) + group[i].Fase);
        }
        Configuracion.group_data.group[2+(i*9)] = '\0';
#ifdef DEBUG_GROUPS
        Serial.printf("Gestion_Grupo - Almacenamiento de grupo en flash COMPLETO. Buffer=[%s] Size=%u\n", Configuracion.group_data.group, size);
#endif
        Configuracion.Group_Guardar = true;
        delay(500);

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
        memset(Configuracion.group_data.group, 0, MAX_GROUP_BUFFER_SIZE);
        sprintf(&Configuracion.group_data.group[0],"00_CLEAN_");
		Configuracion.group_data.group[8]='\0';
        Configuracion.Group_Guardar = true;
        delay(500);
    }
	delay(500);
    return;
}

void store_params_in_mem(){
    #ifdef DEBUG_GROUPS
    Serial.println("Gestion_Grupo - store_params_in_mem: Almacenando datos en la memoria\n");
    #endif
    Configuracion.group_data.params[0] = ChargingGroup.Params.GroupMaster;
    Configuracion.group_data.params[1] = ChargingGroup.Params.GroupActive;
    Configuracion.group_data.params[2] =  ChargingGroup.Params.CDP;
    Configuracion.group_data.params[3] = (uint8_t) (ChargingGroup.Params.ContractPower  & 0x00FF);
    Configuracion.group_data.params[4] = (uint8_t) ((ChargingGroup.Params.ContractPower >> 8)  & 0x00FF);
    Configuracion.group_data.params[5] = (uint8_t) (ChargingGroup.Params.potencia_max  & 0x00FF);
    Configuracion.group_data.params[6] = (uint8_t) ((ChargingGroup.Params.potencia_max >> 8)  & 0x00FF);
	Configuracion.Group_Guardar = true;
    delay (200); // La actualizacion de parámetros se ejecuta cada 100 
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
    Serial.printf("Gestion_Grupo - remove_group: Error al eliminar el grupo\n");
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
                    udp.sendTo(mensaje, net_group[i].IP,2702, TCPIP_ADAPTER_IF_ETH);
#ifdef DEBUG_UDP
                    Serial.printf("Gestion_Grupo - broadcast_a_grupo: udp.sendTo \"%s\" to %s\n", Mensaje, net_group[i].IP.toString().c_str());
#endif
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
        udp.sendTo(_mensaje,IP,2702,TCPIP_ADAPTER_IF_ETH);
    }
}

//Arrancar la comunicacion udp para escuchar cuando el maestro nos lo ordene
void start_udp(){
#ifdef DEBUG_UDP
    Serial.printf("Gestion_Grupo - start_udp\n");
#endif   
    if(udp_arrancado){  
        return;
    }

    if(udp.listen(2702)) {
        udp_arrancado = true;
        udp.onPacket([](AsyncUDPPacket packet) {    
            int size = packet.length();
            char buffer[size+1] ;
            memcpy(buffer, packet.data(), size);
            buffer[size] = '\0';

            String Desencriptado;
            Desencriptado = Decipher(String(buffer));
#ifdef DEBUG_UDP
            Serial.printf("Gestion_Grupo - start_udp: paquete UDP recibido: \"%s\" desde %s por la interfaz %i\n",Desencriptado.c_str(),packet.remoteIP().toString().c_str(),packet.interface());
#endif   
            if(size<=8){
                if(packet.isBroadcast()){                   
                    packet.print(Encipher(String(ConfigFirebase.Device_Id)).c_str());
                }
                uint8 index = check_in_group(Desencriptado.c_str(), net_group, net_group_size);
                if (index != 255){
                    remove_from_group(Desencriptado.c_str(),net_group, &net_group_size);
                }
                if(packet.remoteIP()[0] == 0 && packet.remoteIP()[1] == 0 && packet.remoteIP()[2] == 0 && packet.remoteIP()[3] == 0 ){
#ifdef DEBUG_UDP
                    Serial.printf("Gestion_Grupo - start_udp: udp.broadcast %s\n",ConfigFirebase.Device_Id);
#endif
                    AsyncUDPMessage broadcast_message(8);
                    broadcast_message.write((uint8_t *)(Encipher(ConfigFirebase.Device_Id).c_str()), 8);
                    udp.sendTo(broadcast_message, IPADDR_BROADCAST, 2702, TCPIP_ADAPTER_IF_ETH);
                    //udp.broadcast(Encipher(String(ConfigFirebase.Device_Id)).c_str());
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
                print_net_table(net_group, "Net group", net_group_size);
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
#ifdef DEBUG_UDP
                        Serial.printf("Gestion_Grupo - start_udp: udp.sendTo \"Start client\" to %s\n",packet.remoteIP().toString().c_str());
#endif
                        udp.sendTo(mensaje,packet.remoteIP(),2702,TCPIP_ADAPTER_IF_ETH);
                    }
                }
            }
            else{
                if(!memcmp(Desencriptado.c_str(), "Start client", 12)){
#ifdef DEBUG_GROUPS
                    Serial.printf("Gestion_Grupo - start_udp: Me ha llegado start client\n");
                    Serial.printf("Gestion_Grupo - start_udp: ChargingGroup.Conected = %i, ChargingGroup.StartClient = %i\n", ChargingGroup.Conected, ChargingGroup.StartClient);
                    print_group_param (&ChargingGroup);
#endif
                    if(!ChargingGroup.Conected && !ChargingGroup.StartClient){
#ifdef DEBUG_GROUPS
                        Serial.printf("Soy parte de un grupo !!\n");
#endif
                        ChargingGroup.StartClient = true;
                        ChargingGroup.Params.GroupMaster = false;
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.MasterIP = packet.remoteIP();
                    }
                    else if(ChargingGroup.Conected){
                        // ADS Hay que reiniciar el cliente.
#ifdef DEBUG_GROUPS
                        Serial.printf("Gestion_Grupo - start_udp: Hay que reiniciar el cliente\n");
#endif                  
                        if (PARAR_CLIENTE){
                            coap_start_client();
                            PARAR_CLIENTE=false;
                        }
                        else{
                            PARAR_CLIENTE=true;
                        }
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Master here?", 12)){
                    if(ChargingGroup.Conected && ChargingGroup.Params.GroupMaster){
#ifdef DEBUG_UDP
                        Serial.printf("Gestion_Grupo - start_udp: Respuesta: \"Yes, Master here\" to %s\n",packet.remoteIP().toString().c_str());
#endif
                        packet.print(Encipher(String("Yes, Master here")).c_str());

                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Yes, Master here", 16)){
                    if(ChargingGroup.Conected && ChargingGroup.Params.GroupMaster){
                        ChargingGroup.StartClient = true;
                        ChargingGroup.Params.GroupMaster = false;
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.MasterIP =packet.remoteIP();
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Delete group", 12)){
                    if(ChargingGroup.Conected){
#ifdef DEBUG_GROUPS
                        Serial.println("el maestro me pide que borre el grupo!");
#endif
                        ChargingGroup.DeleteOrder = true;
                    }
                }
                else if(!memcmp(Desencriptado.c_str(), "Pause group", 11)){

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
#ifdef DEBUG_UDP
    Serial.printf("Gestion_Grupo - start_udp: udp.broadcast %s\n",ConfigFirebase.Device_Id);
    int netif_index = esp_netif_get_netif_impl_index (eth_netif);
    Serial.printf("Gestion_Grupo - start_udp: Interfaz=%i\n",netif_index);
#endif
    AsyncUDPMessage broadcast_message (8);
    broadcast_message.write((uint8_t*)(Encipher(ConfigFirebase.Device_Id).c_str()),8);
    udp.sendTo(broadcast_message,IPADDR_BROADCAST,2702,TCPIP_ADAPTER_IF_ETH);
    //udp.broadcast(Encipher(String(ConfigFirebase.Device_Id)).c_str());
}

void close_udp(){
#ifdef DEBUG_UDP
    printf("Gestion_Grupo - close_udp\n");
#endif   
    remove_group(net_group,&net_group_size);
    net_group_size = 0;
    udp.close();
    udp_arrancado = false;
}

#endif