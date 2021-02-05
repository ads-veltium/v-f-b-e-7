/*
 * DRACO_IO.c
 *
 *  Created on: 21 feb. 2020
 *      Author: Draco
 */

#include <Arduino.h>

#include "DRACO_IO.h"


unsigned char DRACO_GPIO_Init(void) {

	//INPUTS

	pinMode( GPIO_MODEM_PWRMON, INPUT_PULLUP);
	digitalWrite(GPIO_MODEM_PWRMON, LOW);

	//OUTPUTS
	pinMode ( GPIO_MODEM_PWR_EN,OUTPUT);
	digitalWrite(GPIO_MODEM_PWR_EN, LOW);
            
    pinMode ( EMAC_nRESET,OUTPUT);
	digitalWrite(EMAC_nRESET,LOW);
            

	//OUT OPEN-DRAIN
    pinMode ( nMAIN_XRES,INPUT_PULLUP);    
	DRACO_GPIO_Reset_MCU(1);
	DRACO_GPIO_Reset_MCU(0);
	DRACO_GPIO_Reset_MCU(1);
	return 1;

}



unsigned char DRACO_GPIO_MODEM_Pulse(void)
{
	digitalWrite(GPIO_MODEM_PWR_EN, HIGH);
	vTaskDelay(pdMS_TO_TICKS(5000));
	digitalWrite(GPIO_MODEM_PWR_EN, LOW);
	vTaskDelay(pdMS_TO_TICKS(1000));
	return 0;

}

unsigned char DRACO_GPIO_MODEM_is_Powered(void)
{
	if(!digitalRead(GPIO_MODEM_PWRMON))
	{
		return 1;
	}
	else
	{
		return 0;

	}
}

unsigned char DRACO_GPIO_Reset_MCU( unsigned char val){


	if ( val == 1 )
	{
		pinMode ( nMAIN_XRES,INPUT_PULLUP);
       	    
	}
	if ( val == 0 )
	{
		pinMode ( nMAIN_XRES,OUTPUT);
		digitalWrite(nMAIN_XRES,LOW);
            
	}

	return 0;

}


