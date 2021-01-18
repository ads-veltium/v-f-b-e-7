#ifndef __CONTROL_MAIN
#define __CONTROL_MAIN

//configuration
#define USE_WIFI
#define USE_DRACO_BLE

#include "Arduino.h"
#include "serverble.h"
#include "controlLed.h"
#include "DRACO_IO.h"

#include "cybtldr_parse.h"
#include "OTA_Firmware_Upgrade.h"
#include "cybtldr_api.h"

#include "tipos.h"
#include "dev_auth.h"
#include "Update.h"
#include <stdio.h>
#include <stdlib.h>
#include "HardwareSerialMOD.h"
#include <math.h>
#include "SPIFFS.h"

#ifdef USE_WIFI
	#include "Wifi_Station.h"
#endif

//Prioridades FreeRTOS
#define PRIORIDAD_LEDS     1
#define PRIORIDAD_CONTROL  3
#define PRIORIDAD_BLE      3
#define PRIORIDAD_DESCARGA 6
#define PRIORIDAD_UPDATE   2
#define PRIORIDAD_FIREBASE 5

// ESTADO GENERAL
#define	ESTADO_ARRANQUE			0
#define	ESTADO_INICIALIZACION	1
#define	ESTADO_NORMAL			2
#define ESTADO_CONECTADO		3

// ESTADO AUTOMATA UART
#define ESPERANDO_HEADER	0
#define ESPERANDO_TIPO		1
#define ESPERANDO_LONGITUD	2
#define RECIBIENDO_BLOQUE	3

// TIMEOUTS
#define TIMEOUT_TX_BLOQUE	10
#define TIMEOUT_RX_BLOQUE	10
#define TIMEOUT_INICIO		10
#define TIME_PARPADEO		500 		// 500 mS (2seg)

#define HEADER_RX			':'
#define HEADER_TX			':'

#define STACK_SIZE 4096*4

//SISTEMA DE ACTUALIZACION
#define VBLE_UPDATE    1
#define VELT_UPDATE    2


void updateCharacteristic(uint8_t* data, uint16_t len, uint16_t attrHandle);
void procesar_bloque(uint16 tipo_bloque);
int controlSendToSerialLocal(uint8_t * data, int len);
uint8_t sendBinaryBlock ( uint8_t *data, int len );
int controlMain(void);
void deviceConnectInd(void);
void deviceDisconnectInd(void);
uint8_t isMainFwUpdateActive(void);
uint8_t setMainFwUpdateActive(uint8_t val);
uint8_t setAuthToken(uint8_t *data, int len);
uint8_t authorizedOK(void);
void MAIN_RESET_Write(uint8_t val);
void controlInit(void);
void UpdateTask(void *arg);
void UpdateESP();

#endif // __CONTROL_MAIN
