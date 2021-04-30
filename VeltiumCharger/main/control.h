#ifndef __CONTROL_MAIN
#define __CONTROL_MAIN

//configuration
#define USE_COMS
#define USE_DRACO_BLE

#define DEVELOPMENT
#define GROUPS

//#define DEBUG
//#define DEBUG_GROUPS

#define MAX_GROUP_SIZE 50

#include "Arduino.h"
#include "tipos.h"

#ifdef USE_COMS	
	#include "coms/Wifi_Station.h"
	#ifndef CONNECTED
		#define CONNECTED
	#endif
#endif

#include "ble/serverble.h"
#include "controlLed.h"
#include "DRACO_IO.h"

#include "cybtldr/cybtldr_parse.h"
#include "cybtldr/cybtldr_api.h"

#include "ble/dev_auth.h"
#include "Update.h"
#include <stdio.h>
#include <stdlib.h>
#include "HardwareSerialMOD.h"
#include <math.h>
#include "SPIFFS.h"
#include "helpers.h"


/*********** Pruebas tar.gz **************/
//Descomentar para probar el sistema de actualizacion con firmware comprimido
//#define UPDATE_COMPRESSED
#ifdef UPDATE_COMPRESSED
	#define  DEST_FS_USES_SPIFFS
	#include "ESP32-targz.h"
#endif


#define BLE  0
#define COMS 1
//Prioridades FreeRTOS
#define PRIORIDAD_LEDS     1
#define PRIORIDAD_CONTROL  4
#define PRIORIDAD_BLE      2
#define PRIORIDAD_FIREBASE 2
#define PRIORIDAD_COMS	   1
#define PRIORIDAD_MQTT     2

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
#define TIMEOUT_TX_BLOQUE	75
#define TIMEOUT_TX_BLOQUE2	20
#define TIMEOUT_RX_BLOQUE	10
#define TIMEOUT_INICIO		10
#define TIME_PARPADEO		50

#define HEADER_RX			':'
#define HEADER_TX			':'

#define STACK_SIZE 4096*4

//SISTEMA DE ACTUALIZACION
#define VBLE_UPDATE    1
#define VELT_UPDATE    2
/*Direcciones del archivo*/
#define PRIMERA_FILA 14
#define TAMANO_FILA  527

void updateCharacteristic(uint8_t* data, uint16_t len, uint16_t attrHandle);
void procesar_bloque(uint16 tipo_bloque);
int controlSendToSerialLocal(uint8_t * data, int len);
void UpdateCompressedTask(void *arg);
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
void modifyCharacteristic(uint8* data, uint16 len, uint16 attrHandle);

void SendToPSOC5(uint8 data, uint16 attrHandle);
void SendToPSOC5(char *data, uint16 len, uint16 attrHandle);

uint32 GetStateTime(TickType_t xStart);
#endif // __CONTROL_MAIN
