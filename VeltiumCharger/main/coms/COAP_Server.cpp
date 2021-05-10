
#include <coap-simple.h>
#include <WiFiUdp.h>
#include "../control.h"
#include "../group_control.h"
#include "cJSON.h"

extern carac_Coms  Coms;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_Status Status;
extern carac_Params Params;
extern carac_group  ChargingGroup;
extern carac_chargers FaseChargers;
extern carac_chargers net_group;
extern carac_Comands  Comands ;

#define GROUP_PARAMS   1
#define GROUP_CONTROL  2
#define GROUP_CHARGERS 3
#define SEND_DATA      4
#define NEW_DATA       5

// UDP and CoAP class
WiFiUDP  Udp   EXT_RAM_ATTR;
Coap     coap  EXT_RAM_ATTR;

uint8_t check_in_group(const char* ID, carac_chargers* group);
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);
bool remove_from_group(const char* ID ,carac_chargers* group);
void store_group_in_mem(carac_chargers* group);
void broadcast_a_grupo(char* Mensaje);
void send_to(IPAddress IP,  char* Mensaje);
IPAddress get_IP(const char* ID);

void coap_broadcast_to_group(char* Mensaje, uint8_t messageID){

    for(int i =0; i < net_group.size;i++){
        for(int j=0;j < ChargingGroup.group_chargers.size;j++){
            if(check_in_group(net_group.charger_table[i].name, &ChargingGroup.group_chargers)<255){
                ChargingGroup.group_chargers.charger_table[j].IP = net_group.charger_table[i].IP; 
                coap.sendResponse(ChargingGroup.group_chargers.charger_table[j].IP, 5683, messageID, Mensaje);
            }
        }
    }
    coap.sendResponse(IPAddress(192,168,20,163), 5683, messageID, Mensaje);
}

//Enviar mis datos de carga
void Send_Data(){
  
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

  coap.put(ChargingGroup.MasterIP, 5683, "Data", my_json_string);
  free(my_json_string);
  ChargingGroup.SendNewData = false;     

}

// rebotar la informacion segun el topic
void callback_PARAMS(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("PARAMS");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';
  
  String message(p);

  Serial.println(message);
  Serial.println(ip);

  if(packet.payloadlen >0){
    coap_broadcast_to_group((char*)packet.payload, GROUP_PARAMS);
  }

}

void callback_DATA(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("DATA");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';
  
  String message(p);

  Serial.println(message);

  //if(packet.payloadlen >0){
    coap_broadcast_to_group("Pruebas", SEND_DATA);
  //} 
}

void callback_CONTROL(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("CONTROL");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';
  
  String message(p);

  Serial.println(message);
  if(packet.payloadlen >0){
    coap_broadcast_to_group((char*)packet.payload, GROUP_CONTROL);
  }
}

void callback_CHARGERS(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("CHARGERS");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';
  
  String message(p);

  Serial.println(message);
  if(packet.payloadlen >0){
    coap_broadcast_to_group((char*)packet.payload, GROUP_CHARGERS);
  }
}

void callback_PING(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("PING");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';
  
  String message(p);

  Serial.println(message);
  if(packet.payloadlen >0){
    coap_broadcast_to_group((char*)packet.payload, GROUP_CHARGERS);
  }
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Coap Response got]");
  
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';
  
  Serial.println(packet.messageid);

  switch(packet.messageid){
    case GROUP_PARAMS: 
      New_Params(p, packet.payloadlen);
      break;
    case GROUP_CONTROL:
      New_Control(p, packet.payloadlen);
      break;
    case GROUP_CHARGERS:
      New_Group(p, packet.payloadlen);
      break;
    case NEW_DATA:
      New_Data(p, packet.payloadlen);
      break;
    case SEND_DATA:
      printf("Debo mandar mis datos!\n");
      Send_Data();
      break;
  }
}

/*Tarea de emergencia para cuando el maestro se queda muerto mucho tiempo*/
void MasterPanicTask(void *args){
    TickType_t xStart = xTaskGetTickCount();
    uint8 reintentos = 1;
    ChargingGroup.StartClient = false;
    int delai = 30000;
    Serial.println("Necesitamos un nuevo maestro!");
    while(!ChargingGroup.Conected){
        if(pdTICKS_TO_MS(xTaskGetTickCount() - xStart) > delai){ //si pasan 30 segundos, elegir un nuevo maestro
            delai=10000;           

            if(!memcmp(ChargingGroup.group_chargers.charger_table[reintentos].name,ConfigFirebase.Device_Id,8)){
                ChargingGroup.Params.GroupActive = true;
                break;
            }
            else{
                //Ultima opcion, mirar si yo era el maestro
                if(!memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id,8)){
                    ChargingGroup.Params.GroupActive = true;
                }
                xStart = xTaskGetTickCount();
                reintentos++;
                if(reintentos == ChargingGroup.group_chargers.size){
                    printf("Ya he mirado todos los equipos y no estoy!\n");
                    break;
                }
            }
        }
        if(!Coms.ETH.conectado){
            break;
        }
        delay(1000);
    }
    vTaskDelete(NULL);
}

