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
uint8_t check_in_group(const char* ID, carac_chargers* group);
IPAddress get_IP(const char* ID);


uint8_t  is_c3_Charger;         
uint8_t  is_LimiteConsumo; 
uint8_t  Delta_total;

int Conex;
int Conex_Delta;
uint8_t Pc;

TickType_t xStart;

void Calculo_Consigna();
void input_values();


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

void New_Params(char* Data, int Data_size){
    char n[2];
    memcpy(n,Data,2);
    uint8_t numero_de_cargadores = atoi(n);
    bool newchargers=false;
    
    //comprobar si el grupo es distinto
    if(numero_de_cargadores == ChargingGroup.group_chargers.size){
      for(uint8_t i=0;i<numero_de_cargadores;i++){
        if(memcmp(ChargingGroup.group_chargers.charger_table[i].name, &Data[i*9+2],8)){
          newchargers=true;
          break;
        }
      }
    }
    else{
      newchargers=true;
    }
    
    if(newchargers){
      printf("Nuevos cargadores recibidos!\n");
      //Copiar el nuevo grupo a nuestra tabla
      ChargingGroup.group_chargers.size = 0;
      for(uint8_t i=0;i<numero_de_cargadores;i++){
          char ID[8];
          memcpy(ID, &Data[i*9+2],8);
          add_to_group(ID,get_IP(ID),&ChargingGroup.group_chargers);
      }   

      //Almacenarlo en el PSOC5
      uint8_t sendBuffer[252];
      sendBuffer[0]=numero_de_cargadores;
      if(numero_de_cargadores<25){
        memcpy(&sendBuffer[1],&Data[2],numero_de_cargadores*9+1);
        SendToPSOC5((char*)sendBuffer,numero_de_cargadores*9+1,GROUPS_DEVICES); 
      }
    }

    uint8 buffer[7];
    ChargingGroup.Params.potencia_max = 16;
    ChargingGroup.Params.inst_max = 13;
    buffer[0] = ChargingGroup.Params.GroupMaster;
    buffer[1] = ChargingGroup.Params.GroupActive;
    buffer[2] = ChargingGroup.Params.inst_max;
    buffer[3] = ChargingGroup.Params.CDP;
    buffer[4] = ChargingGroup.Params.ContractPower;
    buffer[5] = ChargingGroup.Params.UserID;
    buffer[6] = ChargingGroup.Params.potencia_max;
    SendToPSOC5((char*)buffer,7,GROUPS_PARAMS); 
}

void Calculo_Consigna(){

  if (is_c3_Charger == 0) {
    is_c3_Charger = IN_CochesConectados;                   
    Comands.desired_current = 0;
    
  } 
  else if (is_c3_Charger == IN_CochesConectados) {
    if ((Pc > 0) && !memcmp(Status.HPT_status,"C2",2)) {
      is_c3_Charger = IN_LimiteConsumo;
      if (Delta_total == 0) {
        is_LimiteConsumo = IN_Cargando1;

        Comands.desired_current = MAX_CURRENT;
      } 
      else {
        if (Delta_total > 0) {
          is_LimiteConsumo = IN_Cargando3;

          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max/Conex;
        }
      }
    } 
    else {Comands.desired_current = 0;}
  } 
  else{

    if ( Pc == 0|| memcmp(Status.HPT_status,"C2",2)) {
      is_LimiteConsumo = IN_NO_ACTIVE_CHILD;
      is_c3_Charger = IN_CochesConectados;

      Comands.desired_current = 0;

    } else {
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
        } else if (Comands.desired_current - Status.Measures.instant_current > 6) {
          is_LimiteConsumo = IN_Contador;
          xStart = xTaskGetTickCount();
        } else {

          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max / Conex;
          
          if (Comands.desired_current >= MAX_CURRENT){
              Comands.desired_current = MAX_CURRENT;
          }

        }
        break;

       case IN_Contador:
        if (Comands.desired_current - Status.Measures.instant_current <= 6) {
          is_LimiteConsumo = IN_Cargando3;
          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max /Conex;
          
        } else if (pdTICKS_TO_MS(xTaskGetTickCount()-xStart) >= 20000) {
          is_LimiteConsumo = IN_Repartir;
          Comands.desired_current = ChargingGroup.Params.potencia_max / Conex;

          Status.Delta = Comands.desired_current - Status.Measures.instant_current;
        } 
        break;

       case IN_ReduccionPc:
        if ((ChargingGroup.Params.potencia_max > Pc) && (Comands.desired_current * Conex <= ChargingGroup.Params.potencia_max)) {
          is_LimiteConsumo = IN_Cargando3;
          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max /
            Conex;
          
        } else {
          Comands.desired_current = ChargingGroup.Params.potencia_max / Conex;
        }
        break;

       default:

        if (Comands.desired_current - Status.Measures.instant_current <= 6) {

          Status.Delta = 0;
          is_LimiteConsumo = IN_Cargando3;
          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.Params.potencia_max /Conex;
        
        } else {
          Comands.desired_current = ChargingGroup.Params.potencia_max / Conex;
          Status.Delta = Comands.desired_current - Status.Measures.instant_current;
        }
        break;
      }
    }
  }
  //printf("Comand desired current %i \n", Comands.desired_current);
  //SendToPSOC5(Comands.desired_current,MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
}

void input_values(){
    Conex=0;
    Delta_total=0;
    Pc=0;
    uint16_t total_pc =0;
    for(int i=0; i< ChargingGroup.group_chargers.size-1;i++){
        if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2)){
            Conex++;
        }
        Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;
        total_pc += ChargingGroup.group_chargers.charger_table[i].Current;
    }   
    Pc=total_pc/100;
    //printf("Total PC and Delta %i %i \n",Pc,Delta_total); 
}

