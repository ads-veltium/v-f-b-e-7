#ifndef __CONTROL_MAIN
#define __CONTROL_MAIN

#include "serverble.h"
#include "controlLed.h"
#include "DRACO_IO.h"

#include "cybtldr_parse.h"
#include "cybtldr_command.h"
#include "cybtldr_api.h"

#include "tipos.h"
#include "dev_auth.h"
#include "Update.h"
#ifdef USE_WIFI
    #include "FirebaseClient.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include "HardwareSerialMOD.h"
#include "OTA_Firmware_Upgrade.h"

#include <math.h>

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



void updateCharacteristic(uint8_t* data, uint16_t len, uint16_t attrHandle);
void procesar_bloque(uint16 tipo_bloque);
int controlSendToSerialLocal(uint8_t * data, int len);
uint8_t sendBinaryBlock ( uint8_t *data, int len );
void LED_Control(uint8_t luminosity, uint8_t r_level, uint8_t g_level, uint8_t b_level);
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
