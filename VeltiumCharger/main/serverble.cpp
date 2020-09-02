#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "tipos.h"
#include "control.h"
#include "serverble.h"
#include "controlLed.h"
#include <Arduino.h>


/*
#include "esp32-hal-psram.h"

#include <new>
void* operator new(std::size_t sz)
{
	Serial.printf("[before] free heap memory: %7u bytes\n", ESP.getFreeHeap());
	Serial.printf("Overriden NEW operator: requested %u bytes\n", sz);
	Serial.printf("[after ] free heap memory: %7u bytes\n", ESP.getFreeHeap());
	void* ptr = ps_malloc(sz);
	return ptr;
}

void operator delete(void* ptr)
{
	free(ptr);
}
*/

BLEServer *pServer = NULL;
bool deviceBleConnected = false;
bool oldDeviceBleConnected = false;
uint8_t txValue = 0;

TaskHandle_t hdServerble = NULL;
 
uint8 buffer_tx[1024];

BLEService *pbleServices[NUMBER_OF_SERVICES];
BLECharacteristic *pbleCharacteristics[NUMBER_OF_CHARACTERISTICS];

BLE_FIELD blefields[MAX_BLE_FIELDS] =
{
	//SERVICELEDS, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_LUMI_COLOR, BLEUUID((uint16_t)0xCD0B),4 , 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_LUMI_COLOR, BLEUUID((uint16_t)0xCC40),4, BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_READ, LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE, CHA_Luminisity_Level, BLE_CHA_CHA_Luminisity_Level, 0},
	{TYPE_CHARACTERISTIC, SERV_LUMI_COLOR, BLEUUID((uint16_t)0xCC41),4, BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_READ, LED_LUMIN_COLOR_R_LED_COLOUR_CHAR_HANDLE, CHA_R_Led_Color, BLE_CHA_CHA_R_Led_Color, 0},
	{TYPE_CHARACTERISTIC, SERV_LUMI_COLOR, BLEUUID((uint16_t)0xCC42),4, BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_READ, LED_LUMIN_COLOR_G_LED_COLOUR_CHAR_HANDLE, CHA_G_Led_Color, BLE_CHA_CHA_G_Led_Color, 0},
	{TYPE_CHARACTERISTIC, SERV_LUMI_COLOR, BLEUUID((uint16_t)0xCC43),4, BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_READ, LED_LUMIN_COLOR_B_LED_COLOUR_CHAR_HANDLE, CHA_B_Led_Color, BLE_CHA_CHA_B_Led_Color, 0},
	//SERVICESTATUS, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_STATUS, BLEUUID((uint16_t)0xCD01),5, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_STATUS, BLEUUID((uint16_t)0xCCC1),5, BLECharacteristic::PROPERTY_NOTIFY|BLECharacteristic::PROPERTY_READ, STATUS_HPT_STATUS_CHAR_HANDLE, HPT_STATUS, BLE_CHA_HPT_STATUS, 1},
	{TYPE_CHARACTERISTIC, SERV_STATUS, BLEUUID((uint16_t)0xCCC2),5, BLECharacteristic::PROPERTY_READ, STATUS_ICP_STATUS_CHAR_HANDLE, ICP_STATUS, BLE_CHA_ICP_STATUS, 0},
	{TYPE_CHARACTERISTIC, SERV_STATUS, BLEUUID((uint16_t)0xCCC3),5, BLECharacteristic::PROPERTY_READ, STATUS_MCB_STATUS_CHAR_HANDLE, MCB_STATUS, BLE_CHA_MCB_STATUS, 0},
	{TYPE_CHARACTERISTIC, SERV_STATUS, BLEUUID((uint16_t)0xCCC4),5, BLECharacteristic::PROPERTY_READ, STATUS_RCD_STATUS_CHAR_HANDLE, RCD_STATUS, BLE_CHA_RCD_STATUS, 0},
	{TYPE_CHARACTERISTIC, SERV_STATUS, BLEUUID((uint16_t)0xCCC5),5, BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_READ, STATUS_CONN_LOCK_STATUS_CHAR_HANDLE, CONN_LOCK_STATUS, BLE_CHA_CONN_LOCK_STATUS, 0},
	//SERVICEMEASURES, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_MEASURES, BLEUUID((uint16_t)0xCD02),11, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB0),11, BLECharacteristic::PROPERTY_READ, MEASURES_MAX_CURRENT_CABLE_CHAR_HANDLE, MAX_CURRENT_CABLE, BLE_CHA_MAX_CURRENT_CABLE, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB1),11, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE, INSTALATION_CURRENT_LIMIT, BLE_CHA_INSTALATION_CURRENT_LIMIT, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB2),11, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, MEASURES_CURRENT_COMMAND_CHAR_HANDLE, CURRENT_COMMAND, BLE_CHA_CURRENT_COMMAND, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB3),11, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_NOTIFY, MEASURES_INST_CURRENT_CHAR_HANDLE, INST_CURRENT, BLE_CHA_INST_CURRENT, 1},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB4),11, BLECharacteristic::PROPERTY_READ, MEASURES_INST_VOLTAGE_CHAR_HANDLE, INST_VOLTAGE, BLE_CHA_INST_VOLTAGE, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB5),11, BLECharacteristic::PROPERTY_READ, MEASURES_ACTIVE_POWER_CHAR_HANDLE, ACTIVE_POWER, BLE_CHA_ACTIVE_POWER, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB6),11, BLECharacteristic::PROPERTY_READ, MEASURES_REACTIVE_POWER_CHAR_HANDLE, REACTIVE_POWER, BLE_CHA_REACTIVE_POWER, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB7),11, BLECharacteristic::PROPERTY_READ, MEASURES_ACTIVE_ENERGY_CHAR_HANDLE, ACTIVE_ENERGY, BLE_CHA_ACTIVE_ENERGY, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB8),11, BLECharacteristic::PROPERTY_READ, MEASURES_REACTIVE_ENERGY_CHAR_HANDLE, REACTIVE_ENERGY, BLE_CHA_REACTIVE_ENERGY, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCB9),11, BLECharacteristic::PROPERTY_READ, MEASURES_APPARENT_POWER_CHAR_HANDLE, APPARENT_POWER, BLE_CHA_APPARENT_POWER, 0},
	{TYPE_CHARACTERISTIC, SERV_MEASURES, BLEUUID((uint16_t)0xCCBA),11, BLECharacteristic::PROPERTY_READ, MEASURES_POWER_FACTOR_CHAR_HANDLE, POWER_FACTOR, BLE_CHA_POWER_FACTOR, 0},
	//ENERGY_RECORD, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_ENERGY_RECORD, BLEUUID((uint16_t)0xCD03),3, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_ENERGY_RECORD, BLEUUID((uint16_t)0xCCC0),3, BLECharacteristic::PROPERTY_READ, ENERGY_RECORD_RECORD_CHAR_HANDLE, RECORD_RECORD, BLE_CHA_RECORD_RECORD, 0},
	{TYPE_CHARACTERISTIC, SERV_ENERGY_RECORD, BLEUUID((uint16_t)0xCBC1),3, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, ENERGY_RECORD_USER_CHAR_HANDLE, RECORD_USER, BLE_CHA_RECORD_USER, 0},
	{TYPE_CHARACTERISTIC, SERV_ENERGY_RECORD, BLEUUID((uint16_t)0xCBC2),3, BLECharacteristic::PROPERTY_READ, ENERGY_RECORD_SESSION_NUMBER_CHAR_HANDLE, RECORD_SESSION_NUMBER, BLE_CHA_RECORD_SESSION_NUMBER, 0},
	//TIME_DATE, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_TIME_DATE, BLEUUID((uint16_t)0xCD04),6, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_TIME_DATE, BLEUUID((uint16_t)0xCCD0),6, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, TIME_DATE_DATE_TIME_CHAR_HANDLE, DATE_TIME, BLE_CHA_DATE_TIME, 0},
	{TYPE_CHARACTERISTIC, SERV_TIME_DATE, BLEUUID((uint16_t)0xCCD1),6, BLECharacteristic::PROPERTY_READ, TIME_DATE_CONNECTION_DATE_TIME_CHAR_HANDLE, CONNECTION_DATE_TIME, BLE_CHA_CONNECTION_DATE_TIME, 0},
	{TYPE_CHARACTERISTIC, SERV_TIME_DATE, BLEUUID((uint16_t)0xCCD2),6, BLECharacteristic::PROPERTY_READ, TIME_DATE_DISCONNECTION_DATE_TIME_CHAR_HANDLE, DISCONNECTION_DATE_TIME, BLE_CHA_DISCONNECTION_DATE_TIME, 0},
	{TYPE_CHARACTERISTIC, SERV_TIME_DATE, BLEUUID((uint16_t)0xCCD3),6, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, TIME_DATE_DELTA_DELAY_FOR_CHARGING_CHAR_HANDLE, DELTA_DELAY_FOR_CHARGING, BLE_CHA_DELTA_DELAY_FOR_CHARGING, 0},
	{TYPE_CHARACTERISTIC, SERV_TIME_DATE, BLEUUID((uint16_t)0xCCD4),6, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE, CHARGING_START_TIME, BLE_CHA_CHARGING_START_TIME, 0},
	{TYPE_CHARACTERISTIC, SERV_TIME_DATE, BLEUUID((uint16_t)0xCCD5),6, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE, CHARGING_STOP_TIME, BLE_CHA_CHARGING_STOP_TIME, 0},
	//CHARGING, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_CHARGING, BLEUUID((uint16_t)0xCD05),4, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_CHARGING, BLEUUID((uint16_t)0xCCE0),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, CHARGING_INSTANT_DELAYED_CHAR_HANDLE, INSTANT_DELAYED, BLE_CHA_INSTANT_DELAYED, 0},
	{TYPE_CHARACTERISTIC, SERV_CHARGING, BLEUUID((uint16_t)0xCCE1),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, CHARGING_START_STOP_START_MODE_CHAR_HANDLE, START_STOP_START_MODE, BLE_CHA_START_STOP_START_MODE, 0},
	{TYPE_CHARACTERISTIC, SERV_CHARGING, BLEUUID((uint16_t)0xCCE2),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, CHARGING_BLE_MANUAL_START_CHAR_HANDLE, BLE_MANUAL_START, BLE_CHA_BLE_MANUAL_START, 0},
	{TYPE_CHARACTERISTIC, SERV_CHARGING, BLEUUID((uint16_t)0xCCE3),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE, BLE_MANUAL_STOP, BLE_CHA_BLE_MANUAL_STOP, 0},
	//SCHED_CHARGING, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_SCHED_CHARGING, BLEUUID((uint16_t)0xCD06),1, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_SCHED_CHARGING, BLEUUID((uint16_t)0xCCF0),1, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE, SCHEDULE_MATRIX, BLE_CHA_SCHEDULE_MATRIX, 0},
	//VCD_NAME_USERS, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_VCD_NAME_USERS, BLEUUID((uint16_t)0xCD07),4, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_VCD_NAME_USERS, BLEUUID((uint16_t)0xCC00),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_HANDLE, CHARGER_DEVICE_ID, BLE_CHA_CHARGER_DEVICE_ID, 0},
	{TYPE_CHARACTERISTIC, SERV_VCD_NAME_USERS, BLEUUID((uint16_t)0xCC01),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE, USERS_NUMBER, BLE_CHA_USERS_NUMBER, 0},
	{TYPE_CHARACTERISTIC, SERV_VCD_NAME_USERS, BLEUUID((uint16_t)0xCC02),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, VCD_NAME_USERS_UI_X_USER_ID_CHAR_HANDLE, UI_X_USER_ID, BLE_CHA_UI_X_USER_ID, 0},
	{TYPE_CHARACTERISTIC, SERV_VCD_NAME_USERS, BLEUUID((uint16_t)0xCC03),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE, USER_INDEX, BLE_CHA_USER_INDEX, 0},
	//TEST, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_TEST, BLEUUID((uint16_t)0xCD08),4, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_TEST, BLEUUID((uint16_t)0xCC10),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, TEST_LAUNCH_RCD_PE_TEST_CHAR_HANDLE, LAUNCH_RCD_PE_TEST, BLE_CHA_LAUNCH_RCD_PE_TEST, 0},
	{TYPE_CHARACTERISTIC, SERV_TEST, BLEUUID((uint16_t)0xCC11),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE, RCD_PE_TEST_RESULT, BLE_CHA_RCD_PE_TEST_RESULT, 0},
	{TYPE_CHARACTERISTIC, SERV_TEST, BLEUUID((uint16_t)0xCC12),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, TEST_LAUNCH_MCB_TEST_CHAR_HANDLE, LAUNCH_MCB_TEST, BLE_CHA_LAUNCH_MCB_TEST, 0},
	{TYPE_CHARACTERISTIC, SERV_TEST, BLEUUID((uint16_t)0xCC13),4, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, TEST_MCB_TEST_RESULT_CHAR_HANDLE, MCB_TEST_RESULT, BLE_CHA_MCB_TEST_RESULT, 0},
	//MAN_LOCK_UNLOCK, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_MAN_LOCK_UNLOCK, BLEUUID((uint16_t)0xCD09),1, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_MAN_LOCK_UNLOCK, BLEUUID((uint16_t)0xCC20),1, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_CHAR_HANDLE, LOCKING_MACHANISM_ON_OFF, BLE_CHA_LOCKING_MACHANISM_ON_OFF, 0},
	//BOOT_LOADER, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_BOOT_LOADER, BLEUUID((uint16_t)0xCD0A),3, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_BOOT_LOADER, BLEUUID((uint16_t)0xCC30),3, BLECharacteristic::PROPERTY_READ, BOOT_LOADER_PROTECTION_CODE_CHAR_HANDLE, PROTECTION_CODE, BLE_CHA_PROTECTION_CODE, 0},
	{TYPE_CHARACTERISTIC, SERV_BOOT_LOADER, BLEUUID((uint16_t)0xCC31),3, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, BOOT_LOADER_LOAD_SW_APP_CHAR_HANDLE, LOAD_SW_APP, BLE_CHA_LOAD_SW_APP, 0},
	{TYPE_CHARACTERISTIC, SERV_BOOT_LOADER, BLEUUID((uint16_t)0xCC32),3, BLECharacteristic::PROPERTY_NOTIFY|BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_WRITE_NR, BOOT_LOADER_BINARY_BLOCK_CHAR_HANDLE, BINARY_BLOCK, BLE_CHA_BINARY_BLOCK, 1},
	//RESET, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_RESET, BLEUUID((uint16_t)0xCD0C),1, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_RESET, BLEUUID((uint16_t)0xCC50),1, BLECharacteristic::PROPERTY_WRITE, RESET_RESET_CHAR_HANDLE, RESET, BLE_CHA_RESET, 0},
	//CLAVE, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_CLAVE, BLEUUID((uint16_t)0xCD0D),1, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_CLAVE, BLEUUID((uint16_t)0xCC60),1, BLECharacteristic::PROPERTY_WRITE, CLAVE_WRITE_PASS_CHAR_HANDLE, WRITE_PASS, BLE_CHA_WRITE_PASS, 0},
	//RECORDING, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_RECORDING, BLEUUID((uint16_t)0xCD0E),4, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_RECORDING, BLEUUID((uint16_t)0xCCA0),4, BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_READ, RECORDING_REC_CAPACITY_CHAR_HANDLE, REC_CAPACITY, BLE_CHA_REC_CAPACITY, 0},
	{TYPE_CHARACTERISTIC, SERV_RECORDING, BLEUUID((uint16_t)0xCCA1),4, BLECharacteristic::PROPERTY_READ, RECORDING_REC_LAST_CHAR_HANDLE, REC_LAST, BLE_CHA_REC_LAST, 0},
	{TYPE_CHARACTERISTIC, SERV_RECORDING, BLEUUID((uint16_t)0xCCA2),4, BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_READ, RECORDING_REC_INDEX_CHAR_HANDLE, REC_INDEX, BLE_CHA_REC_INDEX, 0},
	{TYPE_CHARACTERISTIC, SERV_RECORDING, BLEUUID((uint16_t)0xCCA3),4, BLECharacteristic::PROPERTY_READ, RECORDING_REC_LAPS_CHAR_HANDLE, REC_LAPS, BLE_CHA_REC_LAPS, 0},
	//VERSIONES, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_VERSIONES, BLEUUID((uint16_t)0xCD0F),2, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_VERSIONES, BLEUUID((uint16_t)0xCC70),2, BLECharacteristic::PROPERTY_READ, VERSIONES_VERSION_FIRMWARE_CHAR_HANDLE, VERSION_FIRMWARE, BLE_CHA_VERSION_FIRMWARE, 0},
	{TYPE_CHARACTERISTIC, SERV_VERSIONES, BLEUUID((uint16_t)0xCC71),2, BLECharacteristic::PROPERTY_READ, VERSIONES_VERSION_FIRM_BLE_CHAR_HANDLE, VERSION_FIRM_BLE, BLE_CHA_VERSION_FIRM_BLE, 0},
	//CONFIGURACION, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_CONFIGURACION, BLEUUID((uint16_t)0xCD00),1, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_CONFIGURACION, BLEUUID((uint16_t)0xCC80),1, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE, AUTENTICATION_MODES, BLE_CHA_AUTENTICATION_MODES, 0},
	//AUTENTICACION, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_AUTENTICACION, BLEUUID((uint16_t)0xCD10),3, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_AUTENTICACION, BLEUUID((uint16_t)0xCC90),3, BLECharacteristic::PROPERTY_WRITE, AUTENTICACION_SERIAL_NUMBER_CHAR_HANDLE, SERIAL_NUMBER, BLE_CHA_SERIAL_NUMBER, 0},
	{TYPE_CHARACTERISTIC, SERV_AUTENTICACION, BLEUUID((uint16_t)0xCC91),3, BLECharacteristic::PROPERTY_READ, AUTENTICACION_MATRIX_CHAR_HANDLE, MATRIX, BLE_CHA_MATRIX, 0},
	{TYPE_CHARACTERISTIC, SERV_AUTENTICACION, BLEUUID((uint16_t)0xCC92),3, BLECharacteristic::PROPERTY_WRITE, AUTENTICACION_TOKEN_CHAR_HANDLE, TOKEN, BLE_CHA_TOKEN, 0},
	//DOMESTIC_CONSUMPTION, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_DOMESTIC_CONSUMPTION, BLEUUID((uint16_t)0xCD20),6, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_DOMESTIC_CONSUMPTION, BLEUUID((uint16_t)0xCE00),6, BLECharacteristic::PROPERTY_READ, DOMESTIC_CONSUMPTION_REAL_CURRENT_LIMIT_CHAR_HANDLE, REAL_CURRENT_LIMIT, BLE_CHA_REAL_CURRENT_LIMIT, 0},
	{TYPE_CHARACTERISTIC, SERV_DOMESTIC_CONSUMPTION, BLEUUID((uint16_t)0xCE01),6, BLECharacteristic::PROPERTY_READ, DOMESTIC_CONSUMPTION_DOMESTIC_CURRENT_CHAR_HANDLE, DOMESTIC_CURRENT, BLE_CHA_DOMESTIC_CURRENT, 0},
	{TYPE_CHARACTERISTIC, SERV_DOMESTIC_CONSUMPTION, BLEUUID((uint16_t)0xCE02),6, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE, KS, BLE_CHA_KS, 0},
	{TYPE_CHARACTERISTIC, SERV_DOMESTIC_CONSUMPTION, BLEUUID((uint16_t)0xCE03),6, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE, FCT, BLE_CHA_FCT, 0},
	{TYPE_CHARACTERISTIC, SERV_DOMESTIC_CONSUMPTION, BLEUUID((uint16_t)0xCE04),6, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE, FS, BLE_CHA_FS, 0},
	{TYPE_CHARACTERISTIC, SERV_DOMESTIC_CONSUMPTION, BLEUUID((uint16_t)0xCE05),6, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE, POTENCIA_CONTRATADA, BLE_CHA_POTENCIA_CONTRATADA, 0},
	//ERROR_STATUS, , , , , , BLE_CHA_, 
	{TYPE_SERVICE, SERV_ERROR_STATUS, BLEUUID((uint16_t)0xCD30),1, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHARACTERISTIC, SERV_ERROR_STATUS, BLEUUID((uint16_t)0xCCF0),1, BLECharacteristic::PROPERTY_READ, ERROR_STATUS_ERROR_CODE_CHAR_HANDLE, ERROR_STATUS, BLE_CHA_ERROR_STATUS, 0}

};

