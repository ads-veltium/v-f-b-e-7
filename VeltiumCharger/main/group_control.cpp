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

uint8_t  is_c3_charger = IN_CochesConectados;         
uint8_t  is_LimiteConsumo;
uint8_t  is_LimiteInstalacion;
uint8_t  Delta_total;
uint8_t  Delta_total_fase;
uint8_t  desired_current;

int Conex;
int Conex_Delta;
int Conex_Delta_Fase;
int Conex_Fase;
int Fases_limitadas;
int Comand_no_lim;
int tmp;

uint8_t Pc;
uint8_t Pc_Fase;

void Calculo_Consigna(carac_charger* Cargador);
void input_values(carac_charger Cargador);
bool check_perm(carac_charger Cargador); 

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
    bool baimena = false;
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
    if(cJSON_HasObjectItem(mensaje_Json,"limite_fase")){
      Cargador.limite_fase = cJSON_GetObjectItem(mensaje_Json,"limite_fase")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"fase")){
      Cargador.Fase = cJSON_GetObjectItem(mensaje_Json,"fase")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"current")){
      Cargador.Current = cJSON_GetObjectItem(mensaje_Json,"current")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"Delta")){
      Cargador.Delta = cJSON_GetObjectItem(mensaje_Json,"Delta")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"Perm")){
      baimena = cJSON_GetObjectItem(mensaje_Json,"Perm")->valueint;
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
        ChargingGroup.group_chargers.charger_table[index]=Cargador;
        ChargingGroup.group_chargers.charger_table[index].Period=0;
        //cls();
        //print_table(ChargingGroup.group_chargers, "Grupo total");
        if(!memcmp(Cargador.HPT, "B1", 2)&& baimena){
          if(check_perm(Cargador)){
            input_values(Cargador);
            Calculo_Consigna(&Cargador);
          }
          else{
            Cargador.DesiredCurrent = 0;
          }
        }
        else if(!memcmp(Cargador.HPT, "C2", 2)){
          input_values(Cargador);
          Calculo_Consigna(&Cargador);
        }
        else{
          Cargador.DesiredCurrent =0;
        }
        
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

  char *Jsonstring =cJSON_Print(mensaje_Json);
  printf("%s\n",Jsonstring);
  free(Jsonstring);

  //compribar que el Json está bien
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
  Status.limite_Fase= (uint8_t) cJSON_GetObjectItem(mensaje_Json,"LF")->valueint;
  Status.Delta = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"D")->valueint;
  ChargingGroup.ChargPerm = (bool) cJSON_GetObjectItem(mensaje_Json,"P")->valueint;

  cJSON_Delete(mensaje_Json);

  if(desired_current!=Comands.desired_current &&  !memcmp(Status.HPT_status,"C2",2)){
      //SendToPSOC5(desired_current,MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
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
    ChargingGroup.group_chargers.size = 0;

    for(uint8_t i=0; i<numero_de_cargadores;i++){    
        for(uint8_t j =0; j< 8; j++){
            ID[j]=(char)Data[2+i*9+j];
        }
        add_to_group(ID, get_IP(ID), &ChargingGroup.group_chargers);
        ChargingGroup.group_chargers.charger_table[i].Fase = Data[10+i*9]-'0';

        if(!memcmp(ID,ConfigFirebase.Device_Id,8)){
            ChargingGroup.group_chargers.charger_table[i].Conected = true;
            Params.Fase =Data[10+i*9]-'0';
        }
    }
    store_group_in_mem(&ChargingGroup.group_chargers);
    print_table(ChargingGroup.group_chargers, "Grupo desde COAP");
}

