#ifndef __CONTROLLED_H
#define __CONTROLLED_H


//HSV Colors
#define VERDE          100
#define VERDE_CLARITO  120
#define NARANJA_OSCURO  20
#define MORADO_CLARITO 200
#define ROSA           225
#define AZUL_OSCURO    160
#define AMARILLO        65
#define ROJO             0
#define AZUL_CLARITO   130
#define BLANCO           0


void initLeds ( void );
void LedControl_Task(void *arg);
void Kit (void);

#endif // __CONTROLLED_H
