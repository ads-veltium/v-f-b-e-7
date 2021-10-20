#include "group_control.h"
#include "control.h"
#if (defined CONNECTED && defined CONNECTED)
#include <string.h>
#include "helpers.h"
#include "cJSON.h"

extern carac_Params Params;
extern carac_Status Status;
extern carac_Comands Comands;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_group    ChargingGroup;
extern carac_charger charger_table[MAX_GROUP_SIZE];
extern carac_circuito Circuitos[MAX_GROUP_SIZE];
extern carac_Contador   ContadorExt;

bool add_to_group(const char* ID, IPAddress IP, carac_charger* group, uint8_t* size);
void remove_group(carac_charger* group, uint8_t* size);
void coap_put( char* Topic, char* Message);
void store_params_in_mem();
uint8_t check_in_group(const char* ID, carac_charger* group, uint8_t size);
void store_group_in_mem(carac_charger* group, uint8_t size);
void broadcast_a_grupo(char* Mensaje, uint16_t size);
IPAddress get_IP(const char* ID);

TickType_t xStart;
TickType_t xStart1;

int Delta_total;
float Corriente_disponible_total = 0;
float corriente_disponible_limitada = 0;
uint8_t Consumo_total = 0;
uint8_t Conex_Delta;
uint8_t Conex;
uint8_t Conex_Con_tri =0;

void Calculo_Consigna(carac_charger* Cargador);
void input_values(carac_charger Cargador);
bool check_perm(carac_charger *Cargador); 

carac_charger New_Data(uint8_t* Buffer, int Data_size){  
  carac_charger cargador;
  if(Data_size <=0){
    return cargador;
  }

  char* Data = (char*) calloc(Data_size, '0');
  memcpy(Data, Buffer, Data_size);

  cargador = New_Data(Data, Data_size);
  free(Data);
  return cargador;

}

//Funcion para procesar los nuevos datos recibidos
carac_charger New_Data(char* Data, int Data_size){

    cJSON *mensaje_Json = cJSON_Parse(Data);
    carac_charger Cargador;

    //comprobar que el Json está bien
    if(!cJSON_HasObjectItem(mensaje_Json,"HPT")){
      cJSON_Delete(mensaje_Json);
      return Cargador;
    }
    
    //Extraer los datos
    memcpy(Cargador.HPT,cJSON_GetObjectItem(mensaje_Json,"HPT")->valuestring,3);

    if(cJSON_HasObjectItem(mensaje_Json,"device_id")){
      memcpy(Cargador.name,cJSON_GetObjectItem(mensaje_Json,"device_id")->valuestring,9);
    }
    if(cJSON_HasObjectItem(mensaje_Json,"current")){
      Cargador.Current = cJSON_GetObjectItem(mensaje_Json,"current")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"Perm")){
      Cargador.Baimena = cJSON_GetObjectItem(mensaje_Json,"Perm")->valueint;

      #ifdef DEBUG_GROUPS
      if(Cargador.Baimena){
        printf("Pidiendo permiso al maestro!\n");
      }
      #endif
    }
    //Datos para los trifasicos
    if(cJSON_HasObjectItem(mensaje_Json,"currentB")){
      Cargador.CurrentB = cJSON_GetObjectItem(mensaje_Json,"currentB")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"currentC")){
      Cargador.CurrentC = cJSON_GetObjectItem(mensaje_Json,"currentC")->valueint;
    }

    cJSON_Delete(mensaje_Json);

    //Actualizacion del grupo total
    //Buscar el equipo en el grupo total
    uint8_t index = check_in_group(Cargador.name, charger_table, ChargingGroup.Charger_number);                  
    if(index < 255){
        charger_table[index].Conected = true;
        charger_table[index].Baimena = Cargador.Baimena;
        
        //si está pidiendo permiso, no actualizamos su hpt ni sus corrientes
        if(!Cargador.Baimena){ 
          if(!(!memcmp( Cargador.HPT, "B1",2) && !memcmp(charger_table[index].HPT, "C2",2))){
            charger_table[index].Current = Cargador.Current;
            charger_table[index].CurrentB = Cargador.CurrentB;
            charger_table[index].CurrentC = Cargador.CurrentC;
            memcpy(charger_table[index].HPT, Cargador.HPT,3);
          }
        }
        else if(memcmp(charger_table[index].HPT, "B1",2) && memcmp(charger_table[index].HPT, "C2",2)){
          charger_table[index].Current = Cargador.Current;
          charger_table[index].CurrentB = Cargador.CurrentB;
          charger_table[index].CurrentC = Cargador.CurrentC;
          memcpy(charger_table[index].HPT, Cargador.HPT,3);
        }
        
        charger_table[index].Period = 0;
        Cargador = charger_table[index];
    }

    return Cargador;
}