void coap_loop(void *args) {
    printf("Arrancando coap loop\n");
    char buffer[500];
    uint8_t  turno =0;
    TickType_t xStart = xTaskGetTickCount();
    while(1){   
      //Enviar nuevos parametros para el grupo
      if( ChargingGroup.SendNewParams){
          memcpy(buffer, &ChargingGroup.Params,7);
          coap.put(ChargingGroup.MasterIP, 5683, "Params", buffer);
          ChargingGroup.SendNewParams = false;
      }

      //Enviar los cargadores de nuestro grupo
      if(ChargingGroup.SendNewGroup){
          if(ChargingGroup.group_chargers.size< 10){
              sprintf(buffer,"0%i",(char)ChargingGroup.group_chargers.size);
          }
          else{
              sprintf(buffer,"%i",(char)ChargingGroup.group_chargers.size);
          }
          
          for(uint8_t i=0;i< ChargingGroup.group_chargers.size;i++){
              memcpy(&buffer[2+(i*9)],ChargingGroup.group_chargers.charger_table[i].name,8);   
              itoa(ChargingGroup.group_chargers.charger_table[i].Fase,&buffer[10+(i*9)],10);
          }
          coap.put(ChargingGroup.MasterIP, 5683, "Chargers", buffer);
          ChargingGroup.SendNewGroup = false;
      }
      if(ChargingGroup.Params.GroupMaster){
        //El maestro debe hacer mas cosas
        //Comprobar si los esclavos se desconectan cada 15 segundos
        TickType_t Transcurrido = xTaskGetTickCount() - xStart;
        if(Transcurrido > 15000){
            xStart = xTaskGetTickCount();
            for(uint8_t i=0 ;i<ChargingGroup.group_chargers.size;i++){
                ChargingGroup.group_chargers.charger_table[i].Period += Transcurrido;
                //si un equipo lleva mucho sin contestar, lo intentamos despertar
                if(ChargingGroup.group_chargers.charger_table[i].Period >=30000 && ChargingGroup.group_chargers.charger_table[i].Period <=60000){ 
                    send_to(ChargingGroup.group_chargers.charger_table[i].IP, "Start client");
                }
                
                //si un equipo lleva muchisimo sin contestar, lo damos por muerto y lo eliminamos de la tabla
                if(ChargingGroup.group_chargers.charger_table[i].Period >=60000 && ChargingGroup.group_chargers.charger_table[i].Period <=65000){
                    if(memcmp(ChargingGroup.group_chargers.charger_table[i].name, ConfigFirebase.Device_Id,8)){
                        remove_from_group(ChargingGroup.group_chargers.charger_table[i].name,&net_group);
                        if(ChargingGroup.group_chargers.charger_table[i].Fase == Params.Fase){
                          remove_from_group(ChargingGroup.group_chargers.charger_table[i].name,&FaseChargers);
                        }
                    }
                }
            }
        }
        //Pedir datos a los esclavos para que no envíen todos a la vez
        coap.sendResponse(get_IP(ChargingGroup.group_chargers.charger_table[turno].name), 5683, SEND_DATA, "X");
        turno++;
        if(turno == ChargingGroup.group_chargers.size){
            turno=1;
            Send_Data(); //Mandar mis datos
        }

        //Si nos llega alguna orden de borrado o pausa, enviarla al resto
        if(!ChargingGroup.Params.GroupActive){
            memcpy(buffer,"Pause",6);
            coap.put(ChargingGroup.MasterIP, 5683, "Control", buffer);
            delay(250);
            break;
        }

        if(ChargingGroup.DeleteOrder || ChargingGroup.StopOrder){
            memcpy(buffer,"Delete",6);
            coap.put(ChargingGroup.MasterIP, 5683, "Control", buffer);
            delay(250);
            break;
        }
      }

      //Avisar al maestro de que seguimos aqui
      if(!ChargingGroup.Params.GroupMaster){ 
        coap.put(ChargingGroup.MasterIP, 5683, "Ping", ConfigFirebase.Device_Id);
      }
      
      delay(ChargingGroup.Params.GroupMaster? 500:2000);   
      coap.loop();
    }
    ChargingGroup.Conected = false;
    vTaskDelete(NULL);
}

void coap_start() {
  coap.set_udp(Udp);
  ChargingGroup.Conected = true;
  if(ChargingGroup.Params.GroupMaster){

    printf("Arrancando servidor COAP\n");

    //Preguntar si hay maestro en el grupo
    for(uint8_t i =0; i<10;i++){
        broadcast_a_grupo("Hay maestro?");
        delay(5);
        if(!ChargingGroup.Params.GroupMaster)break;
    }
    
    if(ChargingGroup.Params.GroupMaster){
        broadcast_a_grupo("Start client");

        //Ponerme el primero en el grupo para indicar que soy el maestro
        if(ChargingGroup.group_chargers.size>0 && check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) != 255){
            while(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                carac_charger OldMaster=ChargingGroup.group_chargers.charger_table[0];
                remove_from_group(OldMaster.name, &ChargingGroup.group_chargers);
                add_to_group(OldMaster.name, OldMaster.IP, &ChargingGroup.group_chargers);
                ChargingGroup.group_chargers.charger_table[ChargingGroup.group_chargers.size-1].Fase=OldMaster.Fase;
            }
            store_group_in_mem(&ChargingGroup.group_chargers);
        }
        else{
            //Si el grupo está vacio o el cargador no está en el grupo,
            printf("No tengo cargadores en el grupo o no estoy en mi propio grupo!\n");
            ChargingGroup.Params.GroupMaster = false;
            ChargingGroup.Params.GroupActive = false;
            ChargingGroup.Conected = false;
            return;
        }

        coap.server(callback_PARAMS,  "Params");
        coap.server(callback_DATA, "Data");
        coap.server(callback_CONTROL, "Control");
        coap.server(callback_CHARGERS, "Chargers");
        coap.server(callback_PING, "Ping");
    }
    else{
        Serial.println("Ya hay un maestro en el grupo, espero a que me ordene conectarme!");
    }
  }
  
  coap.response(callback_response);

  // start coap server/client
  coap.start();
  
  xTaskCreate(coap_loop,"coap", 4096,NULL,2,NULL);
}

