#include "group_control.h"
#include "control.h"
#include <string.h>
#include "helpers.h"
#include "cJSON.h"

extern carac_Params Params;
extern carac_Status Status;
extern carac_Comands Comands;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_chargers FaseChargers;
extern carac_group    ChargingGroup;

bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);
void remove_group(carac_chargers* group);
uint8_t check_in_group(const char* ID, carac_chargers* group);
void store_group_in_mem(carac_chargers* group);
IPAddress get_IP(const char* ID);
void stop_MQTT();

TickType_t xStart;
TickType_t xStart1;

uint8_t  is_active_c3_Charger;
uint8_t  is_c3_charger;         
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

void Calculo_Consigna();
void input_values();
void Ping_Req(const char* Data){
  int n=check_in_group(Data,&ChargingGroup.group_chargers);
  if(n!=255){
    ChargingGroup.group_chargers.charger_table[n].Period =0;
  }
}

//Funcion para procesar los nuevos datos recibidos
void New_Data(const char* Data, int Data_size){

    cJSON *mensaje_Json = cJSON_Parse(Data);
    carac_charger Cargador;
    //Cargador.name[8]='\0';
    //Cargador.HPT[2]='\0';
    //Extraer los datos
    memcpy(Cargador.HPT,cJSON_GetObjectItem(mensaje_Json,"HPT")->valuestring,3);
    memcpy(Cargador.name,cJSON_GetObjectItem(mensaje_Json,"device_id")->valuestring,9);
    Cargador.limite_fase = cJSON_GetObjectItem(mensaje_Json,"limite_fase")->valueint;
    Cargador.Fase = cJSON_GetObjectItem(mensaje_Json,"fase")->valueint;
    Cargador.Current = cJSON_GetObjectItem(mensaje_Json,"current")->valueint;
    Cargador.Delta = cJSON_GetObjectItem(mensaje_Json,"Delta")->valueint;

    cJSON_Delete(mensaje_Json);

    //si no somos un equipo trifásico:
    if(!Status.Trifasico){
      //Comprobar que está en nuestra misma fase
      if(Params.Fase == Cargador.Fase){             
        //comprobar si el cargador ya está almacenado en nuestro grupo de fase                       
        uint8_t index = check_in_group(Cargador.name,&FaseChargers);               
        if(index < 255){                         
            FaseChargers.charger_table[index] =Cargador;
            //cls();
            //print_table(FaseChargers);
        }
        else{
            //Si el cargador no está en la tabla, añadirlo y actualizar los datos
            add_to_group(Cargador.name, get_IP(Cargador.name),&FaseChargers);
            FaseChargers.charger_table[FaseChargers.size-1] = Cargador;
        }
      }
    }

    //Actualizacion del grupo total
    //Buscar el equipo en el grupo total
    uint8_t index = check_in_group(Cargador.name,&ChargingGroup.group_chargers);                  
    if(index < 255){
        ChargingGroup.group_chargers.charger_table[index]=Cargador;
    }
    //print_table(ChargingGroup.group_chargers);
    input_values();
    Calculo_Consigna();
}

//Funcion para recibir nuevos parametros de carga para el grupo
void New_Params(const char* Data, int Data_size){
  uint8_t buffer[7];
  
  buffer[0] = ChargingGroup.Params.GroupMaster;
  buffer[1] = ChargingGroup.Params.GroupActive;
  memcpy(&buffer[2],&Data[2],5);
  SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 
  delay(50);
}

//Funcion para recibir ordenes de grupo que s ehayan enviado a otro cargador
void New_Control(const char* Data, int Data_size){

  if(!memcmp(Data,"Pause",5)){
    printf("Tengo que pausar el grupo\n");
    ChargingGroup.Params.GroupActive = 0;
    stop_MQTT();

    char buffer[7];
    memcpy(&buffer,&ChargingGroup.Params,7);
    SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 

        
  }
  else if(!memcmp(Data,"Delete",6)){
    printf("Tengo que borrar el grupo\n");
    ChargingGroup.Params.GroupActive = 0;
    ChargingGroup.Params.GroupMaster = 0;
    ChargingGroup.DeleteOrder = false;
    stop_MQTT();
    remove_group(&ChargingGroup.group_chargers);
    store_group_in_mem(&ChargingGroup.group_chargers);

    char buffer[7];
    memcpy(&buffer,&ChargingGroup.Params,7);
    SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 
  }
}