void New_Params(uint8_t* Buffer, int Data_size){  
  if(Data_size <=0){
    return;
  }

  char* Data = (char*) calloc(Data_size, '0');
  memcpy(Data, Buffer, Data_size);

  New_Params(Data, Data_size);

  free(Data);
}

//Funcion para recibir nuevos parametros de carga para el grupo
void New_Params(char* Data, int Data_size){    

  ChargingGroup.NewData = true;
  uint8_t buffer[7];

  cJSON *mensaje_Json = cJSON_Parse(Data);

  //comprobar que el Json está bien
  if(!cJSON_HasObjectItem(mensaje_Json,"cdp")){
    return;
  }

  //Si es el maestro, cojemos los datos lo antes posible
  if(ChargingGroup.Params.GroupMaster){
    ChargingGroup.Params.CDP = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"cdp")->valueint;
    ChargingGroup.Params.ContractPower = (uint16_t) cJSON_GetObjectItem(mensaje_Json,"contract")->valueint;
    ChargingGroup.Params.potencia_max  = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"pot_max")->valueint;
  }

  
  //Extraer los datos
  buffer[1] = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"active")->valueint;
  buffer[2]=  (uint8_t) cJSON_GetObjectItem(mensaje_Json,"cdp")->valueint;
  uint16_t Contratada = (uint16_t) cJSON_GetObjectItem(mensaje_Json,"contract")->valueint;
  uint16_t Maxima = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"pot_max")->valueint;
  printf("%i\n", Maxima);
  buffer[3] = (uint8_t) (Contratada  & 0x00FF);
  buffer[4] = (uint8_t) ((Contratada >> 8)  & 0x00FF);
  buffer[5] = (uint8_t) (Maxima  & 0x00FF);
  buffer[6] = (uint8_t) ((Maxima >> 8)  & 0x00FF); 


  cJSON_Delete(mensaje_Json);

  buffer[0] = ChargingGroup.Params.GroupMaster;
  //buffer[1] = ChargingGroup.Params.GroupActive;
  SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 
  delay(50);
}

void New_Control(uint8_t* Buffer, int Data_size){  
  if(Data_size <=0){
    return;
  }

  char* Data = (char*) calloc(Data_size, '0');
  memcpy(Data, Buffer, Data_size);

  New_Control(Data, Data_size);

  free(Data);
}

//Funcion para recibir ordenes de grupo que se hayan enviado a otro cargador
void New_Control(char* Data, int Data_size){

  if(!memcmp(Data,"Pause",5)){
    printf("Tengo que pausar el grupo\n");
    broadcast_a_grupo("Geldituzazu taldea", 18);
    ChargingGroup.StopOrder = true;
    ChargingGroup.Params.GroupActive = 0;
    store_params_in_mem();
  }
  else if(!memcmp(Data,"Delete",6)){
    
    printf("Tengo que borrar el grupo\n");

    
    ChargingGroup.Params.GroupActive   = 0;
    ChargingGroup.Params.GroupMaster   = 0;
    ChargingGroup.Params.ContractPower = 0;
    ChargingGroup.Params.CDP = 0;
    ChargingGroup.Params.potencia_max  = 0;
    ChargingGroup.DeleteOrder = false;
    ChargingGroup.StopOrder   = true;
    
    remove_group(charger_table, &ChargingGroup.Charger_number);
    store_group_in_mem(charger_table,ChargingGroup.Charger_number);
    delay(250);
    store_params_in_mem();
    delay(100);
  }
}

