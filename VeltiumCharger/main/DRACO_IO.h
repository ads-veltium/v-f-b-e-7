/*
 * DRACO_IO.h
 *
 *  Created on: 21 feb. 2020
 *      Author: Draco
 */

#ifndef MAIN_DRACO_IO_H_
#define MAIN_DRACO_IO_H_

#include "Arduino.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


//define IO ports
//INPUTS
#define GPIO_MODEM_PWRMON		36

#define GPIO_UART2_CTS_LOCAL	34 //UART FLOW CONTROL TO MCU CYPRESS

//OUTPUTS
#define GPIO_UART2_RTS_LOCAL	4	//UART FLOW CONTROL TO MCU CYPRESS
#define GPIO_MODEM_PWR_EN		2   //SIGNAL TO POWER-ON/OFF MODEM
#define EMAC_nRESET				12	//RESET OF PHY ETHERNET SWITCH IC
#define DO_NEOPIXEL				14  //DATA LINE BUS NEOPIXEL LED STRING
#define EN_NEOPIXEL                             13   // NEOPIXEL POWER ENABLE


//PERIPHERAL GPIOS
#define GPIO_UART1_RX_MODEM		35	//UART TO MODEM
#define GPIO_UART1_TX_MODEM		32	//UART TO MODEM

#define UART2_RX_LOCAL			16	// RX, INPUT, UART TO MCU CYPRESS
#define UART2_TX_LOCAL			17  // TX , OUTPUT , UART TO MCU CYPRESS
#define UART2_CTS_LOCAL			6	// CTS, IN , UART TO MCU  -> NOT USED
#define UART2_RTS_LOCAL			4	// RTS, OUT , UART TO MCU -> NOT USED

#define EMAC_RXD0				25
#define EMAC_RXD1				26
#define EMAC_CRS_DV				27
#define EMAC_REF_CLK			0
#define EMAC_TXD0				19
#define EMAC_TXD1				22
#define EMAC_MDC				23
#define EMAC_MDIO				18
#define EMAC_TXEN				21



//OUTPUT OPEN-DRAIN
#define nMAIN_XRES				33	//RESET SIGNAL TO MCU CYPRESS


unsigned char DRACO_GPIO_Init(void);

unsigned char DRACO_GPIO_MODEM_Pulse(void);
unsigned char DRACO_GPIO_MODEM_is_Powered(void); 
unsigned char DRACO_GPIO_Reset_MCU( unsigned char val);


#endif /* MAIN_DRACO_IO_H_ */