void serverblekk( uint8_t *data, int len, uint16_t handle )
{
	int i = 0;
	for ( i = 0 ;  i < NUMBER_OF_CHARACTERISTICS; i++ )
	{
		if (  blefields[i].handle == handle ) 
		{
			pbleCharacteristics[blefields[i].indexCharacteristic]->setValue(data,len);
			pbleCharacteristics[blefields[i].indexCharacteristic]->notify();
			return;
		}
	}
	return;
}



void serverbleNotCharacteristic ( uint8_t *data, int len, uint16_t handle )
{
	int i = 0;
	for ( i = 0 ;  i < NUMBER_OF_CHARACTERISTICS; i++ )
	{
		if (  blefields[i].handle == handle ) 
		{
			pbleCharacteristics[blefields[i].indexCharacteristic]->setValue(data,len);
			pbleCharacteristics[blefields[i].indexCharacteristic]->notify();
			return;
		}
	}
	return;
}

void serverbleSetCharacteristic ( uint8_t *data, int len, uint16_t handle )
{
	int i = 0;
	for ( i = 0 ;  i < NUMBER_OF_CHARACTERISTICS; i++ )
	{
		if (  blefields[i].handle == handle ) 
		{
			pbleCharacteristics[blefields[i].indexCharacteristic]->setValue(data,len);
			return;
		}
	}
	return;
}

