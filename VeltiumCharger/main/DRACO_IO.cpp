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
	digitalWrite(GPIO_MODEM_PWR_EN, LOW);
        pinMode ( GPIO_MODEM_PWR_EN,OUTPUT);    
 
	digitalWrite(EMAC_nRESET,LOW);
        pinMode ( EMAC_nRESET,OUTPUT);    

	//OUT OPEN-DRAIN
        pinMode ( nMAIN_XRES,INPUT_PULLUP);    

	return 1;

}



unsigned char DRACO_GPIO_MODEM_Pulse(void)
{
	digitalWrite(GPIO_MODEM_PWR_EN, HIGH);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	digitalWrite(GPIO_MODEM_PWR_EN, LOW);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
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

unsigned char DRACO_GPIO_Reset_MCU( unsigned char val)
{
	Serial.println("Reset called");
	if ( val == 1 )
	{
       		pinMode ( nMAIN_XRES,INPUT_PULLUP);    
	}
	if ( val == 0 )
	{
		digitalWrite(nMAIN_XRES,LOW);
        	pinMode ( nMAIN_XRES,OUTPUT);    
	}

	return 0;

}


