#include "group_control.h"
#include "control.h"
#include <string.h>
#include "helpers.h"
#include "cJSON.h"

extern carac_Params Params;
extern carac_Status Status;
extern carac_Comands Comands;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_group    ChargingGroup;

bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);
void remove_group(carac_chargers* group);
uint8_t check_in_group(const char* ID, carac_chargers* group);
void mqtt_publish(char* Topic, char* Data, size_t data_size, size_t topic_size);
void store_group_in_mem(carac_chargers* group);
IPAddress get_IP(const char* ID);

TickType_t xStart;
TickType_t xStart1;

static carac_fase Fases[3];

int Delta_total;

uint8_t Consumo_total = 0;
uint8_t Conex_Delta;
uint8_t Conex;

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
    if(cJSON_HasObjectItem(mensaje_Json,"fase")){
      Cargador.Fase = cJSON_GetObjectItem(mensaje_Json,"fase")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"current")){
      Cargador.Current = cJSON_GetObjectItem(mensaje_Json,"current")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"Perm")){
      Cargador.Baimena = cJSON_GetObjectItem(mensaje_Json,"Perm")->valueint;
    }

    //Datos para los trifasicos
    if(cJSON_HasObjectItem(mensaje_Json,"currentB")){
      Cargador.CurrentB = cJSON_GetObjectItem(mensaje_Json,"currentB")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"currentC")){
      Cargador.CurrentC = cJSON_GetObjectItem(mensaje_Json,"DeltcurrentC")->valueint;
    }

    cJSON_Delete(mensaje_Json);

    //Actualizacion del grupo total
    //Buscar el equipo en el grupo total
    uint8_t index = check_in_group(Cargador.name,&ChargingGroup.group_chargers);                  
    if(index < 255){
        Cargador.Conected = true;
        Cargador.Consigna = ChargingGroup.group_chargers.charger_table[index].Consigna;
        ChargingGroup.group_chargers.charger_table[index] = Cargador;
        ChargingGroup.group_chargers.charger_table[index].Period = 0;
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

  uint8_t buffer[7];

  cJSON *mensaje_Json = cJSON_Parse(Data);

  //comprobar que el Json está bien
  if(!cJSON_HasObjectItem(mensaje_Json,"cdp")){
    return;
  }
  
  //Extraer los datos
  buffer[0] = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"master")->valueint;
  buffer[1] = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"active")->valueint;
  buffer[2] = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"inst_max")->valueint;
  buffer[3]=  (uint8_t) cJSON_GetObjectItem(mensaje_Json,"cdp")->valueint;
  buffer[4] = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"contract")->valueint;
  buffer[5] = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"userID")->valueint;
  buffer[6]=  (uint8_t) cJSON_GetObjectItem(mensaje_Json,"pot_max")->valueint;

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

    char buffer[7];
    memcpy(&buffer,&ChargingGroup.Params,7);
    SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 

  }
  else if(!memcmp(Data,"Delete",6)){
    printf("Tengo que borrar el grupo\n");
    ChargingGroup.Params.GroupActive = 0;
    ChargingGroup.Params.GroupMaster = 0;
    ChargingGroup.DeleteOrder = false;
    ChargingGroup.StopOrder = true;
    
    remove_group(&ChargingGroup.group_chargers);
    store_group_in_mem(&ChargingGroup.group_chargers);

    char buffer[7];
    memcpy(&buffer,&ChargingGroup.Params,7);
    SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 
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

  //compribar que el Json está bien
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

  if(desired_current!=Comands.desired_current &&  (!memcmp(Status.HPT_status,"C2",2) || ChargingGroup.ChargPerm)){
      SendToPSOC5(desired_current,MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
  }

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
    char ID[8];
    char n[2];
    memcpy(n,Data,2);
    uint8_t numero_de_cargadores = atoi(n);

    //crear una copia temporal para almacenar los que están cargando
    carac_chargers temp_chargers;

    for(uint8_t i=0; i<ChargingGroup.group_chargers.size;i++){    
      if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT, "C2",2)){
          memcpy(temp_chargers.charger_table[temp_chargers.size].name,ChargingGroup.group_chargers.charger_table[i].name,9);
          temp_chargers.charger_table[temp_chargers.size].Current = ChargingGroup.group_chargers.charger_table[i].Current;
          temp_chargers.charger_table[temp_chargers.size].CurrentB = ChargingGroup.group_chargers.charger_table[i].CurrentB;
          temp_chargers.charger_table[temp_chargers.size].CurrentC = ChargingGroup.group_chargers.charger_table[i].CurrentC;

          temp_chargers.charger_table[temp_chargers.size].Delta = ChargingGroup.group_chargers.charger_table[i].Delta;
          temp_chargers.charger_table[temp_chargers.size].Consigna = ChargingGroup.group_chargers.charger_table[i].Consigna;
          temp_chargers.charger_table[temp_chargers.size].Delta_timer = ChargingGroup.group_chargers.charger_table[i].Delta_timer;
          
          
          temp_chargers.size ++;
      }
    }

    ChargingGroup.group_chargers.size = 0;

    for(uint8_t i=0; i<numero_de_cargadores;i++){    
        for(uint8_t j =0; j< 8; j++){
            ID[j]=(char)Data[2+i*9+j];
        }

        add_to_group(ID, get_IP(ID), &ChargingGroup.group_chargers);

        ChargingGroup.group_chargers.charger_table[i].Fase = Data[10+i*9]-'0';

        uint8_t index =check_in_group(ID, &temp_chargers);
        if(index != 255){
          memcpy(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2);
          ChargingGroup.group_chargers.charger_table[i].Current = temp_chargers.charger_table[index].Current;
          ChargingGroup.group_chargers.charger_table[i].CurrentB = temp_chargers.charger_table[index].CurrentB;
          ChargingGroup.group_chargers.charger_table[i].CurrentC = temp_chargers.charger_table[index].CurrentC;
          ChargingGroup.group_chargers.charger_table[i].Delta = temp_chargers.charger_table[index].Delta;
          ChargingGroup.group_chargers.charger_table[i].Consigna = temp_chargers.charger_table[index].Consigna;
          ChargingGroup.group_chargers.charger_table[i].Delta_timer = temp_chargers.charger_table[index].Delta_timer;
        }
        

        if(!memcmp(ID,ConfigFirebase.Device_Id,8)){
            ChargingGroup.group_chargers.charger_table[i].Conected = true;
            Params.Fase =Data[10+i*9]-'0';
        }
    }

    store_group_in_mem(&ChargingGroup.group_chargers);
    print_table(ChargingGroup.group_chargers, "Grupo desde COAP");
}

