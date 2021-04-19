#include "group_control.h"
#include "control.h"
#include <string.h>

extern carac_Params Params;
extern carac_Status Status;
extern carac_Comands Comands;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_chargers FaseChargers;
extern carac_group    ChargingGroup;

bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);
uint8_t check_in_group(const char* ID, carac_chargers* group);
IPAddress get_IP(const char* ID);


uint8_t  is_active_c3_Charger;
uint8_t  is_c3_Charger;         
uint8_t  is_LimiteConsumo; 
uint16_t Delta_total;

int Conex;
int Conex_Delta;
uint16_t Pc;

TickType_t xStart;

void Calculo_Consigna();
void input_values();

static void cls(){
    Serial.write(27);   
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H"); 
}

static void print_table(carac_chargers table){

    printf("=============== Grupo de cargadores ===================\n");
    printf("      ID     Fase   HPT   I          IP         D\n");
    for(int i=0; i< table.size;i++){     //comprobar si el cargador ya está almacenado
        printf("   %s    %i    %s  %i   %s   %i\n", table.charger_table[i].name,table.charger_table[i].Fase,table.charger_table[i].HPT,table.charger_table[i].Current, table.charger_table[i].IP.toString().c_str(), table.charger_table[i].Delta);
    }
    printf("Memoria interna disponible: %i\n", esp_get_free_internal_heap_size());
    printf("Memoria total disponible:   %i\n", esp_get_free_heap_size());
    printf("=======================================================\n");
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

void New_Params(char* Data, int Data_size){
    uint8_t numero_de_cargadores = (Data_size)/8;
    ChargingGroup.group_chargers.size = 0;

    //Copiar el nuevo grupo a nuestra tabla
    for(uint8_t i=0;i<numero_de_cargadores;i++){
        char ID[8];
        memcpy(ID, &Data[i*8],8);
        add_to_group(ID,get_IP(ID),&ChargingGroup.group_chargers);
    }    
}

void Calculo_Consigna(){

  if (is_active_c3_Charger == 0) {
      
    is_active_c3_Charger = 0;
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

          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max/Conex;
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
        if (Comands.desired_current * Conex > ChargingGroup.potencia_max) {
          is_LimiteConsumo = IN_Cargando2;
          Comands.desired_current = MAX_CURRENT;
        } else if ((Pc >= ChargingGroup.potencia_max) && (Comands.desired_current * Conex < ChargingGroup.potencia_max)) {
          is_LimiteConsumo = IN_ReduccionPc;
          Comands.desired_current = ChargingGroup.potencia_max / Conex;
        } else {
          Comands.desired_current = MAX_CURRENT;
        }
        break;

       case IN_Cargando2:
        if (Pc >= ChargingGroup.potencia_max) {
          is_LimiteConsumo = IN_ReduccionPc;

          Comands.desired_current = ChargingGroup.potencia_max / Conex;
        } else if (Comands.desired_current * Conex < ChargingGroup.potencia_max) {
          is_LimiteConsumo = IN_Cargando1;
          Comands.desired_current = MAX_CURRENT;
        } else {

          Comands.desired_current = MAX_CURRENT;
        }
        break;

       case IN_Cargando3:
        if (ChargingGroup.potencia_max > Conex * MAX_CURRENT) {
          is_LimiteConsumo = IN_Cargando1;

          Comands.desired_current = MAX_CURRENT;
        } else if (Comands.desired_current - Status.Measures.instant_current > 6) {
          is_LimiteConsumo = IN_Contador;

        } else {

          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max / Conex;
          
          if (Comands.desired_current >= MAX_CURRENT){
              Comands.desired_current = MAX_CURRENT;
          }

        }
        break;

       case IN_Contador:
        if (Comands.desired_current - Status.Measures.instant_current <= 6) {
          is_LimiteConsumo = IN_Cargando3;
          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max /Conex;
          
        } else if (pdTICKS_TO_MS(xTaskGetTickCount()-xStart) >= 20000) {
          is_LimiteConsumo = IN_Repartir;
          Comands.desired_current = ChargingGroup.potencia_max / Conex;

          Status.Delta = Comands.desired_current - Status.Measures.instant_current;
        } 
        break;

       case IN_ReduccionPc:
        if ((ChargingGroup.potencia_max > Pc) && (Comands.desired_current * Conex <= ChargingGroup.potencia_max)) {
          is_LimiteConsumo = IN_Cargando3;
          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max /
            Conex;
          
        } else {
          Comands.desired_current = ChargingGroup.potencia_max / Conex;
        }
        break;

       default:

        if (Comands.desired_current - Status.Measures.instant_current <= 6) {

          Status.Delta = 0;
          is_LimiteConsumo = IN_Cargando3;
          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max /Conex;
        
        } else {
          Comands.desired_current = ChargingGroup.potencia_max / Conex;
          Status.Delta = Comands.desired_current - Status.Measures.instant_current;
        }
        break;
      }
    }
  }
}

void input_values(){
    Conex=0;
    Delta_total=0;
    Pc=0;
    for(int i=0; i< ChargingGroup.group_chargers.size-1;i++){
        if(!memcmp(ChargingGroup.group_chargers.charger_table[i].HPT,"C2",2)){
            Conex++;
        }

        Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;
        Pc += ChargingGroup.group_chargers.charger_table[i].Current;
    }    
}