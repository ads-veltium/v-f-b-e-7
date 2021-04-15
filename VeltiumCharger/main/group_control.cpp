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
                    cls();
                    print_table(FaseChargers);
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

            print_table(ChargingGroup.group_chargers);
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
    print_table(ChargingGroup.group_chargers);
}

void Calculo_Consigna(){


  // Chart: '<Root>/Charger 1' incorporates:
  //   Inport: '<Root>/Conex'
  //   Inport: '<Root>/Conex_Delta'
  //   Inport: '<Root>/Pc1'
  //   Inport: '<Root>/Pc2'
  //   Inport: '<Root>/Pc3'
  //   Inport: '<Root>/Current'
  //   Inport: '<Root>/potencia_max'
  //   Inport: '<Root>/Delta_total'
  //   Inport: '<Root>/Pw'
  //   Inport: '<Root>/contador'
  //   Outport: '<Root>/desired_current'

  if (is_active_c3_Charger == 0U) {
    is_active_c3_Charger = 1U;
    is_c3_Charger = IN_CochesConectados;

    // Outport: '<Root>/desired_current'                           
    Comands.desired_current = 0.0;
    //rtDW.Pc = (rtU.Pc1 + rtU.Pc2) + rtU.Pc3;

    // Outport: '<Root>/resetCont' incorporates:
    //   Inport: '<Root>/Pc1'
    //   Inport: '<Root>/Pc2'
    //   Inport: '<Root>/Pc3'

    rtY.resetCont = 1.0;
  } else if (is_c3_Charger == IN_CochesConectados) {
    if ((Pc > 0.0) && (Status.HPT_status == "C2")) {
      is_c3_Charger = IN_LimiteConsumo;
      //rtDW.Pc = (rtU.Pc1 + rtU.Pc2) + rtU.Pc3;
      if (Delta_total == 0.0) {
        is_LimiteConsumo = IN_Cargando1;

        // Outport: '<Root>/desired_current' incorporates:
        //   Inport: '<Root>/Pw'

        Comands.desired_current = MAX_CURRENT;
      } else {
        if (Delta_total > 0.0) {
          is_LimiteConsumo = IN_Cargando3;

          // Outport: '<Root>/desired_current' incorporates:
          //   Inport: '<Root>/Conex_Delta'
          //   Inport: '<Root>/potencia_max'

          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max/Conex;

          // Outport: '<Root>/resetCont'
          rtY.resetCont = 1.0;
        }
      }
    } else {
      // Outport: '<Root>/desired_current'
      Comands.desired_current = 0.0;
      //rtDW.Pc = (rtU.Pc1 + rtU.Pc2) + rtU.Pc3;

      // Outport: '<Root>/resetCont' incorporates:
      //   Inport: '<Root>/Pc1'
      //   Inport: '<Root>/Pc2'
      //   Inport: '<Root>/Pc3'

      rtY.resetCont = 1.0;
    }
  } else {
    // case IN_LimiteConsumo:
    if ((Pc == 0.0) || (Status.HPT_status != "C2") {
      is_LimiteConsumo = IN_NO_ACTIVE_CHILD;
      is_c3_Charger = IN_CochesConectados;

      // Outport: '<Root>/desired_current'
      Comands.desired_current = 0.0;
      //rtDW.Pc = (rtU.Pc1 + rtU.Pc2) + rtU.Pc3;
    
      // Outport: '<Root>/resetCont' incorporates:
      //   Inport: '<Root>/Pc1'
      //   Inport: '<Root>/Pc2'
      //   Inport: '<Root>/Pc3'

      rtY.resetCont = 1.0;
    } else {
      //rtDW.Pc = (rtU.Pc1 + rtU.Pc2) + rtU.Pc3;
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

          // Outport: '<Root>/desired_current'
          Comands.desired_current = ChargingGroup.potencia_max / Conex;
        } else if (Comands.desired_current * Conex < ChargingGroup.potencia_max) {
          is_LimiteConsumo = IN_Cargando1;

          // Outport: '<Root>/desired_current' incorporates:
          //   Inport: '<Root>/Pw'

          Comands.desired_current = MAX_CURRENT;
        } else {
          // Outport: '<Root>/desired_current' incorporates:
          //   Inport: '<Root>/Pw'

          Comands.desired_current = MAX_CURRENT;
        }
        break;

       case IN_Cargando3:
        if (ChargingGroup.potencia_max > Conex * MAX_CURRENT) {
          is_LimiteConsumo = IN_Cargando1;

          // Outport: '<Root>/desired_current'
          Comands.desired_current = MAX_CURRENT;
        } else if (Comands.desired_current - Status.Measures.instant_current > 6.0) {
          is_LimiteConsumo = IN_Contador;

          // Outport: '<Root>/resetCont'
          rtY.resetCont = 0.0;
        } else {
          // Outport: '<Root>/desired_current' incorporates:
          //   Inport: '<Root>/Conex_Delta'
          //   Inport: '<Root>/Delta_total'

          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max / Conex;
          
          if (Comands.desired_current >= MAX_CURRENT){
              Comands.desired_current = MAX_CURRENT;
          }

          // Outport: '<Root>/resetCont'
          rtY.resetCont = 1.0;
        }
        break;

       case IN_Contador:
        if (Comands.desired_current - Status.Measures.instant_current <= 6.0) {
          is_LimiteConsumo = IN_Cargando3;
          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max /
            Conex;

          // Outport: '<Root>/resetCont' incorporates:
          //   Inport: '<Root>/Conex_Delta'
          //   Inport: '<Root>/potencia_max'
          //   Inport: '<Root>/Delta_total'

          rtY.resetCont = 1.0;
        } else if (rtU.contador >= 20.0) {
          is_LimiteConsumo = IN_Repartir;
          Comands.desired_current = ChargingGroup.potencia_max / Conex;

          // Outport: '<Root>/Delta' incorporates:
          // Inport: '<Root>/potencia_max'
          Status.Delta = Comands.desired_current - Status.Measures.instant_current;
        } else {
          // Outport: '<Root>/resetCont'
          rtY.resetCont = 0.0;
        }
        break;

       case IN_ReduccionPc:
        if ((ChargingGroup.potencia_max > Pc) && (Comands.desired_current * Conex <= ChargingGroup.potencia_max)) {
          is_LimiteConsumo = IN_Cargando3;
          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max /
            Conex;

          // Outport: '<Root>/resetCont' incorporates:
          //   Inport: '<Root>/Conex_Delta'
          //   Inport: '<Root>/Delta_total'

          rtY.resetCont = 1.0;
        } else {
          Comands.desired_current = ChargingGroup.potencia_max / Conex;
        }
        break;

       default:
        // case IN_Repartir:
        if (Comands.desired_current - Status.Measures.instant_current <= 6.0) {
          // Outport: '<Root>/Delta'
          Status.Delta = 0.0;
          is_LimiteConsumo = IN_Cargando3;
          Comands.desired_current = Delta_total / (Conex - Conex_Delta) + ChargingGroup.potencia_max /
            Conex;

          // Outport: '<Root>/resetCont' incorporates:
          //   Inport: '<Root>/Conex_Delta'
          //   Inport: '<Root>/potencia_max'
          //   Inport: '<Root>/Delta_total'

          rtY.resetCont = 1.0;
        } else {
          Comands.desired_current = ChargingGroup.potencia_max / Conex;

          // Outport: '<Root>/Delta' incorporates:
          //   Inport: '<Root>/potencia_max'

          Status.Delta = Comands.desired_current - Status.Measures.instant_current;
        }
        break;
      }
    }
  }

  // End of Chart: '<Root>/Charger 1'
}

void input_values(){
    for(int i=0; i< ChargingGroup.group_chargers.size-1;i++)
    {
        if(ChargingGroup.group_chargers.charger_table[i].HPT == "C2"){
                Conex+=1;
        }

        Delta_total += ChargingGroup.group_chargers.charger_table[i].Delta;

        Pc += CurrenChargingGroup.group_chargers.charger_table[i].Current:
    }    
}