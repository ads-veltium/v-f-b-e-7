#ifndef __HELPERS_COMS_H
#define __HELPERS_COMS_H

#include "../control.h"

/***********************************************************
//Actualizar el valor de la caracteristica Status_Coms
//que informa del estado de las comunicaciones dell equipo
***********************************************************/
void Update_Status_Coms(uint16_t Code,uint8_t block = 0);

/***********************************************************
 * Calcula la diferencia de tiempo desde xStart asta el 
 * momento actual
***********************************************************/
uint32 GetStateTime(TickType_t xStart);

/***********************************************************
		    Convertir fecha a timestamp epoch
***********************************************************/
int Convert_To_Epoch(uint8* data);

#endif