// Function for Chart: '<Root>/Charger 1'
void LimiteConsumo(carac_charger* Cargador){
  if (ChargingGroup.Params.inst_max < Pc_Fase) {
    Cargador->Delta = 0;
    is_LimiteConsumo = IN_NO_ACTIVE_CHILD;
    is_c3_charger = IN_LimiteInstalacion;

    Cargador->limite_fase = 1;

    if (Delta_total_fase == 0) {
      is_LimiteInstalacion = IN_Limitacion;
      desired_current = ChargingGroup.Params.inst_max / Conex_Fase;
    } 
    else if (Delta_total_fase > 0){
      is_LimiteInstalacion = IN_Limitacion1;
      desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) + ChargingGroup.Params.inst_max / Conex_Fase;
    }
  } 
  else if (memcmp(Cargador->HPT,"C2",2) || Cargador->Current < 500) {
    is_LimiteConsumo = IN_NO_ACTIVE_CHILD;
    is_c3_charger = IN_CochesConectados;
    desired_current = 0;    
  } 
  else {
 
    Comand_no_lim = desired_current;
    Cargador->limite_fase = 0;

    switch (is_LimiteConsumo) {
     case IN_Cargando1:

        tmp = desired_current * Conex;

        if (tmp > ChargingGroup.Params.potencia_max) {
          is_LimiteConsumo = IN_Cargando2;

          desired_current = ChargingGroup.Params.potencia_max ;
        } else if ((Pc >= ChargingGroup.Params.potencia_max) && (tmp < ChargingGroup.Params.potencia_max)) {
          is_LimiteConsumo = IN_ReduccionPc;

          desired_current = ChargingGroup.Params.potencia_max / Conex;
        } else {

          desired_current = ChargingGroup.Params.potencia_max ;
        }
        break;

     case IN_Cargando2:

        if (Pc >= ChargingGroup.Params.potencia_max) {
          is_LimiteConsumo = IN_ReduccionPc;

          desired_current = ChargingGroup.Params.potencia_max / Conex;
        } 
        else if (desired_current * Conex < ChargingGroup.Params.potencia_max) {
          is_LimiteConsumo = IN_Cargando1;

          desired_current = ChargingGroup.Params.potencia_max ;
        } 
        else {
          desired_current = ChargingGroup.Params.potencia_max ;
        }
        break;

     case IN_Cargando3:

        if (ChargingGroup.Params.potencia_max > Conex * ChargingGroup.Params.potencia_max ) {
          is_LimiteConsumo = IN_Cargando1;
    
          desired_current = ChargingGroup.Params.potencia_max ;
        } 
        else if (desired_current - Cargador->Current > 6) {
          is_LimiteConsumo = IN_Contador;

          xStart = xTaskGetTickCount();
          
        } else if ((Fases_limitadas > 0) && (ChargingGroup.Params.potencia_max > (ChargingGroup.Params.potencia_max  - desired_current) + Pc)) {
          is_LimiteConsumo = IN_Cargando4;

          desired_current = ChargingGroup.Params.potencia_max ;
        } else {

          desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

          
        }
        break;

     case IN_Cargando4:

        if (ChargingGroup.Params.potencia_max <= (ChargingGroup.Params.potencia_max  - desired_current) + Pc) {
          is_LimiteConsumo = IN_Cargando3;
          desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;
          
        } else if (Fases_limitadas == 0) {
          is_LimiteConsumo = IN_Cargando1;
          desired_current = ChargingGroup.Params.potencia_max ;
        } else {
          desired_current = ChargingGroup.Params.potencia_max ;
        }
        break;

     case IN_Contador:

        if (desired_current - Cargador->Current <= 6) {
          is_LimiteConsumo = IN_Cargando3;
          desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;
          
        } 
        else if (pdTICKS_TO_MS(xTaskGetTickCount()-xStart) >= 20000) {
          is_LimiteConsumo = IN_Repartir;
          desired_current = ChargingGroup.Params.potencia_max / Conex;
          Cargador->Delta = desired_current - Cargador->Current;
        }
        break;

     case IN_ReduccionPc:

        if ((ChargingGroup.Params.potencia_max > Pc) && (desired_current * Conex <= ChargingGroup.Params.potencia_max)) {
          is_LimiteConsumo = IN_Cargando3;
          desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;
          
        } else {
          desired_current = ChargingGroup.Params.potencia_max / Conex;
        }
        break;

     default:

      // case IN_Repartir:
        if (desired_current - Cargador->Current <= 6) {
          Status.Delta = 0;
          is_LimiteConsumo = IN_Cargando3;
          desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;
          
        } else {
          desired_current = ChargingGroup.Params.potencia_max / Conex;
          Cargador->Delta = desired_current - Cargador->Current;
        }
        break;
    }
  }

}