void New_Current(uint8_t* Buffer, int Data_size){
  if(Data_size <=0){
    return;
  }

  char* Data = (char*) calloc(Data_size, '0');
  memcpy(Data, Buffer, Data_size);

  cJSON *mensaje_Json = cJSON_Parse(Data);

  free(Data);

  //comprobar que el Json está bien
  if(!cJSON_HasObjectItem(mensaje_Json,"DC")){
    return;
  }
  
  //si no son mis datos los ignoro
  if(memcmp(ConfigFirebase.Device_Id,cJSON_GetObjectItem(mensaje_Json,"N")->valuestring,9)){
    printf("No son mis datos!\n");
    return;
  }
  //Extraer los datos
  uint8_t desired_current = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"DC")->valueint;
  if(cJSON_HasObjectItem(mensaje_Json,"P")){
    ChargingGroup.ChargPerm = (bool) cJSON_GetObjectItem(mensaje_Json,"P")->valueint;
  }

  cJSON_Delete(mensaje_Json);

  if(desired_current == 0 && !ChargingGroup.ChargPerm && !memcmp(Status.HPT_status,"C2",2)){
    Serial.printf("Debo detener la carga!!!! %i %i\n", desired_current, ChargingGroup.ChargPerm);
    
    SendToPSOC5(1, BLOQUEO_CARGA);
    SendToPSOC5(desired_current,MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
  }

  if(desired_current!=Comands.desired_current &&  (!memcmp(Status.HPT_status,"C2",2) || ChargingGroup.ChargPerm)){
      Serial.printf("Nueva corriente recibida!!!!\n");
      
      SendToPSOC5(desired_current,MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
      if(ChargingGroup.ChargPerm){
        SendToPSOC5(0, BLOQUEO_CARGA);
      }
  }

}

void New_Circuit(uint8_t* Buffer, int Data_size){
  if(Data_size <=0){
    return;
  }
  printf("%s\n", Buffer);
  
  
  char size_char[2];
  memcpy(size_char,Buffer,2);
  uint8_t size = atoi(size_char);

  uint8_t buffer[size+1];

  buffer[0]=size;
  for(uint8_t i=0; i<size;i++){
    buffer[i+1] = Buffer[i+2];
  }

  if(ChargingGroup.Params.GroupMaster){
    ChargingGroup.Circuit_number = size;
    for(int i=0;i< size;i++){
      Circuitos[i].numero = i+1;
      Circuitos[i].Fases[0].numero = 1;
      Circuitos[i].Fases[1].numero = 2;
      Circuitos[i].Fases[2].numero = 3;
      Circuitos[i].limite_corriente = buffer[i+1];			
    }
  }

  SendToPSOC5((char*)buffer,size+1,GROUPS_CIRCUITS); 
}

void New_Group(uint8_t* Buffer, int Data_size){  
  if(Data_size <=0){
    return;
  }

  char* Data = (char*) calloc(Data_size, '0');
  memcpy(Data, Buffer, Data_size);

  New_Group(Data, Data_size);

  free(Data);
}

//Funcion para recibir un nuevo grupo de cargadores
void New_Group(char* Data, int Data_size){
    ChargingGroup.NewData = true;
    char ID[8];
    char n[2];
    memcpy(n,Data,2);
    uint8_t numero_de_cargadores = atoi(n);

    //crear una copia temporal para almacenar los que están cargando
    carac_charger temp_chargers[MAX_GROUP_SIZE];
    uint8_t temp_chargers_size = 0;

    for(uint8_t i=0; i<ChargingGroup.Charger_number;i++){    
          memcpy(temp_chargers[temp_chargers_size].name,charger_table[i].name,9);
          memcpy(temp_chargers[temp_chargers_size].HPT,charger_table[i].HPT,2);
          temp_chargers[temp_chargers_size].Current = charger_table[i].Current;
          temp_chargers[temp_chargers_size].CurrentB = charger_table[i].CurrentB;
          temp_chargers[temp_chargers_size].CurrentC = charger_table[i].CurrentC;

          temp_chargers[temp_chargers_size].Delta = charger_table[i].Delta;
          temp_chargers[temp_chargers_size].Consigna = charger_table[i].Consigna;
          temp_chargers[temp_chargers_size].Delta_timer = charger_table[i].Delta_timer;
          temp_chargers[temp_chargers_size].Fase = charger_table[i].Fase;
          temp_chargers[temp_chargers_size].Circuito = charger_table[i].Circuito;
          
          temp_chargers_size ++;
    }

    ChargingGroup.Charger_number = 0;

    for(uint8_t i=0; i<numero_de_cargadores;i++){    
        for(uint8_t j =0; j< 8; j++){
            ID[j]=(char)Data[2+i*9+j];
        }

        add_to_group(ID, get_IP(ID), charger_table, &ChargingGroup.Charger_number); 
        
        char v[3];
        memcpy(v,&Data[10+i*11],3);
        uint8_t value = atoi(v);

        printf("Crudo fase y circuito %i %i %i\n",value, value & 0x03, value >> 2);



        charger_table[i].Fase = value & 0x03;
        charger_table[i].Circuito = value >> 2;

        uint8_t index =check_in_group(ID, temp_chargers, temp_chargers_size);
        if(index != 255){
          memcpy(charger_table[i].HPT,temp_chargers[index].HPT,2);
          charger_table[i].Current = temp_chargers[index].Current;
          charger_table[i].CurrentB = temp_chargers[index].CurrentB;
          charger_table[i].CurrentC = temp_chargers[index].CurrentC;
          charger_table[i].Delta = temp_chargers[index].Delta;
          charger_table[i].Consigna = temp_chargers[index].Consigna;
          charger_table[i].Delta_timer = temp_chargers[index].Delta_timer;
        }
        

        if(!memcmp(ID,ConfigFirebase.Device_Id,8)){
            charger_table[i].Conected = true;
            Params.Fase = uint8_t(Data[10+i*9]-'0') & 0x03;
        }
    }
    store_group_in_mem(charger_table, ChargingGroup.Charger_number);
    print_table(charger_table, "Grupo desde COAP",ChargingGroup.Charger_number);
}

void LimiteConsumo(void *p){
  static uint8_t Estado_Anterior = REPOSO;
  static uint8_t ControlGrupoState = REPOSO;
  static uint8_t LastConex = 0;

  TickType_t Delta_Timer = xTaskGetTickCount();
  TickType_t Start_Timer = xTaskGetTickCount();

  while (1){
    
    switch(ControlGrupoState){
      //Estado reposo, no hay nadie cargando
      case REPOSO:
        if(pdTICKS_TO_MS(xTaskGetTickCount() - Start_Timer) > 50000){
          for(int i = 0; i < ChargingGroup.Charger_number; i++){
            if(!memcmp(charger_table[i].HPT, "B1", 2) && charger_table[i].Baimena){
              
              //cls();
              float reparto = (ChargingGroup.Params.potencia_max *100/230);
              if(charger_table[i].trifasico){
                reparto = (ChargingGroup.Params.potencia_max *100/230) / 3;
              }
              
              if(reparto >= 6){
                print_table(charger_table, "Grupo en reposo", ChargingGroup.Charger_number);
                memcpy(charger_table[i].HPT, "C2", 2);
                charger_table[i].order =1;
                ControlGrupoState = CALCULO;
              }
            }
            else{
              charger_table[i].Consigna = 0;
            }
          }
        }
          
      break;

      case CALCULO:{
        //cls();
        //Calculo general
        Calculo_General();

        //Ver corriente y consigna disponible total
        if(Conex <= 0){
          ControlGrupoState = REPOSO;
          break;
        }

        //Repartir toda la potencia disponible viendo cual es la mas pequeña
        for(int i = 0; i < ChargingGroup.Charger_number; i++){
          if(!memcmp(charger_table[i].HPT,"C2",2)){
            if(Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].corriente_disponible <= Corriente_disponible_total)
              charger_table[i].Consigna =  Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].corriente_disponible;
            else
              charger_table[i].Consigna = Corriente_disponible_total;
            
            charger_table[i].Consigna = charger_table[i].Consigna > 32 ? 32 : charger_table[i].Consigna;

            //Si es trifasico, tenemos que tener en cuenta tres potencias mas
            if(charger_table[i].CurrentB > 250 || charger_table[i].CurrentC>250){
              if(charger_table[i].Consigna < 6){
                charger_table[i].Consigna = 0;
                charger_table[i].Baimena = 0;
                memcpy(charger_table[i].HPT,"B1",2);
                charger_table[i].Current = 0;
                charger_table[i].Delta = 0;
                charger_table[i].Delta_timer = 0;
                charger_table[i].CurrentB = 0;
                charger_table[i].CurrentC = 0;
                charger_table[i].trifasico =1;
                Conex--;
                Conex_Con_tri -=3;
              }
            }
          }
        }

        //Intentar repartir la corriente sobrante
        if(Delta_total > 0){
          Reparto_Delta();
        }

        ControlGrupoState = EQUILIBRADO;
        break;
      }

      case EQUILIBRADO:{
        cls();
        print_table(charger_table, "Grupo en equilibrado", ChargingGroup.Charger_number);

        //Calculo general
        if(Calculo_General() || ChargingGroup.NewData){
          ChargingGroup.NewData = false;
          ControlGrupoState = CALCULO;
          break;
        }

        //Comprobar si ha cambiado el numero de equipos (Alguien se desconecta)
        if(LastConex != Conex || Conex == 0){
          ControlGrupoState = Conex == 0 ? REPOSO : CALCULO;
          LastConex = Conex;
          break;
        }
        LastConex = Conex;

        //Comprobar si en algun momento nos estamos pasando de los limites
        if(Consumo_total > corriente_disponible_limitada){
          ControlGrupoState = CALCULO;
          break;
        }

        //Comprobar que no nos pasamos de los limites de las fases
        for(uint8_t j =0; j < ChargingGroup.Circuit_number; j++){
          for(uint8_t i =0; i < 3; i++){
            if(Circuitos[j].Fases[i].corriente_total > Circuitos[j].limite_corriente || Circuitos[j].Fases[i].consigna_total > Circuitos[j].limite_corriente){
              ControlGrupoState = CALCULO;
              break;
            }
          }
        }

        if(ControlGrupoState == CALCULO){
          break;
        }
            

        //Comprobar si algun equipo quiere comenzar a cargar
        for(int i = 0; i < ChargingGroup.Charger_number; i++){
          if(!memcmp(charger_table[i].HPT, "B1", 2) && charger_table[i].Baimena){
            //Compruebo si entraria en el grupo con la corriente general
            if(charger_table[i].trifasico ){ //Si el equipo estaba detectado como trifasico, debemos tener en cuenta el triple de corriente
              float reparto = (corriente_disponible_limitada) / (Conex_Con_tri+3);
              if(reparto >= 6){
                printf("Entra en el grupo!\n");
                //Compruebo si entraria en el circuito           
                if(Circuitos[charger_table[i].Circuito-1].limite_corriente / (Circuitos[charger_table[i].Circuito-1].Fases[0].conex +1 )> 6){
                  if(Circuitos[charger_table[i].Circuito-1].limite_corriente / (Circuitos[charger_table[i].Circuito-1].Fases[1].conex +1 )> 6){
                    if(Circuitos[charger_table[i].Circuito-1].limite_corriente / (Circuitos[charger_table[i].Circuito-1].Fases[2].conex +1 )> 6){
                        printf("Entra en el circuito!\n");
                        memcpy(charger_table[i].HPT, "C2", 2);
                        //Asignar el orden de carga
                        uint8_t last_order = 0;
                        for(int j = 0; j < ChargingGroup.Charger_number; j++){
                          if(!memcmp(charger_table[j].HPT,"C2",2) || !memcmp(charger_table[j].HPT,"B2",2)){
                            last_order = last_order < charger_table[j].order ? charger_table[j].order: last_order;
                          }
                        }
                        charger_table[i].order = last_order + 1;
                        ControlGrupoState = CALCULO;
                        break;
                    }
                  }
                }
              }
            }
            else if((corriente_disponible_limitada) / (Conex_Con_tri+1) >= 6){
              printf("Entra en el grupo!\n");
              //Compruebo si entraria en el circuito           
               if(Circuitos[charger_table[i].Circuito-1].limite_corriente / (Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].conex +1 )> 6){
                printf("Entra en el circuito!\n");
                memcpy(charger_table[i].HPT, "C2", 2);
                //Asignar el orden de carga
                uint8_t last_order = 0;
                for(int j = 0; j < ChargingGroup.Charger_number; j++){
                  if(!memcmp(charger_table[j].HPT,"C2",2) || !memcmp(charger_table[j].HPT,"B2",2)){
                    last_order = last_order < charger_table[j].order ? charger_table[j].order: last_order;
                  }
                }
                charger_table[i].order = last_order + 1;
                ControlGrupoState = CALCULO;
                break;
              }
            }

            printf("No hay sitio en el grupo!\n");
            charger_table[i].Consigna = 0;
          }
        }

        uint16_t transcurrido = pdTICKS_TO_MS (xTaskGetTickCount() - Delta_Timer);

        //Comprobar si alguno de los cargadores tiene corriente de sobra (delta) solo si hay mas de uno cargando
        //Si el equipo es trifasico, solo tenemos en cuenta la corriente de su fase principal
        if(Conex >1){
          for(int i = 0; i < ChargingGroup.Charger_number; i++){
            if(!memcmp(charger_table[i].HPT,"C2",2) || !memcmp(charger_table[i].HPT,"B2",2) || !memcmp(charger_table[i].HPT,"C1",2)){
              uint8 Delta_cargador = 0;

              if(charger_table[i].Consigna > charger_table[i].Current / 100)
                Delta_cargador = charger_table[i].Consigna - charger_table[i].Current / 100;
              else
                Delta_cargador = 0;

              if(Delta_cargador > 2){
                charger_table[i].Delta_timer = charger_table[i].Delta_timer + transcurrido;
              }
              else{
                charger_table[i].Delta_timer = 0;
                charger_table[i].Delta = 0;
              }

              //Si transcurre un minuto sin alcanzar la consigna
              if(charger_table[i].Delta_timer > 60000){
                charger_table[i].Delta = Delta_cargador;
                charger_table[i].Delta_timer = 0;
                ControlGrupoState = CALCULO;
              } 
            }
          }
        }
        Delta_Timer = xTaskGetTickCount();

      break;
      }
      

      default:
        printf("La maquina de Control de grupo tiene algun valor malo\n");
      break;
    }

  #ifdef DEBUG_GROUPS
    if(Estado_Anterior != ControlGrupoState){
      printf("La maquina de control de grupo pasa de %i a %i\n ",Estado_Anterior, ControlGrupoState);
      Estado_Anterior = ControlGrupoState;
    }
  #endif
    delay(500);
  }
}

