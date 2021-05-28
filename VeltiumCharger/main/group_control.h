#ifndef GROUP_CONTROL_H
#define GROUP_CONTROL_H

#include "control.h"
#define DELTA_TIMEOUT 20000
#define MAX_CURRENT 32 

#define GROUP_PARAMS    1
#define GROUP_CONTROL   2
#define GROUP_CHARGERS  3
#define CURRENT_COMMAND 6
#define TURNO           4
#define NEW_DATA        5
      
carac_charger New_Data(uint8_t* Buffer, int Data_size);
void New_Params(uint8_t* Buffer, int Data_size);
void New_Control(uint8_t* Buffer, int Data_size);
void New_Group(uint8_t* Buffer, int Data_size);
void New_Current(uint8_t* Buffer, int Data_size);

carac_charger New_Data(char* Data, int Data_size);
void New_Params(char* Data, int Data_size);
void New_Group(char* Data, int Data_size);
void New_Control(char* Data, int Data_size);

void Calculo_General();
void LimiteConsumo(void *p);

typedef struct{
    uint8_t numero = 0;
	int corriente_total = 0;
    int conex = 0;
    int consigna_total = 0;
    float corriente_disponible = 0;
}carac_fase;




#define REPOSO 1
#define EQUILIBRADO 2
#define CALCULO 3

#endif