// Model step function
void Calculo_Consigna(carac_charger* Cargador)
{
  switch (is_c3_charger) {
     case IN_CochesConectados:
      if (!memcmp(Cargador->HPT,"C2",2) && Cargador->Current> 500) {
        is_c3_charger = IN_LimiteConsumo;
        
        Comand_no_lim = desired_current;

        Cargador->limite_fase = 0;
        if (Delta_total == 0) {
          is_LimiteConsumo = IN_Cargando1;

          desired_current = ChargingGroup.Params.potencia_max ;
        } else {
          if (Delta_total > 0) {
            is_LimiteConsumo = IN_Cargando3;

            desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

            
          }
        }
      } 
      else {
   
        desired_current = 0;
      }
      break;

     case IN_LimiteConsumo:
      LimiteConsumo(Cargador);
      break;

     default:
  
      if (ChargingGroup.Params.inst_max >= Conex_Fase * Comand_no_lim) {
        is_LimiteInstalacion = IN_NO_ACTIVE_CHILD;
        is_c3_charger = IN_LimiteConsumo;
        
        Comand_no_lim = desired_current;

        Cargador->limite_fase = 0;
        if (Delta_total == 0) {
          is_LimiteConsumo = IN_Cargando1;

          desired_current = ChargingGroup.Params.potencia_max ;
        } 
        else if (Delta_total > 0) {

            is_LimiteConsumo = IN_Cargando3;
            desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

        }
      } 
      else if (memcmp(Cargador->HPT,"C2",2) || Cargador->Current < 500) {
        is_LimiteInstalacion = IN_NO_ACTIVE_CHILD;
        is_c3_charger = IN_CochesConectados;

        desired_current = 0;
        
      } 
      else {
        Cargador->limite_fase = 1;
        switch (is_LimiteInstalacion) {
         case IN_ContadorFase:
          if (desired_current - Cargador-> Current <= 6) {
            is_LimiteInstalacion = IN_Limitacion1;
            desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) + ChargingGroup.Params.inst_max / Conex_Fase;

            
          } else if (pdTICKS_TO_MS(xTaskGetTickCount()-xStart1) >= 20000) {
            is_LimiteInstalacion = IN_RepartirFase;
            desired_current = ChargingGroup.Params.inst_max / Conex_Fase;

            Cargador->Delta_fase = desired_current - Cargador->Current;
          }
          break;

         case IN_Limitacion:
          if ((ChargingGroup.Params.inst_max > Pc_Fase) && (desired_current * Conex_Fase <= ChargingGroup.Params.inst_max)) {
            is_LimiteInstalacion = IN_Limitacion1;
            desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) + ChargingGroup.Params.inst_max / Conex_Fase;
            
          } 
          else {
            desired_current = ChargingGroup.Params.inst_max / Conex_Fase;
          }
          break;

         case IN_Limitacion1:
          if (desired_current - Cargador->Current > 6) {
            is_LimiteInstalacion = IN_ContadorFase;

            xStart1 = xTaskGetTickCount();
          } else {
            desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) + ChargingGroup.Params.inst_max / Conex_Fase;
          }
          break;

         default:
          // case IN_RepartirFase:
          if (desired_current - Cargador->Current <= 6) {
      
            Cargador->Delta_fase = 0;
            is_LimiteInstalacion = IN_Limitacion1;
            desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) + ChargingGroup.Params.inst_max / Conex_Fase;
          } 
          else {
            desired_current = ChargingGroup.Params.inst_max / Conex_Fase;
            Cargador->Delta_fase = desired_current - Cargador->Current;
          }
          break;
        }
      }
      break;
    }

#ifdef DEBUG_GROUPS
  cls();
  printf("Cargador %s\n", Cargador->name);
  printf("is_c3_charger= %i\n",is_c3_charger);
  printf("Conex = %i\n", Conex);
  printf("Comand desired current %i \n", desired_current);
  printf("Max p = %i \n", ChargingGroup.Params.potencia_max);
  printf("Inst max = %i \n", ChargingGroup.Params.inst_max);
