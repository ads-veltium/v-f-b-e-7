#include "group_control.h"
#include "control.h"
#include <string.h>
#include "helpers.h"

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
TickType_t xStart;

uint8_t  is_active_c3_Charger;
uint8_t  is_c3_charger;         
uint8_t  is_LimiteConsumo;
uint8_t  is_LimiteInstalacion;
uint8_t  Delta_total;

int Conex;
int Conex_Delta;
int Conex_Fase;
int Fases_limitadas;
int Comand_no_lim;

uint8_t Pc;
uint8_t Pc_Fase;

void Calculo_Consigna();
void input_values();

void Ping_Req(char* Data){
  int n=check_in_group(Data,&ChargingGroup.group_chargers);
  if(n!=255){
    ChargingGroup.group_chargers.charger_table[n].Period =0;
  }
}

//Funcion para procesar los nuevos datos recibidos
void New_Data(char* Data, int Data_size){
    if(memcmp(Data, ConfigFirebase.Device_Id, 8)){                         //comprobar que no son nuestros propios datos
        char fase = Data[8];
        char ID[9];
        ID[8]= '\0';
        memcpy(ID,Data,8); 

        //si no es un equipo trifásico:
        if(!Status.Trifasico){
            if(Params.Fase == atoi(&fase)){                                    //Comprobar que está en nuestra misma fase
                uint8_t index = check_in_group(ID,&FaseChargers);               //comprobar si el cargador ya está almacenado en nuestro grupo de fase
                if(index < 255){                         
                    memcpy(FaseChargers.charger_table[index].HPT,&Data[9],2);   //Si el cargador ya existe, actualizar sus datos
                    FaseChargers.charger_table[index].Fase = atoi(&fase);
                    char Delta[3];
                    memcpy(Delta, &Data[11], 2);
                    FaseChargers.charger_table[index].Delta = atoi(Delta);

                    char current[Data_size-13+1];
                    memcpy( current, &Data[13], Data_size-13 );         
                    FaseChargers.charger_table[index].Current = atoi(current);
                    //cls();
                    //print_table(FaseChargers);
                }
                else{
                    //Si el cargador no está en la tabla, añadirlo y actualizar los datos
                    add_to_group(ID, get_IP(ID),&FaseChargers);

                    FaseChargers.charger_table[FaseChargers.size-1].Fase = atoi(&fase);
                    memcpy(FaseChargers.charger_table[FaseChargers.size-1].HPT,&Data[9],2);

                    char Delta[3];
                    memcpy(Delta, &Data[11], 2);
                    FaseChargers.charger_table[FaseChargers.size-1].Delta = atoi(Delta);
                    
                    char current[Data_size-13+1];
                    memcpy( current, &Data[13], Data_size-13 );  
                    FaseChargers.charger_table[FaseChargers.size-1].Current = atoi(current);
                }


            }
        }

        //Actualizacion del grupo total
        uint8_t index = check_in_group(ID,&ChargingGroup.group_chargers);                  //Buscar el equipo en el grupo total
        if(index < 255){
            ChargingGroup.group_chargers.charger_table[index].Fase = atoi(&fase);
            memcpy(ChargingGroup.group_chargers.charger_table[index].HPT,&Data[9],2);      //Si el cargador ya existe, actualizar sus datos
                        
            char Delta[3];
            memcpy(Delta, &Data[11], 2);
            ChargingGroup.group_chargers.charger_table[index].Delta = atoi(Delta);

            char current[Data_size-13+1];
            memcpy( current, &Data[13], Data_size-13 );         
            ChargingGroup.group_chargers.charger_table[index].Current = atoi(current);

            //print_table(ChargingGroup.group_chargers);
        }
    }

 
    //Actualizar nuestros propios valores
    else{
        uint8_t index = check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers);     //Buscar el equipo en el grupo total
        if(index < 255){
            ChargingGroup.group_chargers.charger_table[index].Fase = Params.Fase;
            memcpy(ChargingGroup.group_chargers.charger_table[index].HPT,Status.HPT_status,2);      //Si el cargador ya existe, actualizar sus datos
                    
            ChargingGroup.group_chargers.charger_table[index].Current = Status.Measures.instant_voltage;
        }
    }
    input_values();
    Calculo_Consigna();
}

//Funcion para recibir nuevos parametros de carga para el grupo
void New_Params(char* Data, int Data_size){
  printf("New params received %s\n", Data);
  char buffer[7];
  
  buffer[0] = ChargingGroup.Params.GroupMaster;
  memcpy(&buffer[1],&Data[1],6);
  SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 
  delay(50);
}