//Funcion para recibir un nuevo grupo de cargadores
void New_Group(const char* Data, int Data_size){
    char n[2];
    memcpy(n,Data,2);
    uint8_t numero_de_cargadores = atoi(n);
   
    //comprobar si el grupo es distinto
    //Si es distinto del que ya tenemos almacenado, lo guardamos en la flash
    if(numero_de_cargadores == ChargingGroup.group_chargers.size){
      for(uint8_t i=0;i<numero_de_cargadores;i++){
        if(memcmp(ChargingGroup.group_chargers.charger_table[i].name, &Data[i*9+2],8) || ChargingGroup.group_chargers.charger_table[i].Fase != Data[i*9+10]-'0'){
          SendToPSOC5((char*)Data,Data_size,GROUPS_DEVICES); 
          delay(50);
          return;
        }
      }
    }
    else{
      SendToPSOC5((char*)Data,Data_size,GROUPS_DEVICES); 
      delay(50);
    }
}

// Function for Chart: '<Root>/Charger 1'
void LimiteConsumo()
{

  if (ChargingGroup.Params.inst_max < Pc_Fase) {
    Status.Delta = 0;
    is_LimiteConsumo = IN_NO_ACTIVE_CHILD;
    is_c3_charger = IN_LimiteInstalacion;

    Status.limite_Fase = 1;

    if (Delta_total_fase == 0) {
      is_LimiteInstalacion = IN_Limitacion;

      desired_current = ChargingGroup.Params.inst_max / Conex_Fase;
    } else {
      if (Delta_total_fase > 0) {
        is_LimiteInstalacion = IN_Limitacion1;

        desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) +
          ChargingGroup.Params.inst_max / Conex_Fase;

        
      }
    }


  } else if ((Pc == 0) || (Conex < 1)) {
    is_LimiteConsumo = IN_NO_ACTIVE_CHILD;
    is_c3_charger = IN_CochesConectados;

    desired_current = 0;

    

    
  } else {
 
    Comand_no_lim = desired_current;

    Status.limite_Fase = 0;
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
      } else if (desired_current * Conex < ChargingGroup.Params.potencia_max) {
        is_LimiteConsumo = IN_Cargando1;

        desired_current = ChargingGroup.Params.potencia_max ;
      } else {

        desired_current = ChargingGroup.Params.potencia_max ;
      }
      break;

     case IN_Cargando3:

      if (ChargingGroup.Params.potencia_max > Conex * ChargingGroup.Params.potencia_max ) {
        is_LimiteConsumo = IN_Cargando1;

  
        desired_current = ChargingGroup.Params.potencia_max ;
      } else if (desired_current - Status.Measures.instant_current > 6) {
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

      if (desired_current - Status.Measures.instant_current <= 6) {
        is_LimiteConsumo = IN_Cargando3;
        desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;
        
      } else if (pdTICKS_TO_MS(xTaskGetTickCount()-xStart) >= 20000) {
        is_LimiteConsumo = IN_Repartir;
        desired_current = ChargingGroup.Params.potencia_max / Conex;
        Status.Delta = desired_current - Status.Measures.instant_current;
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
      if (desired_current - Status.Measures.instant_current <= 6) {
        Status.Delta = 0;
        is_LimiteConsumo = IN_Cargando3;
        desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;
        
      } else {
        desired_current = ChargingGroup.Params.potencia_max / Conex;
        Status.Delta = desired_current - Status.Measures.instant_current;
      }
      break;
    }
  }

}