#endif
  Cargador->DesiredCurrent = desired_current;
  printf("Comand desired current2 %i \n", Cargador->DesiredCurrent);
}

//Comprobar si permitimos que un cargador empieze a cargar
bool check_perm(carac_charger Cargador){
  Conex=0;
  Conex_Delta=0;
  Conex_Delta_Fase=0;
  Delta_total=0;
  Delta_total_fase=0;
  Fases_limitadas=0;
  Pc=0;
  Pc_Fase=0;
  uint16_t total_pc =0;
  uint16_t total_pc_fase =0;

  for(int i=0; i< ChargingGroup.group_chargers.size;i++){
      //Datos por Grupo
      if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2) && ChargingGroup.group_chargers.charger_table[i].Current >500){
          Conex++;
          total_pc += ChargingGroup.group_chargers.charger_table[i].Current;
          if(ChargingGroup.group_chargers.charger_table[i].Delta > 0){
              Conex_Delta++;
              Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;
          }
      }

      //Datos por fase
      if(Cargador.Fase == ChargingGroup.group_chargers.charger_table[i].Fase){
        if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2) && ChargingGroup.group_chargers.charger_table[i].Current >500){
            Conex_Fase++;
            if(ChargingGroup.group_chargers.charger_table[i].Delta > 0){
                Conex_Delta_Fase++;
            }
        }
        total_pc_fase += ChargingGroup.group_chargers.charger_table[i].Current;
        Delta_total_fase += ChargingGroup.group_chargers.charger_table[i].Delta_fase;
      }

      Fases_limitadas+= ChargingGroup.group_chargers.charger_table[i].limite_fase;      
  }

  Pc=total_pc/100;
  Pc_Fase=total_pc_fase/100;  

  //Si estamos por debajo de la potenica límite
  if(Pc < ChargingGroup.Params.potencia_max && Pc_Fase < ChargingGroup.Params.inst_max){
    Cargador.Baimena = true;
    memcpy(Cargador.HPT, "C2",2);
    return true;
  }

  //si no, le intentamos buscar un hueco
  else{
    Cargador.Baimena = true;
    memcpy(Cargador.HPT, "C2",2);
    Cargador.Current = ChargingGroup.Params.potencia_max * 100;
    input_values(Cargador);
    Calculo_Consigna(&Cargador);
    if(Cargador.DesiredCurrent > 6){
      return true;
    }
            
  }

  return false;
}

void input_values(carac_charger Cargador){
    Conex=0;
    Conex_Delta=0;
    Conex_Delta_Fase=0;
    Delta_total=0;
    Delta_total_fase=0;
    Fases_limitadas=0;
    Pc=0;
    Pc_Fase=0;
    uint16_t total_pc =0;
    uint16_t total_pc_fase =0;

    for(int i=0; i< ChargingGroup.group_chargers.size;i++){
        //Datos por Grupo
        if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2) && ChargingGroup.group_chargers.charger_table[i].Current >500){
            Conex++;
            if(ChargingGroup.group_chargers.charger_table[i].Delta > 0){
                Conex_Delta++;
            }
        }

        //Datos por fase
        if(Cargador.Fase == ChargingGroup.group_chargers.charger_table[i].Fase){
          if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2) && ChargingGroup.group_chargers.charger_table[i].Current >500){
              Conex_Fase++;
              if(ChargingGroup.group_chargers.charger_table[i].Delta > 0){
                  Conex_Delta_Fase++;
              }
          }
          total_pc_fase += ChargingGroup.group_chargers.charger_table[i].Current;
          Delta_total_fase += ChargingGroup.group_chargers.charger_table[i].Delta_fase;
        }


        Fases_limitadas+= ChargingGroup.group_chargers.charger_table[i].limite_fase;
        Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;
        total_pc += ChargingGroup.group_chargers.charger_table[i].Current;
    }   
    Pc=total_pc/100;
    Pc_Fase=total_pc_fase/100;
#ifdef DEBUG_GROUPS
    //f("Total PC of phase %i\n",Pc); 
    //printf("Total PC and Delta %i %i \n",Pc,Delta_total);    
#endif
}

