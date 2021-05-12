
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

static StackType_t xCoapStack [4096*4]     EXT_RAM_ATTR;
StaticTask_t xCoapBuffer ;
TaskHandle_t xCoapHandle = NULL;
TickType_t   xMasterTimer = 0;

uint8_t check_in_group(const char* ID, carac_chargers* group);
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);
bool remove_from_group(const char* ID ,carac_chargers* group);
void store_group_in_mem(carac_chargers* group);
void broadcast_a_grupo(char* Mensaje, uint16_t size);
void send_to(IPAddress IP,  char* Mensaje);
void coap_put( char* Topic, char* Message);
IPAddress get_IP(const char* ID);

void start_server();
void start_client();

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

  coap_put("DATA", my_json_string);

  free(my_json_string);
  ChargingGroup.SendNewData = false;     
}

//enviar mi grupo de cargadores
void Send_Chargers(){
  char buffer[500];
  printf("Sending chargers!\n");
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
  coap_put("CHARGERS", buffer);
}

//Enviar parametros
void Send_Params(){ 
  printf("Sending params!\n");
  cJSON *Params_Json;
  Params_Json = cJSON_CreateObject();

  cJSON_AddNumberToObject(Params_Json, "cdp", ChargingGroup.Params.CDP);
  cJSON_AddNumberToObject(Params_Json, "contract", ChargingGroup.Params.ContractPower);
  cJSON_AddNumberToObject(Params_Json, "active", ChargingGroup.Params.GroupActive);
  cJSON_AddNumberToObject(Params_Json, "master", ChargingGroup.Params.GroupMaster);
  cJSON_AddNumberToObject(Params_Json, "inst_max", ChargingGroup.Params.inst_max);
  cJSON_AddNumberToObject(Params_Json, "pot_max", ChargingGroup.Params.potencia_max);
  cJSON_AddNumberToObject(Params_Json, "userID", ChargingGroup.Params.UserID);
  char *my_json_string = cJSON_Print(Params_Json);   
  cJSON_Delete(Params_Json); 
  
  coap_put("PARAMS", my_json_string);

  free(my_json_string);
     
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  xMasterTimer = xTaskGetTickCount();
  Serial.println("[Coap Response got]");
  
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';
  
  Serial.println(p);

  switch(packet.messageid){
    case GROUP_PARAMS: 
      New_Params(p, packet.payloadlen);
      break;
    case GROUP_CONTROL:
      New_Control(p, packet.payloadlen);
      break;
    case GROUP_CHARGERS:
      Serial.println(p);
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
    ChargingGroup.Params.GroupActive = false;
    int delai = 30000;
    Serial.println("Necesitamos un nuevo maestro!");
    while(!ChargingGroup.Conected){
        if(pdTICKS_TO_MS(xTaskGetTickCount() - xStart) > delai){ //si pasan 30 segundos, elegir un nuevo maestro
            delai=10000;           

            if(!memcmp(ChargingGroup.group_chargers.charger_table[reintentos].name,ConfigFirebase.Device_Id,8)){
                ChargingGroup.Params.GroupActive = true;
                ChargingGroup.Params.GroupMaster = true;
                printf("Soy el nuevo maestro!\n");
                break;
            }
            else{
                xStart = xTaskGetTickCount();
                reintentos++;
                if(reintentos == ChargingGroup.group_chargers.size){
                    //Ultima opcion, mirar si yo era el maestro
                    if(!memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id,8)){
                        ChargingGroup.Params.GroupActive = true;
                        ChargingGroup.Params.GroupMaster = true;
                        printf("Soy el nuevo maestro2!\n");
                    }
                    else{
                      printf("Ya he mirado todos los equipos y no estoy!\n");
                    }
                    
                    break;
                }
            }
        }
        if(!Coms.ETH.conectado){
            break;
        }
        delay(delai);
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
          ChargingGroup.Params.ContractPower = 142;     
          Send_Params();
          ChargingGroup.SendNewParams = false;
      }

      //Enviar los cargadores de nuestro grupo
      else if(ChargingGroup.SendNewGroup){
          Send_Chargers();
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
        

        //Pedir datos a los esclavos para que no envíen todos a la vez, obviando a los que no tenemos en la lista
        IPAddress TurnoIP = get_IP(ChargingGroup.group_chargers.charger_table[turno].name);
        turno++;

        if(TurnoIP!= INADDR_NONE && TurnoIP != INADDR_ANY){ 
          //coap.sendResponse(TurnoIP, 5683, SEND_DATA, "X");
        }
        
        if(turno == ChargingGroup.group_chargers.size){
            turno=1;
            Send_Data(); //Mandar mis datos
        }
      }

      //Si nos llega alguna orden de borrado o pausa, enviarla al resto
      if(!ChargingGroup.Params.GroupActive){
          memcpy(buffer,"Pause",6);
          coap_put("CONTROL", buffer);
          delay(250);
          break;
      }

      if(ChargingGroup.DeleteOrder || ChargingGroup.StopOrder){
          memcpy(buffer,"Delete",6);
          coap_put("CONTROL", buffer);
          delay(250);
          break;
      }
      
      delay(ChargingGroup.Params.GroupMaster? 500:1500);   
    }
    printf("Coap detenido\n");
    ChargingGroup.Conected = false;
    xCoapHandle = NULL;
    vTaskDelete(NULL);
}

void coap_start_server(){

  ChargingGroup.Conected = true;
  if(ChargingGroup.Params.GroupMaster){

    printf("Arrancando servidor COAP\n");

    //Preguntar si hay maestro en el grupo
    for(uint8_t i =0; i<10;i++){
        broadcast_a_grupo("Hay maestro?", 12);
        delay(5);
        if(!ChargingGroup.Params.GroupMaster)break;
    }
    if(ChargingGroup.Params.GroupMaster){
        broadcast_a_grupo("Start client", 12);

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
        
        ChargingGroup.MasterIP.fromString(String(ip4addr_ntoa(&Coms.ETH.IP)));
        start_server();
    }
  }
}

void coap_start_client(){
  start_client();
  if(xCoapHandle == NULL){
    xCoapHandle = xTaskCreateStatic(coap_loop,"coap", 4096*4,NULL,2,xCoapStack,&xCoapBuffer);
  }
}

