#ifndef GROUP_CONTROL_H
#define GROUP_CONTROL_H

#include "control.h"
#define DELTA_TIMEOUT 20000
#define MAX_CURRENT 32 

#define GROUP_PARAMS   1
#define GROUP_CONTROL  2
#define GROUP_CHARGERS 3
#define TURNO          4
#define NEW_DATA       5
      
void New_Data(uint8_t* Buffer, int Data_size);
void New_Params(uint8_t* Buffer, int Data_size);
void New_Control(uint8_t* Buffer, int Data_size);
void New_Group(uint8_t* Buffer, int Data_size);

void New_Data(char* Data, int Data_size);
void New_Params(char* Data, int Data_size);
void New_Group(char* Data, int Data_size);
void New_Control(char* Data, int Data_size);

void Calculo_Consigna();
void input_values();
void Ping_Req(const char* Data);

#define IN_Cargando1 1
#define IN_Cargando2 2
#define IN_Cargando3 3
#define IN_Cargando4 4
#define IN_CochesConectados 8
#define IN_Contador 5
#define IN_ContadorFase 9
#define IN_Limitacion 10
#define IN_Limitacion1 11
#define IN_LimiteConsumo 12
#define IN_LimiteInstalacion 13
#define IN_NO_ACTIVE_CHILD 0
#define IN_ReduccionPc 6
#define IN_Repartir 7
#define IN_RepartirFase 14

#endif