bool Calculo_General(){
    Conex=0;
    Conex_Con_tri = 0;
    Consumo_total = 0;
    Delta_total = 0;
    Conex_Delta = 0;
    uint16_t total_pc =0;
    uint16_t total_consigna=0;
    uint8_t conectados = 0;
    static uint16_t recalc_count =0;

    //Resetear valores
    for(uint8_t i =0; i < ChargingGroup.Circuit_number; i++){
        Circuitos[i].conex = 0;
        Circuitos[i].corriente_total = 0;
        Circuitos[i].consigna_total  = 0;
        Circuitos[i].Fases[0].conex = 0;
        Circuitos[i].Fases[0].corriente_total = 0;
        Circuitos[i].Fases[0].consigna_total = 0;
        Circuitos[i].Fases[1].conex = 0;
        Circuitos[i].Fases[1].corriente_total = 0;
        Circuitos[i].Fases[1].consigna_total = 0;
        Circuitos[i].Fases[2].conex = 0;
        Circuitos[i].Fases[2].corriente_total = 0;
        Circuitos[i].Fases[2].consigna_total = 0;
    }

    //Comprobar cuantos cargadores están conectados
    for(int i = 0; i < ChargingGroup.Charger_number; i++){
      if(charger_table[i].Conected){
        conectados ++;
      }
    }
    //Si no están todos conectadoes, tendremos algunos en un estado desconocido, por seguridad limitamos la potencia máxima
    if(conectados == ChargingGroup.Charger_number){
      corriente_disponible_limitada = floor(ChargingGroup.Params.potencia_max *100/230);
    }
    else{
      corriente_disponible_limitada = floor((ChargingGroup.Params.potencia_max * conectados / ChargingGroup.Charger_number)*100/230);
    }


    for(int i = 0; i < ChargingGroup.Charger_number; i++){

        //Datos por Grupo
        if(!memcmp(charger_table[i].HPT,"C2",2) || !memcmp(charger_table[i].HPT,"C1",2) || !memcmp(charger_table[i].HPT,"B2",2)){
            Conex++;
            Conex_Con_tri++;
            uint8_t faseA, faseB = 0, faseC = 0;
            faseA =charger_table[i].Fase -1;

            //Datos por fase
            Circuitos[charger_table[i].Circuito-1].Fases[faseA].conex++;
            Circuitos[charger_table[i].Circuito-1].Fases[faseA].corriente_total += charger_table[i].Current;
            Circuitos[charger_table[i].Circuito-1].Fases[faseA].consigna_total += charger_table[i].Consigna;
            total_consigna += charger_table[i].Consigna;
            total_pc += charger_table[i].Current;
            
            //Si es trifasico, los datos deben ir a todas las fases
            if(charger_table[i].CurrentB > 250 || charger_table[i].CurrentC > 250){
              charger_table[i].trifasico =1;
              total_consigna += charger_table[i].Consigna *2;
              Conex_Con_tri+=2;
              switch(faseA){
                case 0:
                  faseB = 1;
                  faseC = 2;
                  break;

                case 1:
                  faseB = 2;
                  faseC = 0;
                  break;

                case 2:
                  faseB = 0;
                  faseC = 1;
                  break;
              }
              
              if(charger_table[i].CurrentB > 0 ){
                Circuitos[charger_table[i].Circuito-1].Fases[faseB].conex++;
                Circuitos[charger_table[i].Circuito-1].Fases[faseB].corriente_total += charger_table[i].CurrentB;
                Circuitos[charger_table[i].Circuito-1].Fases[faseB].consigna_total += charger_table[i].Consigna ;
                total_pc += charger_table[i].CurrentB;
              }
              if(charger_table[i].CurrentC > 0 ){
                Circuitos[charger_table[i].Circuito-1].Fases[faseC].conex++;
                Circuitos[charger_table[i].Circuito-1].Fases[faseC].corriente_total += charger_table[i].CurrentC;
                Circuitos[charger_table[i].Circuito-1].Fases[faseC].consigna_total += charger_table[i].Consigna ;
                total_pc += charger_table[i].CurrentC;
              }
            }
            //Comprobar si alguno tiene corriente de sobra
            if(charger_table[i].Delta > 0){
              Delta_total += charger_table[i].Delta;
              Conex_Delta ++;
            }
        }

        //A los que no estan cargando les reseteamos los valores
        else if(!memcmp(charger_table[i].HPT,"A1",2) || !memcmp(charger_table[i].HPT,"0V",2)){
            charger_table[i].Current = 0;
            charger_table[i].Delta = 0;
            charger_table[i].Delta_timer = 0;
            charger_table[i].CurrentB = 0;
            charger_table[i].CurrentC = 0;
            charger_table[i].order = 0;
            charger_table[i].trifasico =0;
        }
       
    }   


    //corrientes por fase
    for(uint8_t j =0; j < ChargingGroup.Circuit_number; j++){
      for(uint8_t i =0; i < 3; i++){
        ceil(Circuitos[j].Fases[i].corriente_total /= 100);
        //Calcular la corriente disponible en cada fase
        if(Circuitos[j].Fases[i].conex > 0){
          Circuitos[j].Fases[i].corriente_disponible = floor(Circuitos[j].limite_corriente/ Circuitos[j].Fases[i].conex);
        }
        else{
          Circuitos[j].Fases[i].corriente_disponible = Circuitos[j].limite_corriente;
        }
        Circuitos[j].Fases[i].corriente_sobrante = Circuitos[j].limite_corriente - Circuitos[j].Fases[i].consigna_total;
      }
    }

    Consumo_total = ceil(total_pc/100);
    
    bool recalcular = false;

    if(Conex_Con_tri > 0){
      
      Corriente_disponible_total = floor(corriente_disponible_limitada / Conex_Con_tri);
    
      if(ChargingGroup.Params.CDP >> 4 && ContadorExt.GatewayConectado){
        float Corriente_CDP__sobra = 0;

        //Comprobar primero cuanta potencia nos sobra
        if(((ChargingGroup.Params.CDP>> 2) & 0x03) == 1){ //Medida domestica
            if(ChargingGroup.Params.ContractPower*100 > ContadorExt.DomesticPower){
              Corriente_CDP__sobra = floor((ChargingGroup.Params.ContractPower*100 - ContadorExt.DomesticPower)/230);
            }
            else{
              Corriente_CDP__sobra = 0;
            }
        }

        else if(((ChargingGroup.Params.CDP >> 2) & 0x03) == 2){ //Medida total 
            if(ChargingGroup.Params.ContractPower*100 > ContadorExt.DomesticPower){
              Corriente_CDP__sobra = floor((ChargingGroup.Params.ContractPower*100 - ContadorExt.DomesticPower)/230) - Consumo_total;
            }
            else{
              Corriente_CDP__sobra = 0;
            }
        }

        //Establecer el limite disponible
        uint16_t Corriente_limite_cdp = Corriente_CDP__sobra;

        
        if(floor(ChargingGroup.Params.potencia_max *100/230) > Corriente_limite_cdp){
          if(conectados == ChargingGroup.Charger_number){
            corriente_disponible_limitada =  Corriente_limite_cdp;
          }
          else{
            corriente_disponible_limitada = Corriente_limite_cdp * conectados / ChargingGroup.Charger_number;
          }
          Corriente_disponible_total = floor(corriente_disponible_limitada / Conex_Con_tri);
        }

        //Ver si nos pasamos
        if(Corriente_limite_cdp < Consumo_total){ //Estamos por encima del limite
            Corriente_disponible_total = floor(corriente_disponible_limitada / Conex_Con_tri);
            recalcular = true;
        }
          printf("Recalcular %i \n",recalcular);
          printf("Corriente_limite_cdp %i \n",Corriente_limite_cdp);
          printf("Consumo_total %i \n",Consumo_total);
          printf("corriente_disponible_limitada %f \n",corriente_disponible_limitada);
          printf("Corriente_CDP__sobra %f \n",Corriente_CDP__sobra);
      }

      //Si tenemos un limite menor que 6A para cada coche, debemos expulsar al ultimo que haya entrado
      while(Corriente_disponible_total < 6){
        //Buscar el último que ha empezado a cargar
        uint8_t last_index = 0;
        uint8_t last_order = 0;
        for(int j = 0; j < ChargingGroup.Charger_number; j++){
          if(!memcmp(charger_table[j].HPT,"C2",2) || !memcmp(charger_table[j].HPT,"B2",2) || !memcmp(charger_table[j].HPT,"C1",2)){
            if(last_order < charger_table[j].order ){
                last_order = charger_table[j].order;
                last_index = j;
            }
          }
        }
        charger_table[last_index].Consigna = 0;
        charger_table[last_index].Baimena = 0;
        memcpy(charger_table[last_index].HPT,"B1",2);
        charger_table[last_index].Current = 0;
        charger_table[last_index].Delta = 0;
        charger_table[last_index].Delta_timer = 0;
        charger_table[last_index].CurrentB = 0;
        charger_table[last_index].CurrentC = 0;
        Conex--;
        Conex_Con_tri --;
        if(charger_table[last_index].trifasico){
          Conex_Con_tri -=2;
        }
        if(Conex_Con_tri > 0){
          Corriente_disponible_total = floor(corriente_disponible_limitada / Conex_Con_tri);
        }
        else{
          break;
        }
      }
    }

    if((corriente_disponible_limitada-total_consigna) > 5) {
      if(++recalc_count > 25){
        recalcular = true;
        recalc_count = 0;
      }
      
    }


#ifdef DEBUG_GROUPS
    printf("Total Current %f\n",corriente_disponible_limitada); 
    printf("Consumo total %i \n\n",Consumo_total); 
    printf("Consigna total %i \n\n",total_consigna); 
    printf("Corriente_disponible_total %f \n\n",Corriente_disponible_total); 
#endif
    return recalcular;
}