// Model step function
void Calculo_Consigna()
{

if (is_active_c3_Charger == 0) {
    is_active_c3_Charger = 1;
    is_c3_charger = IN_CochesConectados;

    desired_current = 0;

    

    
  } else {
    switch (is_c3_charger) {
     case IN_CochesConectados:
      if (Conex > 0 && !memcmp(Status.HPT_status,"C2",2)) {
        is_c3_charger = IN_LimiteConsumo;
        
        Comand_no_lim = desired_current;

        Status.limite_Fase = 0;
        if (Delta_total == 0) {
          is_LimiteConsumo = IN_Cargando1;

          desired_current = ChargingGroup.Params.potencia_max ;
        } else {
          if (Delta_total > 0) {
            is_LimiteConsumo = IN_Cargando3;

            desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

            
          }
        }
      } else {
   
        desired_current = 0;

        

        
      }
      break;

     case IN_LimiteConsumo:
      LimiteConsumo();
      break;

     default:
  
      if (ChargingGroup.Params.inst_max >= Conex_Fase * Comand_no_lim) {
        is_LimiteInstalacion = IN_NO_ACTIVE_CHILD;
        is_c3_charger = IN_LimiteConsumo;
        
        Comand_no_lim = desired_current;

        Status.limite_Fase = 0;
        if (Delta_total == 0) {
          is_LimiteConsumo = IN_Cargando1;

          desired_current = ChargingGroup.Params.potencia_max ;
        } else {
          if (Delta_total > 0) {
            is_LimiteConsumo = IN_Cargando3;

            desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

            
          }
        }
      } else if ((Conex == 0) || memcmp(Status.HPT_status,"C2",2)) {
        is_LimiteInstalacion = IN_NO_ACTIVE_CHILD;
        is_c3_charger = IN_CochesConectados;

        desired_current = 0;

        

        
      } else {

        Status.limite_Fase = 1;
        switch (is_LimiteInstalacion) {
         case IN_ContadorFase:
          if (desired_current - Status.Measures.instant_current <= 6) {
            is_LimiteInstalacion = IN_Limitacion1;
            desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) + ChargingGroup.Params.inst_max / Conex_Fase;

            
          } else if (pdTICKS_TO_MS(xTaskGetTickCount()-xStart1) >= 20000) {
            is_LimiteInstalacion = IN_RepartirFase;
            desired_current = ChargingGroup.Params.inst_max / Conex_Fase;

            Status.Delta_Fase = desired_current - Status.Measures.instant_current;
          }
          break;

         case IN_Limitacion:
          if ((ChargingGroup.Params.inst_max > Pc_Fase) && (desired_current * Conex_Fase <=
               ChargingGroup.Params.inst_max)) {
            is_LimiteInstalacion = IN_Limitacion1;
            desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) + ChargingGroup.Params.inst_max / Conex_Fase;

            
          } else {
            desired_current = ChargingGroup.Params.inst_max / Conex_Fase;
          }
          break;

         case IN_Limitacion1:
          if (desired_current - Status.Measures.instant_current > 6) {
            is_LimiteInstalacion = IN_ContadorFase;

            xStart1 = xTaskGetTickCount();
          } else {
            desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) + ChargingGroup.Params.inst_max / Conex_Fase;

            
          }
          break;

         default:
          // case IN_RepartirFase:
          if (desired_current - Status.Measures.instant_current <= 6) {
      
            Status.Delta_Fase = 0;
            is_LimiteInstalacion = IN_Limitacion1;
            desired_current = Delta_total_fase / (Conex_Fase - Conex_Delta_Fase) + ChargingGroup.Params.inst_max / Conex_Fase;

            
          } else {
            desired_current = ChargingGroup.Params.inst_max / Conex_Fase;

            Status.Delta_Fase = desired_current - Status.Measures.instant_current;
          }
          break;
        }
      }
      break;
    }
  }

#ifdef DEBUG_GROUPS
  //printf("is_c3_charger= %i\n",is_c3_charger);
  //printf("Conex = %i\n", Conex);
  //printf("Comand desired current %i \n", desired_current);
  //printf("Max p = %i \n", ChargingGroup.Params.potencia_max);
  //printf("Inst max = %i \n", ChargingGroup.Params.inst_max);
#endif
  if(desired_current!=Comands.desired_current &&  !memcmp(Status.HPT_status,"C2",2)){
      //SendToPSOC5(desired_current,MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
  }
}

void input_values(){
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
        if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2)){
            Conex++;
        }
        if(ChargingGroup.group_chargers.charger_table[i].Delta > 0){
            Conex_Delta++;
        }
        Fases_limitadas+= ChargingGroup.group_chargers.charger_table[i].limite_fase;
        Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;
        total_pc += ChargingGroup.group_chargers.charger_table[i].Current;
    }   
    Pc=total_pc/1000;

    for(int i=0; i< FaseChargers.size;i++){
        if(!memcmp(FaseChargers.charger_table[i].HPT,"C2",2)){
            Conex_Fase++;
        }
        if(FaseChargers.charger_table[i].Delta > 0){
            Conex_Delta_Fase++;
        }
        //Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;
        total_pc_fase += FaseChargers.charger_table[i].Current;
        Delta_total_fase += FaseChargers.charger_table[i].Delta_fase;

    }
    Pc_Fase=total_pc_fase/1000;
#ifdef DEBUG_GROUPS
    //printf("Total PC of phase %i\n",Pc); 
    //printf("Total PC and Delta %i %i \n",Pc,Delta_total);    
#endif
}