void LimiteConsumo(void *p){
  static uint8_t Estado_Anterior = REPOSO;
  static uint8_t ControlGrupoState = REPOSO;
  static uint8_t LastConex = 0;


  TickType_t Delta_Timer = xTaskGetTickCount();

  Fases[0].numero = 1;
  Fases[1].numero = 2;
  Fases[2].numero = 3;

  while (1){

    switch(ControlGrupoState){
      //Estado reposo, no hay nadie cargando
      case REPOSO:
        for(int i = 0; i < ChargingGroup.group_chargers.size; i++){
          if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT, "B1", 2) && ChargingGroup.group_chargers.charger_table[i].Baimena){
            printf("%s me esta pidiendo permiso! \n", ChargingGroup.group_chargers.charger_table[i].name);
            printf("Se lo doy porque no hay nadie mas cargando");
            memcpy(ChargingGroup.group_chargers.charger_table[i].HPT, "C2", 2);
            ControlGrupoState = CALCULO;
          }
          else{
            ChargingGroup.group_chargers.charger_table[i].Consigna = 0;
          }
        }
          
      break;

      case CALCULO:{
        //Calculo general
        Calculo_General();

        //Ver corriente y consigna disponible total
        float Corriente_disponible_total = ChargingGroup.Params.potencia_max / Conex;

        //Repartir toda la potenica disponible viendo cual es la mas pequeña
        for(int i = 0; i < ChargingGroup.group_chargers.size; i++){
          if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2)){
            ChargingGroup.group_chargers.charger_table[i].Consigna = Fases[ChargingGroup.group_chargers.charger_table[i].Fase-1].corriente_disponible >= Corriente_disponible_total ? Corriente_disponible_total : Fases[ChargingGroup.group_chargers.charger_table[i].Fase-1].corriente_disponible;
            ChargingGroup.group_chargers.charger_table[i].Consigna = ChargingGroup.group_chargers.charger_table[i].Consigna > 32 ? 32 : ChargingGroup.group_chargers.charger_table[i].Consigna;
          }
        }

        //Repartir la corriente sobrante
        if(Delta_total > 0){
          uint16_t Delta_disponible = Delta_total / ( Conex - Conex_Delta);
          for(int i = 0; i < ChargingGroup.group_chargers.size; i++){
            if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2)){
              if(ChargingGroup.group_chargers.charger_table[i].Delta > 0){
                ChargingGroup.group_chargers.charger_table[i].Consigna = ChargingGroup.group_chargers.charger_table[i].Current / 100;
              }
              else{
                uint16_t current_limit = ChargingGroup.Params.inst_max > ChargingGroup.Params.potencia_max ? ChargingGroup.Params.potencia_max :  ChargingGroup.Params.inst_max;
                if(ChargingGroup.group_chargers.charger_table[i].Consigna + Delta_disponible > current_limit){
                  ChargingGroup.group_chargers.charger_table[i].Consigna = ChargingGroup.group_chargers.charger_table[i].Consigna +Delta_disponible > 32 ? 32 : ChargingGroup.group_chargers.charger_table[i].Consigna + Delta_disponible;
                } 
              }
            }
          }
        }

        //Check 
        int Suma_Consignas = 0;
        int Suma_Consignas1 = 0;
        int Suma_Consignas2 = 0;
        int Suma_Consignas3 = 0;
        
        for(int i = 0; i < ChargingGroup.group_chargers.size; i++){
          Suma_Consignas += ChargingGroup.group_chargers.charger_table[i].Consigna; 

          if(ChargingGroup.group_chargers.charger_table[i].Fase ==1){
            Suma_Consignas1 += ChargingGroup.group_chargers.charger_table[i].Consigna;
          }

          if(ChargingGroup.group_chargers.charger_table[i].Fase ==2){
            Suma_Consignas2 += ChargingGroup.group_chargers.charger_table[i].Consigna;
          }

          if(ChargingGroup.group_chargers.charger_table[i].Fase ==3){
            Suma_Consignas3 += ChargingGroup.group_chargers.charger_table[i].Consigna;
          }
        }

        printf("Sumas de las consgnas %i %i %i %i \n", Suma_Consignas1, Suma_Consignas2, Suma_Consignas3, Suma_Consignas);


        ControlGrupoState = EQUILIBRADO;
      break;
      }

      case EQUILIBRADO:{
        //Calculo general
        Calculo_General();

        //Comprobar si ha cambiado el numero de equipos (Alguien se desconecta)
        if(LastConex != Conex){
          ControlGrupoState = Conex == 0 ? REPOSO : CALCULO;
          break;
        }

        //Comprobar si algun equipo quiere comenzar a cargar
        for(int i = 0; i < ChargingGroup.group_chargers.size; i++){
          if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT, "B1", 2) && ChargingGroup.group_chargers.charger_table[i].Baimena){
            printf("%s me esta pidiendo permiso! \n", ChargingGroup.group_chargers.charger_table[i].name);

            //Compruebo si entraria en el grupo con la corriente general
            if(ChargingGroup.Params.potencia_max / (Conex+1) > 6){
              //Compruebo si entraria en la fase 
              if(Fases[ChargingGroup.group_chargers.charger_table[i].Fase-1].corriente_disponible / (Fases[ChargingGroup.group_chargers.charger_table[i].Fase-1].conex +1) > 6){
                //TODO: Añadir circuito!!!
                printf("Entra en el grupo!\n");
                memcpy(ChargingGroup.group_chargers.charger_table[i].HPT, "C2", 2);
                ControlGrupoState = CALCULO;
                break;
              }
            }

            printf("No hay sitio en el grupo!\n");
            ChargingGroup.group_chargers.charger_table[i].Consigna = 0;
          }
        }

        uint16_t transcurrido = pdTICKS_TO_MS (xTaskGetTickCount() -Delta_Timer);

        //Comprobar si alguno de los cargadores tiene corriente de sobra
        for(int i = 0; i < ChargingGroup.group_chargers.size; i++){
          if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2)){
            uint16_t Delta_cargador = ChargingGroup.group_chargers.charger_table[i].Consigna - ChargingGroup.group_chargers.charger_table[i].Current;

            if(Delta_cargador > 2){
              ChargingGroup.group_chargers.charger_table[i].Delta_timer += transcurrido;
            }
            else{
              ChargingGroup.group_chargers.charger_table[i].Delta_timer = 0;
            }

            //Si transcurre un minuto sin alcanzar la consigna
            if(ChargingGroup.group_chargers.charger_table[i].Delta_timer > 60000){
              ChargingGroup.group_chargers.charger_table[i].Delta = Delta_cargador;
              ChargingGroup.group_chargers.charger_table[i].Delta_timer = 0;
              ControlGrupoState = CALCULO;
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
    delay(250);
  }
}