void Reparto_Delta(){

  //Ajustamos las consignas de los que les sobra corriente
  for(int i = 0; i < ChargingGroup.Charger_number; i++){
      if(!memcmp(charger_table[i].HPT,"C2",2) || !memcmp(charger_table[i].HPT,"B2",2) || !memcmp(charger_table[i].HPT,"C1",2)){

        //Al que le sobra la corriente le ajustamos su consigna:
        if(charger_table[i].Delta > 0){
          charger_table[i].Consigna = charger_table[i].Current / 100;
          if(charger_table[i].Consigna <6)
            charger_table[i].Consigna =6;
        }
      }
  }

  //Recalculamos limites
  Calculo_General();

  //Mientras tengamos delta a repartir:
  uint8_t i = 0;
  uint8_t Last_Delta = Delta_total;
  while(Delta_total > 0){
    if((!memcmp(charger_table[i].HPT,"C2",2) || !memcmp(charger_table[i].HPT,"B2",2) || !memcmp(charger_table[i].HPT,"C1",2)) && charger_table[i].Delta == 0 && charger_table[i].Consigna < 32){
      //Equipo monofásico
      if(charger_table[i].CurrentB == 0 && charger_table[i].CurrentC == 0){
        //Comprobar si la fase de este cargador me permite ampliar
        if(Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].corriente_sobrante - 1 > 0){
          //comprobar que el circuito me permite añadir carga
            Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].corriente_sobrante --;
            Delta_total --;
            charger_table[i].Consigna ++;
          //}
        }
      }

      //Equipo trifásico
      else {
        //Comprobar si tengo tres amperios para repartir
        if(Delta_total >= 3 && charger_table[i].Consigna < 32){
          //Comprobar si las tres fases me permiten ampliar
          if(Circuitos[charger_table[i].Circuito-1].Fases[0].corriente_sobrante - 1 > 0){
            if(Circuitos[charger_table[i].Circuito-1].Fases[1].corriente_sobrante - 1 > 0){
              if(Circuitos[charger_table[i].Circuito-1].Fases[2].corriente_sobrante - 1 > 0){
                Circuitos[charger_table[i].Circuito-1].Fases[0].corriente_sobrante --;
                Circuitos[charger_table[i].Circuito-1].Fases[1].corriente_sobrante --;
                Circuitos[charger_table[i].Circuito-1].Fases[2].corriente_sobrante --;
                Delta_total -= 3;
                charger_table[i].Consigna ++;
              }
            }
          }
        }
      }
    }

    i = i+1 >= ChargingGroup.Charger_number ? 0 : i+1; 

    delay(10);
    if(i==0){
      //Si tras dar la vuelta a todo el grupo no he recolocado nada de corriente, es que nadie quiere mas
      if(Last_Delta == Delta_total){
        break;
      }
      Last_Delta = Delta_total;
    }
  }
}

#endif