bool serverbleGetConnected(void)
{
	return deviceBleConnected;
}

class serverCallbacks: public BLEServerCallbacks 
{
	void onConnect(BLEServer* pServer) 
	{
		deviceBleConnected = true;
		deviceConnectInd();
	};

	void onDisconnect(BLEServer* pServer) 
	{
		deviceBleConnected = false;
		deviceDisconnectInd();
	}
};


class CBCharacteristic: public BLECharacteristicCallbacks 
{
	void onWrite(BLECharacteristic *pCharacteristic) 
	{
		std::string rxValue = pCharacteristic->getValue();

		if ( pCharacteristic->getUUID().equals(blefields[TOKEN].uuid) )
		{
			setAutentificationToken((uint8_t*)rxValue.c_str(),rxValue.length());
			return;
		}

		if ( pCharacteristic->getUUID().equals(blefields[BINARY_BLOCK].uuid) )
		{
			sendBinaryBlock((uint8_t*)rxValue.c_str(),rxValue.length());
			return;
		}

		if ( hasAutentification() )
		{
			if ( pCharacteristic->getUUID().equals(blefields[CHA_Luminisity_Level].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CHA_Luminisity_Level].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CHA_Luminisity_Level].handle);
				buffer_tx[3] = 1;
				buffer_tx[4] = rxValue[0];
				controlSendToSerialLocal(buffer_tx,5);
				//pCharacteristic->setValue((uint8_t*)&rxValue[0],1);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[CHA_R_Led_Color].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CHA_R_Led_Color].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CHA_R_Led_Color].handle);
				buffer_tx[3] = 1;
				buffer_tx[4] = rxValue[0];
				controlSendToSerialLocal(buffer_tx,5);
				//pCharacteristic->setValue((uint8_t*)&rxValue[0],1);
				return;
			}	
			if ( pCharacteristic->getUUID().equals(blefields[CHA_G_Led_Color].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CHA_G_Led_Color].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CHA_G_Led_Color].handle);
				buffer_tx[3] = 1;
				buffer_tx[4] = rxValue[0];
				controlSendToSerialLocal(buffer_tx,5);
				//pCharacteristic->setValue((uint8_t*)&rxValue[0],1);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[CHA_B_Led_Color].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CHA_B_Led_Color].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CHA_B_Led_Color].handle);
				buffer_tx[3] = 1;
				buffer_tx[4] = rxValue[0];
				controlSendToSerialLocal(buffer_tx,5);
				//pCharacteristic->setValue((uint8_t*)&rxValue[0],1);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[AUTENTICATION_MODES].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CHA_B_Led_Color].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CHA_B_Led_Color].handle);
				buffer_tx[3] = 2;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 2);
				controlSendToSerialLocal(buffer_tx,6);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[CONN_LOCK_STATUS].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CONN_LOCK_STATUS].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CONN_LOCK_STATUS].handle);
				buffer_tx[3] = 2;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 2);
				controlSendToSerialLocal(buffer_tx,6);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[INSTALATION_CURRENT_LIMIT].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[INSTALATION_CURRENT_LIMIT].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[INSTALATION_CURRENT_LIMIT].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[CURRENT_COMMAND].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CURRENT_COMMAND].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CURRENT_COMMAND].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[DATE_TIME].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[DATE_TIME].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[DATE_TIME].handle);
				buffer_tx[3] = 6;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 6);
				controlSendToSerialLocal(buffer_tx,10);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[DELTA_DELAY_FOR_CHARGING].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[DELTA_DELAY_FOR_CHARGING].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[DELTA_DELAY_FOR_CHARGING].handle);
				buffer_tx[3] = 2;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 2);
				controlSendToSerialLocal(buffer_tx,6);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[CHARGING_START_TIME].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CHARGING_START_TIME].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CHARGING_START_TIME].handle);
				buffer_tx[3] = 6;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 6);
				controlSendToSerialLocal(buffer_tx,10);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[CHARGING_STOP_TIME].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CHARGING_STOP_TIME].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CHARGING_STOP_TIME].handle);
				buffer_tx[3] = 6;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 6);
				controlSendToSerialLocal(buffer_tx,10);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[INSTANT_DELAYED].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[INSTANT_DELAYED].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[INSTANT_DELAYED].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[START_STOP_START_MODE].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[START_STOP_START_MODE].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[START_STOP_START_MODE].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[BLE_MANUAL_START].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[BLE_MANUAL_START].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[BLE_MANUAL_START].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[BLE_MANUAL_STOP].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[BLE_MANUAL_STOP].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[BLE_MANUAL_STOP].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[SCHEDULE_MATRIX].uuid) )
			{
				if( getModeTelecarga() == 0)
				{
					buffer_tx[0] = HEADER_TX;
					buffer_tx[1] = (uint8)(blefields[SCHEDULE_MATRIX].handle >> 8);
					buffer_tx[2] = (uint8)(blefields[SCHEDULE_MATRIX].handle);
					buffer_tx[3] = 168;
					memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 168);
					controlSendToSerialLocal(buffer_tx,172);
				}
				else
				{
					memcpy(&buffer_tx[0], (uint8_t*)&rxValue[0], 144);
					controlSendToSerialLocal(buffer_tx,144);
				}
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[CHARGER_DEVICE_ID].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[CHARGER_DEVICE_ID].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[CHARGER_DEVICE_ID].handle);
				buffer_tx[3] = 11;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 11);
				controlSendToSerialLocal(buffer_tx,15);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[USERS_NUMBER].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[USERS_NUMBER].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[USERS_NUMBER].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[UI_X_USER_ID].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[UI_X_USER_ID].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[UI_X_USER_ID].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[USER_INDEX].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[USER_INDEX].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[USER_INDEX].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[LAUNCH_RCD_PE_TEST].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[LAUNCH_RCD_PE_TEST].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[LAUNCH_RCD_PE_TEST].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[RCD_PE_TEST_RESULT].uuid) )
			{
				Update_VELT1_CHARGER_characteristic((uint8_t*)&rxValue[0], 1, TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[LAUNCH_MCB_TEST].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[LAUNCH_MCB_TEST].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[LAUNCH_MCB_TEST].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[MCB_TEST_RESULT].uuid) )
			{
				Update_VELT1_CHARGER_characteristic((uint8_t*)&rxValue[0], 1, TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[LOCKING_MACHANISM_ON_OFF].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[LOCKING_MACHANISM_ON_OFF].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[LOCKING_MACHANISM_ON_OFF].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[PROTECTION_CODE].uuid) )
			{
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[LOAD_SW_APP].uuid) )
			{
				if(rxValue[0] == 0xFF)
				{
					vTaskDelay(200 / portTICK_PERIOD_MS);	
					// Enter Bootloader LOCAL 
					//Bootloadable_1_Load();
					printf("Now I execute new FW - Not developed this jump\r\n");
				}
				else if(rxValue[0] == 0xAA)
				{
					// Micro Principal Bootloading 
					buffer_tx[0] = HEADER_TX;
					buffer_tx[1] = (uint8)(LOAD_SW_APP >> 8);
					buffer_tx[2] = (uint8)(LOAD_SW_APP);
					buffer_tx[3] = 1;
					memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
					controlSendToSerialLocal(buffer_tx,5);
					vTaskDelay(500 / portTICK_PERIOD_MS);	
					LED_Control(100, 10, 10, 10);
					setModeTelecarga(1);
				}
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[BINARY_BLOCK].uuid) )
			{
				if ( getModeTelecarga() )
				{
					memcpy(&buffer_tx[0], (uint8_t*)&rxValue[0],rxValue.length());
					controlSendToSerialLocal(buffer_tx,rxValue.length());
				}
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[RESET].uuid) )
			{
				if( getModeTelecarga() )
				{
					vTaskDelay(200 / portTICK_PERIOD_MS);	
					MAIN_RESET_Write(0);
					vTaskDelay(100 / portTICK_PERIOD_MS);	
					esp_restart();
				}
				else if(rxValue[0] == 0xAA)
				{
					// Micro Principal Bootloading 
					buffer_tx[0] = HEADER_TX;
					buffer_tx[1] = (uint8)(RESET >> 8);
					buffer_tx[2] = (uint8)(RESET);
					buffer_tx[3] = 1;
					memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
					controlSendToSerialLocal(buffer_tx,5);
				}
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[WRITE_PASS].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[WRITE_PASS].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[WRITE_PASS].handle);
				buffer_tx[3] = 6;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 6);
				controlSendToSerialLocal(buffer_tx,10);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[REC_CAPACITY].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[REC_CAPACITY].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[REC_CAPACITY].handle);
				buffer_tx[3] = 2;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 2);
				controlSendToSerialLocal(buffer_tx,6);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[REC_INDEX].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[REC_INDEX].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[REC_INDEX].handle);
				buffer_tx[3] = 2;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 2);
				controlSendToSerialLocal(buffer_tx,6);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[SERIAL_NUMBER].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[SERIAL_NUMBER].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[SERIAL_NUMBER].handle);
				buffer_tx[3] = 10;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 10);
				controlSendToSerialLocal(buffer_tx,14);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[KS].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[KS].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[KS].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[FCT].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[FCT].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[FCT].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[FCT].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[FCT].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[FCT].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[FS].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[FS].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[FS].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}
			if ( pCharacteristic->getUUID().equals(blefields[POTENCIA_CONTRATADA].uuid) )
			{
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(blefields[POTENCIA_CONTRATADA].handle >> 8);
				buffer_tx[2] = (uint8)(blefields[POTENCIA_CONTRATADA].handle);
				buffer_tx[3] = 1;
				memcpy(&buffer_tx[4], (uint8_t*)&rxValue[0], 1);
				controlSendToSerialLocal(buffer_tx,5);
				return;
			}

		}

	}
};