void Calculo_General(){
    Conex=0;
    Consumo_total = 0;
    Delta_total = 0;
    Conex_Delta = 0;
    uint16_t total_pc =0;

    //Resetear valores
    for(uint8_t i =0; i < 3; i++){
        Fases[i].conex = 0;
        Fases[i].corriente_total = 0;
        Fases[i].consigna_total  = 0;
    }

    for(int i = 0; i < ChargingGroup.group_chargers.size; i++){

        //Datos por Grupo
        if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2) && ChargingGroup.group_chargers.charger_table[i].Current >500){
            Conex++;
        }

        total_pc += ChargingGroup.group_chargers.charger_table[i].Current;

        //Datos por fase
        for(uint8_t i =0; i < 3; i++){
            if(ChargingGroup.group_chargers.charger_table[i].Fase == Fases[i].numero){
              if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2) && ChargingGroup.group_chargers.charger_table[i].Current >500){
                  Fases[i].conex++;
                  Fases[i].corriente_total += ChargingGroup.group_chargers.charger_table[i].Current;
                  Fases[i].consigna_total += ChargingGroup.group_chargers.charger_table[i].Consigna;
              }
              break;
            }
        }

        //Comprobar si alguno tiene corriente de sobra
        if(ChargingGroup.group_chargers.charger_table[i].Delta > 0){
          Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;
          Conex_Delta ++;
        }
    }   


    for(uint8_t i =0; i < 3; i++){
      Fases[i].corriente_total /= 100;
      //Ver corriente disponible en fase
      Fases[i].corriente_disponible = ChargingGroup.Params.inst_max / Fases[i].conex;
    }

    Consumo_total = total_pc/100;

#ifdef DEBUG_GROUPS
    printf("Total PC of phase %i %i %i\n",Fases[0].corriente_total, Fases[1].corriente_total, Fases[3].corriente_total); 
    printf("Total consigna of phase %i %i %i\n",Fases[0].consigna_total, Fases[1].consigna_total, Fases[3].consigna_total); 
    printf("Total PC and Delta %i \n",Consumo_total);    
#endif
}

