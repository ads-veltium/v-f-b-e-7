#include "group_control.h"
#include "control.h"
#if (defined CONNECTED && defined USE_GROUPS)
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

bool add_to_group(const char* ID, IPAddress IP, carac_charger* group, uint8_t* size);
void remove_group(carac_charger* group, uint8_t* size);
uint8_t check_in_group(const char* ID, carac_charger* group, uint8_t size);
void store_group_in_mem(carac_charger* group, uint8_t size);
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
    uint8_t index = check_in_group(Cargador.name, charger_table, ChargingGroup.Charger_number);                  
    if(index < 255){
        charger_table[index].Conected = true;
        charger_table[index].Baimena = Cargador.Baimena;
        charger_table[index].Current = Cargador.Current;
        charger_table[index].CurrentB = Cargador.CurrentB;
        charger_table[index].CurrentC = Cargador.CurrentC;
        memcpy(charger_table[index].HPT, Cargador.HPT,3);
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
    
    remove_group(charger_table, &ChargingGroup.Charger_number);
    store_group_in_mem(charger_table,ChargingGroup.Charger_number);

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

void New_Circuit(uint8_t* Buffer, int Data_size){
  if(Data_size <=0){
    return;
  }

  ChargingGroup.Circuit_number = Buffer[0];
  printf("Nuevos circuitos recibidos\n");
  for(uint8_t i=0; i<ChargingGroup.Circuit_number;i++){
    Circuitos[i].limite_corriente = Buffer[i+1];
    printf("%i ", Circuitos[i].limite_corriente);
  }
  printf("\n");
  
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
    carac_charger temp_chargers[MAX_GROUP_SIZE];
    uint8_t temp_chargers_size = 0;

    for(uint8_t i=0; i<ChargingGroup.Charger_number;i++){    
      if(!memcmp(charger_table[i].HPT, "C2",2)){
          memcpy(temp_chargers[temp_chargers_size].name,charger_table[i].name,9);
          temp_chargers[temp_chargers_size].Current = charger_table[i].Current;
          temp_chargers[temp_chargers_size].CurrentB = charger_table[i].CurrentB;
          temp_chargers[temp_chargers_size].CurrentC = charger_table[i].CurrentC;

          temp_chargers[temp_chargers_size].Delta = charger_table[i].Delta;
          temp_chargers[temp_chargers_size].Consigna = charger_table[i].Consigna;
          temp_chargers[temp_chargers_size].Delta_timer = charger_table[i].Delta_timer;
          
          
          temp_chargers_size ++;
      }
    }

    ChargingGroup.Charger_number = 0;

    for(uint8_t i=0; i<numero_de_cargadores;i++){    
        for(uint8_t j =0; j< 8; j++){
            ID[j]=(char)Data[2+i*9+j];
        }

        add_to_group(ID, get_IP(ID), charger_table, &ChargingGroup.Charger_number);

        charger_table[i].Fase = Data[10+i*9]-'0';

        uint8_t index =check_in_group(ID, temp_chargers, temp_chargers_size);
        if(index != 255){
          memcpy(charger_table[i].HPT,"C2",2);
          charger_table[i].Current = temp_chargers[index].Current;
          charger_table[i].CurrentB = temp_chargers[index].CurrentB;
          charger_table[i].CurrentC = temp_chargers[index].CurrentC;
          charger_table[i].Delta = temp_chargers[index].Delta;
          charger_table[i].Consigna = temp_chargers[index].Consigna;
          charger_table[i].Delta_timer = temp_chargers[index].Delta_timer;
        }
        

        if(!memcmp(ID,ConfigFirebase.Device_Id,8)){
            charger_table[i].Conected = true;
            Params.Fase =Data[10+i*9]-'0';
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

  Fases[0].numero = 1;
  Fases[1].numero = 2;
  Fases[2].numero = 3;

  while (1){
    
    switch(ControlGrupoState){
      //Estado reposo, no hay nadie cargando
      case REPOSO:
        for(int i = 0; i < ChargingGroup.Charger_number; i++){
          if(!memcmp(charger_table[i].HPT, "B1", 2) && charger_table[i].Baimena){
            memcpy(charger_table[i].HPT, "C2", 2);
            ControlGrupoState = CALCULO;
          }
          else{
            charger_table[i].Consigna = 0;
          }
        }
          
      break;

      case CALCULO:{
        //Calculo general
        Calculo_General();

        //Ver corriente y consigna disponible total
        if(Conex <= 0){
          break;
        }
        float Corriente_disponible_total = ChargingGroup.Params.potencia_max / Conex;

        //Repartir toda la potencia disponible viendo cual es la mas pequeña
        for(int i = 0; i < ChargingGroup.Charger_number; i++){
          if(!memcmp(charger_table[i].HPT,"C2",2)){
            if(Fases[charger_table[i].Fase-1].corriente_disponible <= Corriente_disponible_total /*&& Fases[charger_table[i].Fase-1].corriente_disponible <= Circuitos[charger_table[i].Circuito-1].corriente_disponible*/)
              charger_table[i].Consigna =  Fases[charger_table[i].Fase-1].corriente_disponible;
            /*else if(Corriente_disponible_total <= Circuitos[charger_table[i].Circuito-1].corriente_disponible)
              charger_table[i].Consigna = Corriente_disponible_total;*/
            else
              charger_table[i].Consigna = Corriente_disponible_total;
            
            
            charger_table[i].Consigna = charger_table[i].Consigna > 32 ? 32 : charger_table[i].Consigna;
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
        Calculo_General();

        //Comprobar si ha cambiado el numero de equipos (Alguien se desconecta)
        if(LastConex != Conex){
          ControlGrupoState = Conex == 0 ? REPOSO : CALCULO;
          LastConex = Conex;
          break;
        }
        LastConex = Conex;

        //Comprobar si en algun momento nos estamos pasando de los limites
        if(Consumo_total > ChargingGroup.Params.potencia_max){
          ControlGrupoState = CALCULO;
          break;
        }

        //Comprobar que no nos pasamos de los limites de las fases
        for(uint8_t i =0; i < 3; i++){
          if(Fases[i].corriente_total > ChargingGroup.Params.inst_max || Fases[i].consigna_total > ChargingGroup.Params.inst_max){
            ControlGrupoState = CALCULO;
            break;
          }
        }

        //Comprobar que no nos pasamos de los limites de los circuitos
        /*for(uint8_t i =0; i < ChargingGroup.Circuit_number; i++){
          if(Circuitos[i].corriente_total > ChargingGroup.Params.inst_max || Circuitos[i].consigna_total > ChargingGroup.Params.inst_max){
            ControlGrupoState = CALCULO;
            break;
          }
        }*/

        if(ControlGrupoState == CALCULO){
          break;
        }
            

        //Comprobar si algun equipo quiere comenzar a cargar
        for(int i = 0; i < ChargingGroup.Charger_number; i++){
          if(!memcmp(charger_table[i].HPT, "B1", 2) && charger_table[i].Baimena){
            //Compruebo si entraria en el grupo con la corriente general
            if(ChargingGroup.Params.potencia_max / (Conex+1) >= 6){
              printf("Entra en el grupo!\n");
              //Compruebo si entraria en la fase 
              if(ChargingGroup.Params.inst_max / (Fases[charger_table[i].Fase-1].conex +1) > 6){
                printf("Entra en la fase!\n");
                //Compruebo si entraria en el circuito
                //if(ChargingGroup.Params.inst_max / (Circuitos[charger_table[i].Circuito-1].conex +1) > 6){
                  printf("Entra en el circuito!\n");
                  memcpy(charger_table[i].HPT, "C2", 2);
                  ControlGrupoState = CALCULO;
                //}
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
            if(!memcmp(charger_table[i].HPT,"C2",2)){
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

    for(uint8_t i =0; i < ChargingGroup.Circuit_number; i++){
        Circuitos[i].conex = 0;
        Circuitos[i].corriente_total = 0;
        Circuitos[i].consigna_total  = 0;
    }

    for(int i = 0; i < ChargingGroup.Charger_number; i++){

        //Datos por Grupo
        if(!memcmp(charger_table[i].HPT,"C2",2)){
            Conex++;
            uint8_t faseA, faseB = 0, faseC = 0;
            faseA =charger_table[i].Fase -1;

            //Datos por fase
            Fases[faseA].conex++;
            Fases[faseA].corriente_total += charger_table[i].Current;
            Fases[faseA].consigna_total += charger_table[i].Consigna;
            
            //Si es trifasico, los datos deben ir a todas las fases
            if(charger_table[i].CurrentB > 0 || charger_table[i].CurrentC > 0){
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
                Fases[faseB].conex++;
                Fases[faseB].corriente_total += charger_table[i].CurrentB;
                Fases[faseB].consigna_total += charger_table[i].Consigna;
              }
              if(charger_table[i].CurrentC > 0 ){
                Fases[faseC].conex++;
                Fases[faseC].corriente_total += charger_table[i].CurrentC;
                Fases[faseC].consigna_total += charger_table[i].Consigna;
              }
            }

            //si no es trifasico, actualizamos circuitos
            else{
              //Datos por circuito
              Circuitos[charger_table[i].Circuito-1].conex++;
              Circuitos[charger_table[i].Circuito-1].corriente_total += charger_table[i].Current;
              Circuitos[charger_table[i].Circuito-1].consigna_total += charger_table[i].Consigna;
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
        }
        total_pc += charger_table[i].Current;
    }   


    //corrientes por fase
    for(uint8_t i =0; i < 3; i++){
      Fases[i].corriente_total /= 100;
      //Calcular la corriente disponible en cada fase
      if(Fases[i].conex > 0){
        Fases[i].corriente_disponible = ChargingGroup.Params.inst_max / Fases[i].conex;
      }
      else{
        Fases[i].corriente_disponible = ChargingGroup.Params.inst_max;
      }
      Fases[i].corriente_sobrante = ChargingGroup.Params.inst_max - Fases[i].consigna_total;
    }

    //Corrientes por circuito
    for(uint8_t i =0; i < ChargingGroup.Circuit_number; i++){
      Circuitos[i].corriente_total /= 100;
      //Calcular la corriente disponible en cada circuito
      if(Circuitos[i].conex > 0){
        Circuitos[i].corriente_disponible = Circuitos[i].limite_corriente / Circuitos[i].conex;
      }
      else{
        Circuitos[i].corriente_disponible = Circuitos[i].limite_corriente;
      }
      Circuitos[i].corriente_sobrante = Circuitos[i].consigna_total - Circuitos[i].consigna_total;
    }

    Consumo_total = total_pc/100;

#ifdef DEBUG_GROUPS
    printf("\n\nTotal PC of phase %i %i %i\n",Fases[0].corriente_total, Fases[1].corriente_total, Fases[2].corriente_total); 
    printf("Total consigna of phase %i %i %i\n",Fases[0].consigna_total, Fases[1].consigna_total, Fases[2].consigna_total); 
    printf("Total conex of phase %i %i %i\n",Fases[0].conex, Fases[1].conex, Fases[2].conex); 
    printf("Corriente disponible fases %f %f %f\n",Fases[0].corriente_disponible, Fases[1].corriente_disponible, Fases[2].corriente_disponible); 
    printf("Total conex %i\n",Conex); 
    printf("Consumo total %i \n\n",Consumo_total);    
#endif
}

void Reparto_Delta(){

  //Ajustamos las consignas de los que les sobra corriente
  for(int i = 0; i < ChargingGroup.Charger_number; i++){
      if(!memcmp(charger_table[i].HPT,"C2",2)){

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
    if(!memcmp(charger_table[i].HPT,"C2",2) && charger_table[i].Delta == 0){
      //Equipo monofásico
      if(charger_table[i].CurrentB == 0 && charger_table[i].CurrentC == 0){
        //Comprobar si la fase de este cargador me permite ampliar
        if(Fases[charger_table[i].Fase-1].corriente_sobrante - 1 > 0){
          //comprobar que el circuito me permite añadir carga
          //if(Circuitos[charger_table[i].Circuito-1].corriente_sobrante - 1 > 0){
            Fases[charger_table[i].Fase-1].corriente_sobrante --;
            //Circuitos[charger_table[i].Circuito-1].corriente_sobrante --;
            Delta_total --;
            charger_table[i].Consigna ++;
          //}
        }
      }

      //Equipo trifásico
      else {
        //Comprobar si tengo tres amperios para repartir
        if(Delta_total >= 3){
          //Comprobar si las tres fases me permiten ampliar
          if(Fases[0].corriente_sobrante - 1 > 0){
            if(Fases[1].corriente_sobrante - 1 > 0){
              if(Fases[2].corriente_sobrante - 1 > 0){
                Fases[0].corriente_sobrante --;
                Fases[1].corriente_sobrante --;
                Fases[2].corriente_sobrante --;
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