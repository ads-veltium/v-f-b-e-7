#ifndef GROUP_CONTROL_H
#define GROUP_CONTROL_H
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CURRENT 32 
      
void New_Data(char* Data, int Data_size);
void New_Params(char* Data, int Data_size);
void Calculo_Consigna();
void input_values();

#define IN_Cargando1        1
#define IN_Cargando2        2
#define IN_Cargando3        3
#define IN_CochesConectados 1
#define IN_Contador         4
#define IN_LimiteConsumo    2
#define IN_NO_ACTIVE_CHILD  0
#define IN_ReduccionPc      5
#define IN_Repartir         6

#ifdef __cplusplus
}
#endif
#endif