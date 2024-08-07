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

extern uint8 Bloqueo_de_carga;

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

int delta_total;
float corriente_disponible_total = 0;
float corriente_disponible_limitada = 0;
uint8_t consumo_total = 0;
uint8_t conex_delta;
uint8_t conex;
uint8_t conex_con_tri =0;

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
      Cargador.ChargeReq = cJSON_GetObjectItem(mensaje_Json,"Perm")->valueint;
      }
    //Datos para los trifasicos
    if(cJSON_HasObjectItem(mensaje_Json,"currentB")){
      Cargador.CurrentB = cJSON_GetObjectItem(mensaje_Json,"currentB")->valueint;
    }
    if(cJSON_HasObjectItem(mensaje_Json,"currentC")){
      Cargador.CurrentC = cJSON_GetObjectItem(mensaje_Json,"currentC")->valueint;
    }
#ifdef DEBUG_COAP
    Serial.printf("group_control - New_Data: Cargador %s con HPT %s, Corriente %i y Perm %i\n", Cargador.name, Cargador.HPT, Cargador.Current, Cargador.ChargeReq);
#endif
    cJSON_Delete(mensaje_Json);

    //Actualizacion del grupo total
    //Buscar el equipo en el grupo total
    uint8_t index = check_in_group(Cargador.name, charger_table, ChargingGroup.Charger_number);                  
    if(index < 255){
        charger_table[index].Conected = true;
        charger_table[index].ChargeReq = Cargador.ChargeReq;
        
        //si está pidiendo permiso, no actualizamos su hpt ni sus corrientes
        if(!Cargador.ChargeReq){ 
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
  Serial.printf("pot_max %i\n", Maxima);
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
    #ifdef DEBUG_GROUPS
      Serial.printf("Tengo que pausar el grupo\n");
    #endif
    broadcast_a_grupo("Pause group", 11);
    ChargingGroup.StopOrder = true;
    ChargingGroup.Params.GroupActive = 0;
    store_params_in_mem();
  }
  else if(!memcmp(Data,"Delete",6)){
    #ifdef DEBUG_GROUPS
      Serial.printf("Tengo que borrar el grupo\n");
    #endif
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
#ifdef DEBUG_COAP
  char *jsonPrintString = cJSON_Print(mensaje_Json);
  Serial.printf("group_control - New_Current: JSON RECIBIDO: %s\n", jsonPrintString);
#endif
  free(Data);

  //comprobar que el Json está bien
  if(!cJSON_HasObjectItem(mensaje_Json,"DC")){
    return;
  }
  
  //si no son mis datos los ignoro
  if(memcmp(ConfigFirebase.Device_Id,cJSON_GetObjectItem(mensaje_Json,"N")->valuestring,9)){
    Serial.printf("No son mis datos!\n");
    return;
  }
  //Extraer los datos
  uint8_t desired_current = (uint8_t) cJSON_GetObjectItem(mensaje_Json,"DC")->valueint;
  if(cJSON_HasObjectItem(mensaje_Json,"P")){
    ChargingGroup.ChargPerm = (bool) cJSON_GetObjectItem(mensaje_Json,"P")->valueint;
  }

  cJSON_Delete(mensaje_Json);

  //Borrar la petición de permiso   
  if (ChargingGroup.ChargPerm){  
    ChargingGroup.AskPerm = 0;
  }
  
  if(desired_current == 0 && !ChargingGroup.ChargPerm && !memcmp(Status.HPT_status,"C2",2)){
#ifdef DEBUG_GROUPS
    Serial.printf("group_control - New_Current: ¡ Detener la carga !\n");
    Serial.printf("group_control - New_Current: Envío 1 en BLOQUEO_CARGA al PSoC. Bloqueo_de_carga %i=\n",Bloqueo_de_carga);
    Serial.printf("group_control - New_Current: Envío desired_current = %i al PSoC\n",desired_current);
#endif
    SendToPSOC5(1, BLOQUEO_CARGA);
    SendToPSOC5(desired_current,MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
  }

  if(desired_current!=Comands.desired_current &&  (!memcmp(Status.HPT_status,"C2",2) || ChargingGroup.ChargPerm)){
#ifdef DEBUG_GROUPS
    Serial.printf("group_control - New_Current: Envío desired_current = %i al PSoC\n",desired_current);
#endif      
      SendToPSOC5(desired_current,MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
      if(ChargingGroup.ChargPerm){
#ifdef DEBUG_GROUPS
        Serial.printf("group_control - New_Current: Envío Bloqueo_de_carga = 0 al PSoC\n");
#endif
        SendToPSOC5(0, BLOQUEO_CARGA);
      }
  }

}

void New_Circuit(uint8_t* Buffer, int Data_size){
  if(Data_size <=0){
    return;
  }
  
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
          memcpy(temp_chargers[temp_chargers_size].HPT,charger_table[i].HPT,3);
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
            ID[j]=(char)Data[2+i*11+j];
        }

        add_to_group(ID, get_IP(ID), charger_table, &ChargingGroup.Charger_number); 
        
        char v[3];
        memcpy(v,&Data[10+i*11],3);
        uint8_t value = atoi(v);

        printf("Crudo fase y circuito %s %i %i %i\n",ID, value, value & 0x03, value >> 2);



        charger_table[i].Fase = value & 0x03;
        charger_table[i].Circuito = value >> 2;

        uint8_t index =check_in_group(ID, temp_chargers, temp_chargers_size);
        if(index != 255){
          memcpy(charger_table[i].HPT,temp_chargers[index].HPT,3);
          charger_table[i].Current = temp_chargers[index].Current;
          charger_table[i].CurrentB = temp_chargers[index].CurrentB;
          charger_table[i].CurrentC = temp_chargers[index].CurrentC;
          charger_table[i].Delta = temp_chargers[index].Delta;
          charger_table[i].Consigna = temp_chargers[index].Consigna;
          charger_table[i].Delta_timer = temp_chargers[index].Delta_timer;
        }
        

        if(!memcmp(ID,ConfigFirebase.Device_Id,8)){
            charger_table[i].Conected = true;
        }
    }
    store_group_in_mem(charger_table, ChargingGroup.Charger_number);
    #ifdef DEBUG_GROUPS
    print_table(charger_table, "Grupo desde COAP",ChargingGroup.Charger_number);
    #endif
}

void LimiteConsumo(void *p){
  static uint8_t Estado_Anterior = REPOSO;
  static uint8_t ControlGrupoState = REPOSO;
  static uint8_t LastConex = 0;

  TickType_t Delta_Timer = xTaskGetTickCount();
  TickType_t Start_Timer = xTaskGetTickCount();
#ifdef DEVELOPMENT
  Serial.printf("group_control - LimiteConsumo: Arrancada tarea LimiteConsumo\n");
#endif
  while (1){
#ifdef DEBUG_GROUPS
    if (Estado_Anterior != ControlGrupoState){
      Serial.printf("group_control - LimiteConsumo: La maquina de control de grupo pasa de %s a %s\n",
                    (Estado_Anterior == REPOSO) ? "REPOSO" : (Estado_Anterior == CALCULO)   ? "CALCULO"
                                                         : (Estado_Anterior == EQUILIBRADO) ? "EQUILIBRADO"
                                                                                            : "OTRO",
                    (ControlGrupoState == REPOSO) ? "REPOSO" : (ControlGrupoState == CALCULO)   ? "CALCULO"
                                                           : (ControlGrupoState == EQUILIBRADO) ? "EQUILIBRADO"
                                                                                                : "OTRO");
      Estado_Anterior = ControlGrupoState;
    }
#endif
    switch(ControlGrupoState){
      //Estado reposo, no hay nadie cargando
      case REPOSO:
        if(pdTICKS_TO_MS(xTaskGetTickCount() - Start_Timer) > GROUP_REST_TIME){ 
          for(int i = 0; i < ChargingGroup.Charger_number; i++){
            if(!memcmp(charger_table[i].HPT, "B1", 2) && charger_table[i].ChargeReq){
              
              //cls();
              float reparto = (ChargingGroup.Params.potencia_max *100/230);
              if(charger_table[i].trifasico){
                reparto = (ChargingGroup.Params.potencia_max *100/230) / 3;
              }
              
              if(reparto >= 6){
#ifdef DEBUG_GROUPS
                print_table(charger_table, "Grupo en reposo", ChargingGroup.Charger_number);
#endif
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
        if(conex <= 0){
          ControlGrupoState = REPOSO;
          break;
        }

        //Repartir toda la potencia disponible viendo cual es la mas pequeña
        for(int i = 0; i < ChargingGroup.Charger_number; i++){
          if(!memcmp(charger_table[i].HPT,"C2",2) || !memcmp(charger_table[i].HPT,"B",1)){
            if(Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].corriente_disponible <= corriente_disponible_total)
              charger_table[i].Consigna =  Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].corriente_disponible;
            else
              charger_table[i].Consigna = corriente_disponible_total;
            
            charger_table[i].Consigna = charger_table[i].Consigna > 32 ? 32 : charger_table[i].Consigna;

            //Si es trifasico, tenemos que tener en cuenta tres potencias mas
            if(charger_table[i].CurrentB > 250 || charger_table[i].CurrentC>250){
              if(charger_table[i].Consigna < 6){
                charger_table[i].Consigna = 0;
                charger_table[i].ChargeReq = 0;
#ifdef DEBUG_GROUPS
                Serial.printf("group_control - LimiteConsumo: CALCULO PASAMOS A %s DE %s A B1\n",charger_table[i].name,charger_table[i].HPT);
#endif
                memcpy(charger_table[i].HPT,"B1",3);
                charger_table[i].Current = 0;
                charger_table[i].Delta = 0;
                charger_table[i].Delta_timer = 0;
                charger_table[i].CurrentB = 0;
                charger_table[i].CurrentC = 0;
                charger_table[i].trifasico =1;
                conex--;
                conex_con_tri -=3;
              }
            }
          }
        }

        //Intentar repartir la corriente sobrante
        if(delta_total > 0){
          // Reparto_Delta(); // ADS Eliminado el reparto de Delta de corriente
        }

        ControlGrupoState = EQUILIBRADO;
        break;
      }

      case EQUILIBRADO:{
#ifdef DEBUG_GROUPS
        cls(); // ADS - Borrado de pantalla
        print_table(charger_table, "Grupo en equilibrado", ChargingGroup.Charger_number);
#endif

        //Calculo general
        if(Calculo_General() || ChargingGroup.NewData){
          ChargingGroup.NewData = false;
          ControlGrupoState = CALCULO;
          break;
        }

        //Comprobar si ha cambiado el numero de equipos (Alguien se desconecta)
        if(LastConex != conex || conex == 0){
          ControlGrupoState = conex == 0 ? REPOSO : CALCULO;
          LastConex = conex;
          break;
        }
        LastConex = conex;

        //Comprobar si en algun momento nos estamos pasando de los limites
        if(consumo_total > corriente_disponible_limitada){
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
          //if(!memcmp(charger_table[i].HPT, "B1", 2) && charger_table[i].ChargeReq){  // ADS Eliminado requisito B1
          if(charger_table[i].ChargeReq){ 
            //Compruebo si entraria en el grupo con la corriente general
            if(charger_table[i].trifasico ){ //Si el equipo estaba detectado como trifasico, debemos tener en cuenta el triple de corriente
              float reparto = (corriente_disponible_limitada) / (conex_con_tri+3);
              if(reparto >= 6){
                Serial.printf("Entra en el grupo!\n");
                //Compruebo si entraria en el circuito           
                if(Circuitos[charger_table[i].Circuito-1].limite_corriente / (Circuitos[charger_table[i].Circuito-1].Fases[0].conex +1 )> 6){
                  if(Circuitos[charger_table[i].Circuito-1].limite_corriente / (Circuitos[charger_table[i].Circuito-1].Fases[1].conex +1 )> 6){
                    if(Circuitos[charger_table[i].Circuito-1].limite_corriente / (Circuitos[charger_table[i].Circuito-1].Fases[2].conex +1 )> 6){
                        Serial.printf("Entra en el circuito!\n");
#ifdef DEBUG_GROUPS
                        Serial.printf("group_control - LimiteConsumo: EQUILIBRADO PASAMOS A %s DE %s A C2\n", charger_table[i].name, charger_table[i].HPT);
#endif
                        memcpy(charger_table[i].HPT, "C2", 3);
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
            else if((corriente_disponible_limitada) / (conex_con_tri+1) >= 6){
              Serial.printf("Entra en el grupo!\n");
              //Compruebo si entraria en el circuito           
               if(Circuitos[charger_table[i].Circuito-1].limite_corriente / (Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].conex +1 )> 6){
                Serial.printf("Entra en el circuito!\n");
#ifdef DEBUG_GROUPS
                Serial.printf("group_control - LimiteConsumo: EQUILIBRADO_2 PASAMOS A %s DE %s A C2\n", charger_table[i].name, charger_table[i].HPT);
#endif
                memcpy(charger_table[i].HPT, "C2", 3);
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

            Serial.printf("No hay sitio en el grupo!\n");
            charger_table[i].Consigna = 0;
          }
        }

        uint16_t transcurrido = pdTICKS_TO_MS (xTaskGetTickCount() - Delta_Timer);

        //Comprobar si alguno de los cargadores tiene corriente de sobra (delta) solo si hay mas de uno cargando
        //Si el equipo es trifasico, solo tenemos en cuenta la corriente de su fase principal
        if(conex >1){
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
              if(charger_table[i].Delta_timer > 10000){ // ADS - Tiempo de respuesta bajado a 10 segundos
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
        Serial.printf("La maquina de Control de grupo tiene algun valor malo\n");
      break;
    }
    delay(500);
  }
}

bool Calculo_General(){
    conex=0;
    conex_con_tri = 0;
    consumo_total = 0;
    delta_total = 0;
    conex_delta = 0;
    uint16_t total_pc =0;
    uint16_t total_consigna=0;
    uint8_t conectados = 0;
    static uint16_t recalc_count =0;
    bool recalcular = false;
    float corriente_CDP__sobra = 0;
    uint16_t corriente_limite_cdp = 0;

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

    for(int i = 0; i < ChargingGroup.Charger_number; i++){
        //Contar cuantos cargadores están conectados
        if(charger_table[i].Conected){
          conectados ++;
        }
#ifdef DEBUG_GROUPS
        Serial.printf("group_control - Calculo_General: Cargador %s. HPT = %s. I = %i. ChargeReq = %i\n",charger_table[i].name,charger_table[i].HPT,charger_table[i].Current,charger_table[i].ChargeReq);
#endif
        //Datos por Grupo
        if(!memcmp(charger_table[i].HPT,"C2",2) || !memcmp(charger_table[i].HPT,"C1",2) || !memcmp(charger_table[i].HPT,"B2",2) || !memcmp(charger_table[i].HPT,"B1",2)){
            conex++;
            conex_con_tri++;
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
              conex_con_tri+=2;
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
              delta_total += charger_table[i].Delta;
              conex_delta ++;
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
            charger_table[i].Consigna =0; // ADS Añadido reseteo de consigna
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

    // Si no están todos conectadoes, tendremos algunos en un estado desconocido, por seguridad limitamos la potencia máxima
    // Si no hay CDP usamos potencia_max. Si la hay, ContractPower
    if (!ContadorConfigurado()){
      if (conectados == ChargingGroup.Charger_number){
        corriente_disponible_limitada = floor(ChargingGroup.Params.potencia_max * 100 / 230);
      }
      else{
        corriente_disponible_limitada = floor((ChargingGroup.Params.potencia_max * conectados / ChargingGroup.Charger_number) * 100 / 230);
      }
    }
    else{
      if (conectados == ChargingGroup.Charger_number){
        corriente_disponible_limitada = floor(ChargingGroup.Params.ContractPower * 100 / 230);
      }
      else{
        corriente_disponible_limitada = floor((ChargingGroup.Params.ContractPower * conectados / ChargingGroup.Charger_number) * 100 / 230);
      }
    }
    consumo_total = ceil(total_pc / 100);

#ifdef DEBUG_GROUPS
    Serial.printf("group_control - Calculo_General: consumo_total = %i \n",consumo_total); 
    Serial.printf("group_control - Calculo_General: conex_con_tri = %i \n",conex_con_tri);
#endif

    if (conex_con_tri > 0){
      corriente_disponible_total = floor(corriente_disponible_limitada / conex_con_tri);
      if (ContadorConfigurado() && ContadorExt.GatewayConectado){
        if (((ChargingGroup.Params.CDP >> 2) & 0x03) == 1){ // Medida domestica
          if (ChargingGroup.Params.ContractPower * 100 > ContadorExt.Power){
            corriente_CDP__sobra = floor((ChargingGroup.Params.ContractPower * 100 - ContadorExt.Power) / 230);
          }
          else {
            corriente_CDP__sobra = 0;
          }
        }
        else if (((ChargingGroup.Params.CDP >> 2) & 0x03) == 2){ // Medida total
#ifdef DEBUG_GROUPS
          Serial.printf("group_control - Calculo_General: ContractPower: %i. ContadorExt.Power: %i. consumo_total: %i\n", ChargingGroup.Params.ContractPower*100, ContadorExt.Power, 230*consumo_total);
#endif
          if (ChargingGroup.Params.ContractPower * 100 > (ContadorExt.Power-(230*consumo_total))){
            corriente_CDP__sobra = (ChargingGroup.Params.ContractPower * 100)/230 + consumo_total - (ContadorExt.Power/230);
          }
          else{
            corriente_CDP__sobra = 0;
          }
        }
        // Establecer el limite disponible
        corriente_limite_cdp = corriente_CDP__sobra;
        if (conectados == ChargingGroup.Charger_number){
          corriente_disponible_limitada = corriente_limite_cdp;
        }
        else{
          corriente_disponible_limitada = corriente_limite_cdp * conectados / ChargingGroup.Charger_number;
        }
        corriente_disponible_total = floor(corriente_disponible_limitada / conex_con_tri);
#ifdef DEBUG_GROUPS
    Serial.printf("group_control - Calculo_General: corriente_limite_cdp = %i \n",corriente_limite_cdp); 
    Serial.printf("group_control - Calculo_General: corriente_disponible_total = %F \n",corriente_disponible_total); 
#endif
        // Ver si nos pasamos
        if (corriente_limite_cdp < consumo_total){ // Estamos por encima del limite
          recalcular = true;
        }
      }
      // Si tenemos un limite menor que 6A para cada coche, debemos expulsar al ultimo que haya entrado
      while(corriente_disponible_total < 6){
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
        charger_table[last_index].ChargeReq = 0;
#ifdef DEBUG_GROUPS
        Serial.printf("group_control - LimiteConsumo: corriente_disponible_total=%f. Echamos a %s, en %s, Corriente=%i y Consigna=%i\n", corriente_disponible_total, charger_table[last_index].name, charger_table[last_index].HPT,charger_table[last_index].Current,charger_table[last_index].Consigna);
#endif
        memcpy(charger_table[last_index].HPT,"B1",3);
        charger_table[last_index].Current = 0;
        charger_table[last_index].Delta = 0;
        charger_table[last_index].Delta_timer = 0;
        charger_table[last_index].CurrentB = 0;
        charger_table[last_index].CurrentC = 0;
        conex--;
        conex_con_tri --;
        if(charger_table[last_index].trifasico){
          conex_con_tri -=2;
        }
        if(conex_con_tri > 0){
          corriente_disponible_total = floor(corriente_disponible_limitada / conex_con_tri);
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
    Serial.printf("conectados %i \n",conectados);
    Serial.printf("conex %i \n",conex);
    Serial.printf("Medida del contador %i W\n",ContadorExt.Power); 
    Serial.printf("Valor CDP %i \n",ChargingGroup.Params.CDP);
    Serial.printf("CDP activo %i \n",ContadorConfigurado());
    Serial.printf("potencia máxima %i \n",ChargingGroup.Params.potencia_max);
    Serial.printf("potencia contratada %i \n",ChargingGroup.Params.ContractPower);
    Serial.printf("corriente_disponible_limitada %f\n",corriente_disponible_limitada); 
    if (ContadorConfigurado()){
      Serial.printf("corriente_CDP__sobra %f \n", corriente_CDP__sobra);
    }
    Serial.printf("Consigna total %i \n",total_consigna);
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
  uint8_t last_delta = delta_total;
  while(delta_total > 0){
    if((!memcmp(charger_table[i].HPT,"C2",2) || !memcmp(charger_table[i].HPT,"B2",2) || !memcmp(charger_table[i].HPT,"C1",2)) && charger_table[i].Delta == 0 && charger_table[i].Consigna < 32){
      //Equipo monofásico
      if(charger_table[i].CurrentB == 0 && charger_table[i].CurrentC == 0){
        //Comprobar si la fase de este cargador me permite ampliar
        if(Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].corriente_sobrante - 1 > 0){
          //comprobar que el circuito me permite añadir carga
            Circuitos[charger_table[i].Circuito-1].Fases[charger_table[i].Fase-1].corriente_sobrante --;
            delta_total --;
            charger_table[i].Consigna ++;
          //}
        }
      }

      //Equipo trifásico
      else {
        //Comprobar si tengo tres amperios para repartir
        if(delta_total >= 3 && charger_table[i].Consigna < 32){
          //Comprobar si las tres fases me permiten ampliar
          if(Circuitos[charger_table[i].Circuito-1].Fases[0].corriente_sobrante - 1 > 0){
            if(Circuitos[charger_table[i].Circuito-1].Fases[1].corriente_sobrante - 1 > 0){
              if(Circuitos[charger_table[i].Circuito-1].Fases[2].corriente_sobrante - 1 > 0){
                Circuitos[charger_table[i].Circuito-1].Fases[0].corriente_sobrante --;
                Circuitos[charger_table[i].Circuito-1].Fases[1].corriente_sobrante --;
                Circuitos[charger_table[i].Circuito-1].Fases[2].corriente_sobrante --;
                delta_total -= 3;
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
      if(last_delta == delta_total){
        break;
      }
      last_delta = delta_total;
    }
  }
}

#endif