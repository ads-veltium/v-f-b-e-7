#ifndef GROUP_CONTROL_H
#define GROUP_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CURRENT 32 
// Named constants for Chart: '<Root>/Charger 1'
const uint8 IN_Cargando1 = 1U;
const uint8 IN_Cargando2 = 2U;
const uint8 IN_Cargando3 = 3U;
const uint8 IN_CochesConectados = 1U;
const uint8 IN_Contador = 4U;
const uint8 IN_LimiteConsumo = 2U;
const uint8 IN_NO_ACTIVE_CHILD = 0U;
const uint8 IN_ReduccionPc = 5U;
const uint8 IN_Repartir = 6U;

uint8 is_active_c3_Charger;
uint8 is_c3_Charger;         
uint8 is_LimiteConsumo;       


void New_Data(char* Data, int Data_size);
void New_Params(char* Data, int Data_size);
void Calculo_Consigna();
void input_values();

int Conex;
int Conex_Delta;
uint16_t Pc;

#ifdef __cplusplus
}
#endif
#endif