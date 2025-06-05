#ifndef __CONTROL_MAIN
#define __CONTROL_MAIN

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wwrite-strings"

/*********** Configuracion del build**************/ 

#define IS_UNO_KUBO               //Comentar para no utilzar las comunicaciones [ Veltium lite Zero]
#ifdef IS_UNO_KUBO
	#define USE_GROUPS			   //Comentar para no utilizar los grupos de potencia
#endif

// #define DEVELOPMENT				   //Comentar para pasar firmware a produccion ( Cambio de base de datos y quitar debugs)

#ifdef DEVELOPMENT
	#define DEBUG				   //Activar los distintos debugs
	#ifdef DEBUG
		#ifdef USE_GROUPS
			#define DEBUG_GROUPS	//Activar el debug de los grupos
			#define DEBUG_COAP		// ADS - Debug de mensajes COAP
			//#define DEBUG_UDP		//Debug de mensajes UDP enviados y recibidos
		#endif

		#define DEBUG_BLE		   //Activar el debug del ble
		//#define DEBUG_CONFIG	   //Debugar el almacenamiento de la configuracion
		//#define DEBUG_UPDATE
		#define DEBUG_TX_UART
		//#define DEBUG_RX_UART
		#define DEBUG_RECORDS

		#ifdef IS_UNO_KUBO	
			//#define DEBUG_WIFI	     //Activar el debug del wifi
			//#define DEBUG_ETH	   	 //Activar el debug del ETH
			//#define DEBUG_MEDIDOR  //Activar el debug del medidor
		#endif
	#endif
	#define DEMO_MODE				// Modo para activar valores de medida y estados del cargador ficticios
	#ifdef DEMO_MODE
		//#define DEMO_MODE_METER		//Lectura del medidor ficticia
		//#define DEMO_MODE_CURRENT	//Valor de corriente de carga enviado por un cargador ficticia
		#define DEMO_MODE_BACKEND   //Modo para conectar el equipo al backend de Firebase de producción estando trabajando en DEVELOPMENT
	#endif 
#endif

/*********** Fin configuracion build**************/
#include "Arduino.h"
#include "tipos.h"

#include "cybtldr/cybtldr_api2.h"
#include "cybtldr/cybtldr_command.h"
#include "driver/uart.h"
#include "cybtldr/cybtldr_api.h"

#ifdef IS_UNO_KUBO
	#include "coms/Wifi_Station.h"
	#include "coms/helper_coms.h"
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
#include "config.h"


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
#define PRIORIDAD_BLE      4
#define PRIORIDAD_FIREBASE 3
#define PRIORIDAD_COMS	   2
#define PRIORIDAD_UART	   3
#define PRIORIDAD_CONFIG   5

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
#define ESP_UPDATE    1
#define PSOC_UPDATE    2

/*Direcciones del archivo*/
#define PRIMERA_FILA 14
#define TAMANO_FILA  527

//Estados para la maquina de estados
#define APAGADO          255
#define DISCONNECTED      0
#define DISCONNECTING     2
#define STARTING          1
#define CONNECTING        5
#define CONECTADO        10
#define IDLE             20
#define USER_CONNECTED   21
#define READING_CONTROL  25
#define READING_PARAMS   26
#define READING_COMS     27
#define READING_GROUP    28
#define WRITING_CONTROL  35
#define WRITING_STATUS   36
#define WRITING_PARAMS   37
#define WRITING_COMS     38
#define WRITING_TIMES    39
#define WRITING_RECORD   40
#define DOWNLOADING      55
#define UPDATING         65
#define INSTALLING       70
#define KILLING          75

void updateCharacteristic(uint8_t* data, uint16_t len, uint16_t attrHandle);
void procesar_bloque(uint16 tipo_bloque);
int controlSendToSerialLocal(uint8_t * data, int len);
void UpdateCompressedTask(void *arg);
int controlMain(void);

void MAIN_RESET_Write(uint8_t val);
void controlInit(void);
void UpdateTask(void *arg);
void UpdateESP();
void modifyCharacteristic(uint8* data, uint16 len, uint16 attrHandle);

void Get_Stored_Group_Data();
void Get_Stored_Group_Params();
void Get_Stored_Group_Circuits();

uint8 checkSpiffsBug();

#endif // __CONTROL_MAIN