void New_Control(char* Data, int Data_size){
  printf("New control received %s\n", Data);

  if(!memcmp(Data,"Pause",6)){
    printf("Tengo que pausar el grupo\n");
  }
  else if(!memcmp(Data,"Delete",6)){
    printf("Tengo que borrar el grupo\n");
  }
}

//Funcion para recibir un nuevo grupo de cargadores
void New_Group(char* Data, int Data_size){
    char n[2];
    memcpy(n,Data,2);
    uint8_t numero_de_cargadores = atoi(n);
   
    //comprobar si el grupo es distinto
    //Si es distinto del que ya tenemos almacenado, lo guardamos en la flash
    if(numero_de_cargadores == ChargingGroup.group_chargers.size){
      for(uint8_t i=0;i<numero_de_cargadores;i++){
        if(memcmp(ChargingGroup.group_chargers.charger_table[i].name, &Data[i*9+2],8) || ChargingGroup.group_chargers.charger_table[i].Fase != Data[i*9+10]){
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

void Calculo_Consigna(){

  if (is_active_c3_Charger == 0) {
    is_active_c3_Charger = 1;
    is_c3_charger = IN_CochesConectados;

    Comands.desired_current = 0;

  } else {
    switch (is_c3_charger) {
     case IN_CochesConectados:
      if ((Pc > 0) && !memcmp(Status.HPT_status,"C2",2)) {
        is_c3_charger = IN_LimiteConsumo;
        Comand_no_lim = Comands.desired_current;

        Status.limite_Fase = 0;
        if (Delta_total == 0) {
          is_LimiteConsumo = IN_Cargando1;
          Comands.desired_current = MAX_CURRENT;
        } else {
          if (Delta_total > 0) {
            is_LimiteConsumo = IN_Cargando3;

            Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max /Conex;
          }
        }
      } else {
        Comands.desired_current = 0;
      }
      break;

     case IN_LimiteConsumo:
      if (ChargingGroup.Params.inst_max < Pc_Fase) {
        Status.Delta = 0;
        is_LimiteConsumo = IN_NO_ACTIVE_CHILD;
        is_c3_charger = IN_LimiteInstalacion;
       

        Status.limite_Fase = 1;
        is_LimiteInstalacion = IN_Limitacion;

        Comands.desired_current = ChargingGroup.Params.inst_max / Conex_Fase;
      } else if ((Pc == 0) || memcmp(Status.HPT_status,"C2",2)) {
        is_LimiteConsumo = IN_NO_ACTIVE_CHILD;
        is_c3_charger = IN_CochesConectados;

        Comands.desired_current = 0;

      } else {
        Comand_no_lim = Comands.desired_current;

        Status.limite_Fase = 0;
        switch (is_LimiteConsumo) {
         case IN_Cargando1:
          if (Comands.desired_current * Conex > ChargingGroup.Params.potencia_max) {
            is_LimiteConsumo = IN_Cargando2;
            Comands.desired_current = MAX_CURRENT;
          } else if ((Pc >= ChargingGroup.Params.potencia_max) && (Comands.desired_current * Conex < ChargingGroup.Params.potencia_max)) {
            is_LimiteConsumo = IN_ReduccionPc;
            Comands.desired_current = ChargingGroup.Params.potencia_max / Conex;
          } else {
            Comands.desired_current = MAX_CURRENT;
          }
          break;

         case IN_Cargando2:
          if (Pc >= ChargingGroup.Params.potencia_max) {
            is_LimiteConsumo = IN_ReduccionPc;

            Comands.desired_current = ChargingGroup.Params.potencia_max / Conex;
          } else if (Comands.desired_current * Conex < ChargingGroup.Params.potencia_max) {
            is_LimiteConsumo = IN_Cargando1;

            Comands.desired_current = MAX_CURRENT;
          } else {

            Comands.desired_current = MAX_CURRENT;
          }
          break;

         case IN_Cargando3:
          if (ChargingGroup.Params.potencia_max > Conex * MAX_CURRENT) {
            is_LimiteConsumo = IN_Cargando1;

            Comands.desired_current = MAX_CURRENT;
          } else if (Comands.desired_current - Status.Measures.instant_current > 6.0) {
            is_LimiteConsumo = IN_Contador;

            xStart = xTaskGetTickCount();
          } else if ((Fases_limitadas > 0) && (ChargingGroup.Params.potencia_max > (MAX_CURRENT - Comands.desired_current) + Pc)) {
            is_LimiteConsumo = IN_Cargando4;

            Comands.desired_current = MAX_CURRENT;
          } else {

            Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

            if (Comands.desired_current >= MAX_CURRENT){
              Comands.desired_current = MAX_CURRENT;
            }

          }
          break;

         case IN_Cargando4:
          if (ChargingGroup.Params.potencia_max <= (MAX_CURRENT - Comands.desired_current) + Pc) {
            is_LimiteConsumo = IN_Cargando3;
            Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

          } else if (Fases_limitadas == 0) {
            is_LimiteConsumo = IN_Cargando1;
            Comands.desired_current = MAX_CURRENT;
          } else {
            Comands.desired_current = MAX_CURRENT;
          }
          break;

         case IN_Contador:
          if (Comands.desired_current - Status.Measures.instant_current <= 6.0) {
            is_LimiteConsumo = IN_Cargando3;
            Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

          } else if (pdTICKS_TO_MS(xTaskGetTickCount()-xStart) >= 20000) {
            is_LimiteConsumo = IN_Repartir;
            Comands.desired_current = ChargingGroup.Params.potencia_max / Conex;

            Status.Delta = Comands.desired_current - Status.Measures.instant_current;
          }
          break;

         case IN_ReduccionPc:
          if ((ChargingGroup.Params.potencia_max > Pc) && (Comands.desired_current * Conex <= ChargingGroup.Params.potencia_max)) {
            is_LimiteConsumo = IN_Cargando3;
            Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

          } else {
            Comands.desired_current = ChargingGroup.Params.potencia_max / Conex;
          }
          break;

         default:
          // case IN_Repartir:
          if (Comands.desired_current - Status.Measures.instant_current <= 6.0) {
 
            Status.Delta = 0;
            is_LimiteConsumo = IN_Cargando3;
            Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;

          } else {
            Comands.desired_current = ChargingGroup.Params.potencia_max / Conex;

            Status.Delta = Comands.desired_current - Status.Measures.instant_current;
          }
          break;
        }
      }
      break;

     default:
      // case IN_LimiteInstalacion:
      if (ChargingGroup.Params.inst_max >= Conex_Fase * Comand_no_lim) {
        is_LimiteInstalacion = IN_NO_ACTIVE_CHILD;
        is_c3_charger = IN_LimiteConsumo;
        Comand_no_lim = Comands.desired_current;

        Status.limite_Fase = 0;
        if (Delta_total == 0) {
          is_LimiteConsumo = IN_Cargando1;

          Comands.desired_current = MAX_CURRENT;
        } else {
          if (Delta_total > 0) {
            is_LimiteConsumo = IN_Cargando3;

            Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;
          }
        }
      } else if ((Pc == 0) || memcmp(Status.HPT_status,"C2",2)) {
        is_LimiteInstalacion = IN_NO_ACTIVE_CHILD;
        is_c3_charger = IN_CochesConectados;

        Comands.desired_current = 0;

      } else {

        Status.limite_Fase = 1;
        if (is_LimiteInstalacion == IN_Limitacion) {
          if ((ChargingGroup.Params.inst_max > Pc_Fase) && (Comands.desired_current * Conex_Fase <= ChargingGroup.Params.inst_max)) {
            is_LimiteInstalacion = IN_Limitacion1;
            Comands.desired_current = ChargingGroup.Params.inst_max / Conex_Fase;
          } else {
            Comands.desired_current = ChargingGroup.Params.inst_max / Conex_Fase;
          }
        } else {
          Comands.desired_current = ChargingGroup.Params.inst_max / Conex_Fase;
        }
      }
      break;
    }
  }
  //printf("Comand desired current %i \n", Comands.desired_current);
  //SendToPSOC5(Comands.desired_current,MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
}

void input_values(){
    Conex=0;
    Conex_Delta=0;
    Delta_total=0;
    Fases_limitadas=0;
    Pc=0;
    Pc_Fase=0;
    uint16_t total_pc =0;
    uint16_t total_pc_fase =0;
    for(int i=0; i< ChargingGroup.group_chargers.size-1;i++){
        if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2)){
            Conex++;
        }
        if(ChargingGroup.group_chargers.charger_table[i].Delta > 0){
            Conex_Delta++;
        }
        Fases_limitadas+= ChargingGroup.group_chargers.charger_table[i].Num_limitado;
        Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;
        total_pc += ChargingGroup.group_chargers.charger_table[i].Current;
    }   
    Pc=total_pc/100;
    //printf("Total PC and Delta %i %i \n",Pc,Delta_total); 

    for(int i=0; i< FaseChargers.size-1;i++){
        if(!memcmp(FaseChargers.charger_table[i].HPT,"C2",2)){
            Conex_Fase++;
        }
        //Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;
        total_pc_fase += ChargingGroup.group_chargers.charger_table[i].Current;
    }
    Pc_Fase=total_pc_fase/100;
    //printf("Total PC of phase %i\n",Pc);    
}

