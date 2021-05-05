#ifndef __SERVER_BLE_H
#define __SERVER_BLE_H


#include "../control.h"
#include "NimBLEDevice.h"
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include "NimBLE2904.h"

#define NUMBER_OF_SERVICES 		  1

#ifdef CONNECTED
#define NUMBER_OF_CHARACTERISTICS 9
#else
#define NUMBER_OF_CHARACTERISTICS 7
#endif

#define MAX_BLE_FIELDS            (NUMBER_OF_SERVICES+NUMBER_OF_CHARACTERISTICS) 

#define TYPE_SERV		0
#define TYPE_CHAR	1 

enum indexServices
{
	SERV_STATUS = 0,
};

enum indexCharacteristicsAll
{
	// SERV_STATUS
	SELECTOR     = 1,
	OMNIBUS      = 2,
	RCS_HPT_STAT = 3,
	RCS_INS_CURR = 4,
	RCS_RECORD   = 5,
	RCS_SCH_MAT  = 6,
	FW_DATA      = 7,
#ifdef CONNECTED
	RCS_INSB_CURR   = 8,
	RCS_INSC_CURR   = 9,
#endif
};

enum indexCharacteristics
{
	// SERV_STATUS
	BLE_CHA_SELECTOR    = 0,
	BLE_CHA_OMNIBUS     = 1,
	BLE_CHA_HPT_STAT    = 2,
	BLE_CHA_INS_CURR    = 3,
	BLE_CHA_RCS_RECORD  = 4,
	BLE_CHA_RCS_SCH_MAT = 5,
	BLE_CHA_FW_DATA     = 6,
#ifdef CONNECTED
	BLE_CHA_INSB_CURR   = 7,
	BLE_CHA_INSC_CURR   = 8,
#endif
};

typedef struct _BLE_FIELD
{
	uint8_t type;	// service or characteristic 0 or 1
	indexServices indexServ;
	BLEUUID	uuid;
	uint8_t numberOfCharacteristics;
	uint32_t properties;
	uint16_t handle;
	indexCharacteristicsAll indexCharacteristicAll;
	indexCharacteristics indexCharacteristic;
	uint8_t descriptor2902;
}BLE_FIELD;


void serverbleInit(void);
void serverbleStartAdvertising(void);
bool serverbleGetConnected(void);
void serverbleSetConnected(bool value);

void serverbleTask(void *arg);
void serverbleSetCharacteristic(uint8_t * data, int len, uint16_t handle);
void serverbleNotCharacteristic ( uint8_t *data, int len, uint16_t handle );
void changeAdvName( uint8_t * name);

#endif // __SERVER_BLE_H