CBCharacteristic pbleCharacteristicCallbacks;

BLEAdvertisementData advert;
BLEAdvertising *pAdvertising;

void changeAdvName( uint8_t * name )
{
	printf("changing name at adv\r\n");
	pAdvertising = pServer->getAdvertising();
	pAdvertising->stop();
	advert.setName(std::string((char*)name));
	pAdvertising->setAdvertisementData(advert);
	pAdvertising->start();

	return;
}


void serverbleInit() {

	// Create the BLE Device
	BLEDevice::init("VCD1701XXXX");

	// Create the BLE Server
	pServer = BLEDevice::createServer();
	pServer->setCallbacks(new serverCallbacks());

	int indexService = 0;
	int indexCharacteristic = 0;
	int i;
	for ( i = 0; i < MAX_BLE_FIELDS; i++ )
	{
		if ( blefields[i].type == TYPE_SERVICE )
		{
			pbleServices[indexService] = pServer->createService(blefields[i].uuid,blefields[i].numberOfCharacteristics*3 );	
			indexService++;
		}
		else if ( blefields[i].type == TYPE_CHARACTERISTIC )
		{
			Serial.println("Free Heap Memory [before characteristic] **************************");
			Serial.println(ESP.getFreeHeap());


			pbleCharacteristics[indexCharacteristic] = pbleServices[blefields[i].indexServ]->createCharacteristic( blefields[i].uuid, blefields[i].properties );
			if ( blefields[i].descriptor2902 == 1 )
			{
				pbleCharacteristics[indexCharacteristic]->addDescriptor(new BLE2902());
			}
			pbleCharacteristics[indexCharacteristic]->setCallbacks(&pbleCharacteristicCallbacks);
			indexCharacteristic++;
		}
	}

	// Start the service
	for ( i = 0; i < NUMBER_OF_SERVICES; i++ )
	{
		pbleServices[i]->start();
	}

	// Start advertising
	pServer->getAdvertising()->addServiceUUID(BLEUUID((uint16_t)0xCD03));
	pServer->getAdvertising()->addServiceUUID(BLEUUID((uint16_t)0xCD04));
	pServer->getAdvertising()->addServiceUUID(BLEUUID((uint16_t)0xCD05));
	pServer->getAdvertising()->addServiceUUID(BLEUUID((uint16_t)0xCD01));
	pServer->getAdvertising()->addServiceUUID(BLEUUID((uint16_t)0xCD02));
	pServer->getAdvertising()->start();
	printf("Waiting a client connection to notify...\r\n");

	xTaskCreate(	
			serverbleTask,
			"TASK BLE",
			4096,//4096*4,
			NULL,
			1,
			NULL	
		   );

}

void serverbleTask(void *arg)
{
	while (1)
	{
		if (deviceBleConnected) {
			printf("connected----\r\n");
		}

		// disconnecting
		if (!deviceBleConnected && oldDeviceBleConnected) {
			pServer->startAdvertising(); // restart advertising
			printf(" disconnecting \r\n");
			printf("start advertising again\r\n");
			oldDeviceBleConnected = deviceBleConnected;
		}
		// connecting
		if (deviceBleConnected && !oldDeviceBleConnected) {
			printf(" connecting \r\n");
			// do stuff here on connecting
			oldDeviceBleConnected = deviceBleConnected;
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);	
	}
}
