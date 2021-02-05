#ifndef __CONTROL_MAIN
#define __CONTROL_MAIN

//configuration
#define USE_WIFI
//#define USE_WIFI_ESP
//#define USE_ETH
#define USE_DRACO_BLE

#include "Arduino.h"
#include "ble/serverble.h"
#include "controlLed.h"
#include "DRACO_IO.h"

#include "cybtldr/cybtldr_parse.h"
#include "cybtldr/cybtldr_api.h"

#include "tipos.h"
#include "ble/dev_auth.h"
#include "Update.h"
#include <stdio.h>
#include <stdlib.h>
#include "HardwareSerialMOD.h"
#include <math.h>
#include "SPIFFS.h"

/*********** Pruebas tar.gz **************/
//Descomentar para probar el sistema de actualizacion con firmware comprimido
//#define UPDATE_COMPRESSED
#ifdef UPDATE_COMPRESSED
	#define  DEST_FS_USES_SPIFFS
	#include "ESP32-targz.h"
#endif

#ifdef USE_WIFI	
	#ifndef CONNECTED
		#define CONNECTED
	#endif
#endif

#ifdef USE_ETH
	#ifndef CONNECTED
		#define CONNECTED
	#endif
#endif

#ifdef CONNECTED
	#include "coms/Wifi_Station.h"
   #include "coms/FirebaseClient.h"
#endif

//Prioridades FreeRTOS
#define PRIORIDAD_LEDS     3
#define PRIORIDAD_CONTROL  1
#define PRIORIDAD_BLE      1
#define PRIORIDAD_DESCARGA 5
#define PRIORIDAD_UPDATE   2
#define PRIORIDAD_FIREBASE 4
#define PRIORIDAD_COMS	   3

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

#endif // __CONTROL_MAIN
