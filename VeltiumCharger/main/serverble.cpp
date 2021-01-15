#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "tipos.h"
#include "control.h"
#include "serverble.h"
#include "controlLed.h"
#ifdef USE_WIFI
#include "FirebaseClient.h"
#endif
#include <Arduino.h>

#include "ble_rcs.h"
#include "ble_rcs_server.h"
#include "esp32-hal-psram.h"

extern TaskHandle_t xHandle;

/* milestone: one-liner for reporting memory usage */
void milestone(const char* mname)
{
	static uint32_t ms_prev = 0;
	int32_t delta = 0;
	uint32_t ms_curr = ESP.getFreeHeap();
	if (ms_prev > 0)
		delta = (int32_t)ms_prev - (int32_t)ms_curr;
	ms_prev = ms_curr;

	Serial.printf ("**MILESTONE** [%6u] [%+7d] [%s]\n", ms_curr, delta, mname);
}

//BLEServer *pServer = NULL;
BLEServer *pServer = (BLEServer*) ps_malloc(sizeof(BLEServer));
bool deviceBleConnected = false;
bool oldDeviceBleConnected = false;

uint8_t txValue = 0;

TaskHandle_t hdServerble = NULL;

// buffer for sending data downstream (to main CPU).
// must be large enough to hold maximum writeable characteristic (288 bytes)
// plus 4 header bytes, so 300 bytes should be enough.
// actually
//uint8 *buffer_tx=(uint8 *)ps_calloc(300,sizeof(uint8)); EXT_RAM_ATTR
uint8 buffer_tx[300];// EXT_RAM_ATTR;


BLEService *pbleServices[NUMBER_OF_SERVICES];

BLECharacteristic *pbleCharacteristics[NUMBER_OF_CHARACTERISTICS];

#define PROP_RW BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE
#define PROP_RN BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_NOTIFY
#define PROP_R  BLECharacteristic::PROPERTY_READ
#define PROP_WN BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_NOTIFY

// VSC_SELECTOR     RW 16
// VSC_OMNIBUS      RW 16
// VSC_OMNINOT      RN 16
// VSC_RECORD       R  512
// VSC_SCHED_MATRIX RW 168
// VSC_FW_COMMAND   WN 288

BLE_FIELD blefields[MAX_BLE_FIELDS] =
{
	{TYPE_SERV, SERV_STATUS, BLEUUID((uint16_t)0xCD01),6, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC001),6, PROP_RW, 0, SELECTOR,     BLE_CHA_SELECTOR,    0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC002),6, PROP_RW, 0, OMNIBUS,      BLE_CHA_OMNIBUS,     0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC003),6, PROP_RN, 0, RCS_HPT_STAT, BLE_CHA_HPT_STAT,    1},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC004),6, PROP_RN, 0, RCS_INS_CURR, BLE_CHA_INS_CURR,    1},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC005),6, PROP_R,  0, RCS_RECORD,   BLE_CHA_RCS_RECORD,  0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC006),6, PROP_RW, 0, RCS_SCH_MAT,  BLE_CHA_RCS_SCH_MAT, 0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC007),6, PROP_WN, 0, FW_DATA,      BLE_CHA_FW_DATA,     1}
};


void serverbleNotCharacteristic ( uint8_t *data, int len, uint16_t handle )
{
	// check non-selected handles first
	if (handle == STATUS_HPT_STATUS_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_HPT_STAT]->setValue(data, len);
		pbleCharacteristics[BLE_CHA_HPT_STAT]->notify();
		return;		
	}
	if (handle == MEASURES_INST_CURRENT_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_INS_CURR]->setValue(data, len);
		pbleCharacteristics[BLE_CHA_INS_CURR]->notify();
		return;
	}
	if (handle == FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_FW_DATA]->setValue(data, len);
		pbleCharacteristics[BLE_CHA_FW_DATA]->notify();
		return;
	}
}

void serverbleSetCharacteristic ( uint8_t *data, int len, uint16_t handle )
{
	// check non-selected handles first
	if (handle == STATUS_HPT_STATUS_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_HPT_STAT]->setValue(data, len);
		return;		
	}
	if (handle == MEASURES_INST_CURRENT_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_INS_CURR]->setValue(data, len);
		return;
	}
	if (handle == ENERGY_RECORD_RECORD_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_RCS_RECORD]->setValue(data, len);
		return;
	}
	if (handle == SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_RCS_SCH_MAT]->setValue(data, len);
		return;
	}
	// set characteristic value using selector mechanism
	rcs_server_set_chr_value(handle, data, len);
}

bool serverbleGetConnected(void)
{
	return deviceBleConnected;
}

class serverCallbacks: public BLEServerCallbacks 
{
	void onConnect(BLEServer* pServer) 
	{
		#ifdef USE_WIFI
			pauseFirebaseClient();
		#endif
		deviceBleConnected = true;
		deviceConnectInd();
		
	};

	void onDisconnect(BLEServer* pServer) 
	{
		#ifdef USE_WIFI
			resumeFirebaseClient();
		#endif
		deviceBleConnected = false;
		deviceDisconnectInd();
	}
};

class CBCharacteristic: public BLECharacteristicCallbacks 
{
	void onWrite(BLECharacteristic *pCharacteristic) 
	{
		std::string rxValue = pCharacteristic->getValue();

		Serial.printf("onWrite: char = %s\n", pCharacteristic->getUUID().toString().c_str());

		static uint8_t omnibus_packet_buffer[4 + RCS_CHR_OMNIBUS_SIZE];

		uint8_t* data = (uint8_t*)&rxValue[0];
		uint16_t dlen = (uint16_t)rxValue.length();

		if (pCharacteristic->getUUID().equals(blefields[SELECTOR].uuid))
		{
			// safety check: at least one byte for selector
			if (dlen < 1) {
				Serial.println("Error at write callback for Selector CHR: size less than 1");
				uint8_t empty_packet[4] = {0, 0, 0, 0};
				pbleCharacteristics[BLE_CHA_OMNIBUS]->setValue(empty_packet, 4);
				return;
			}
			// characteristic selector https://open.spotify.com/track/04hWYuhqETLXrUy7S8Rxzp?si=iMGb3JzTRna3ikopCm89Pw
			uint8_t selector = data[0];

			// payload to be write to omnibus characteristic
			uint8_t* payload = rcs_server_get_data_for_selector(selector);
			// size of payload to be written to omnibus characteristic
			uint8_t  pldsize = rcs_get_size(selector);

			Serial.printf("Received write request for selector %u\n", selector);

			// prepare packet to be written to characteristic:
			// header with 2 bytes selector little endian, 2 bytes payload size little endian
			// afterwards, the payload with the actual data
			uint8_t* packet = omnibus_packet_buffer;
			uint8_t pktsize = 4 + pldsize;
			packet[0] = selector;
			packet[1] = 0;
			packet[2] = pldsize;
			packet[3] = 0;

			// this flag will be set to 1 if we are not allowed to read (not authenticated)
			uint8_t force_read_dummy_data = 0;

			// check authentication
			if (!authorizedOK())
			{
				// no authentication, only allowed operation is matrix read
				uint16_t handle = rcs_handle_for_idx(selector);
				if (handle != AUTENTICACION_MATRIX_CHAR_HANDLE) {
					Serial.println("BAD AUTHENTICATION TOKEN. Will return dummy data when read");
					force_read_dummy_data = 1;
				}
			}

			if (!force_read_dummy_data) {
				// authorized, copy payload from pseudo-characteristics buffer
				memcpy((packet + 4), payload, pldsize);
			}
			else {
				// non authorized, copy zeros
				memset((packet + 4), 0, pldsize);
			}


			// set characteristic value to be read by other end
			pbleCharacteristics[BLE_CHA_OMNIBUS]->setValue(packet, pktsize);

			return;
		}

		if (pCharacteristic->getUUID().equals(blefields[OMNIBUS].uuid))
		{
			// when omnibus is used for writing, it should be prefixed with:
			// 2 bytes for selector
			// 2 bytes for size prefix (ignored)
			// as it should have a minimum payload of 1,
			// its size should never be less than 5
			if (dlen < 5) {
				Serial.println("Error at write callback for Omnibus CHR: size less than 5");
				return;
			}

			uint8_t selector = data[0];
			uint16_t handle = rcs_handle_for_idx(selector);
			uint8_t  size = rcs_get_size(selector);

			uint8_t* payload = data + 4;

			Serial.printf("Received omnibus write request for selector %u\n", selector);
			Serial.printf("Received omnibus write request for handle   %u\n", handle);
			

			// special cases
			if (handle == AUTENTICACION_TOKEN_CHAR_HANDLE) {
				setAuthToken(payload, size);
				return;
			}
			if (handle == RESET_RESET_CHAR_HANDLE) {
				if( isMainFwUpdateActive() )
				{
					vTaskDelay(200 / portTICK_PERIOD_MS);	
					MAIN_RESET_Write(0);
					vTaskDelay(100 / portTICK_PERIOD_MS);	
					esp_restart();
					return;
				}
			}
			if (handle == TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE) {
				// mechanics: write 0 to result, write launch, wait, read result
				updateCharacteristic((uint8_t*)&rxValue[0], 1, TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE);
				return;
			}
			if (handle == TEST_MCB_TEST_RESULT_CHAR_INDEX) {
				updateCharacteristic((uint8_t*)&rxValue[0], 1, TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE);
				return;
			}
			if (handle == FWUPDATE_BIRD_PROLOG_PSEUDO_CHAR_HANDLE) {
				// prolog
				char signature[11];
				memcpy(signature, &payload[0], 10);
				signature[10] = '\0';

				uint32_t fileSize;
				memcpy(&fileSize, &payload[10], sizeof(fileSize));

				uint16_t partCount;
				memcpy(&partCount, &payload[14], sizeof(partCount));

				Serial.printf("Received FwUpdate Prolog\n");
				//for (int i = 0; i < size; i++) Serial.printf("%02X ", payload[i]);
				//Serial.printf("\n");
				Serial.printf("Firmware file with signature %s has %u bytes in %u parts\n", signature, fileSize, partCount);

				// notify success (0x00000000)
				uint32_t successCode = 0x00000000;
				serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);

				return;
			}
			if (handle == FWUPDATE_BIRD_EPILOG_PSEUDO_CHAR_HANDLE) {
				// epilog
				Serial.printf("Received FwUpdate Epilog\n");
				//for (int i = 0; i < size; i++) Serial.printf("%02X ", payload[i]);
				//Serial.printf("\n");

				uint32_t fullCRC32;
				memcpy(&fullCRC32, &payload[0], 4);

				Serial.printf("Firmware file has global crc32 %08X\n", fullCRC32);

				// notify success (0x00000000)
				uint32_t successCode = 0x00000000;
				serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);

				return;
			}

			// check if we are authenticated, and leave if we aren't
			if (!authorizedOK()) {
				return;
			}

			
			// if we are here, we are authenticated.
			// send payload downstream.
			buffer_tx[0] = HEADER_TX;
			buffer_tx[1] = (uint8)(handle >> 8);
			buffer_tx[2] = (uint8)(handle);
			buffer_tx[3] = size;
			memcpy(&buffer_tx[4], payload, size);
			controlSendToSerialLocal(buffer_tx, size + 4);
			return;
		}

		if ( pCharacteristic->getUUID().equals(blefields[RCS_SCH_MAT].uuid) )
		{
			uint16_t size = 168;	// for schedule matrix
			
			buffer_tx[0] = HEADER_TX;
			buffer_tx[1] = (uint8)(SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE >> 8);
			buffer_tx[2] = (uint8)(SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);
			buffer_tx[3] = size;
			memcpy(&buffer_tx[4], data, size);
			controlSendToSerialLocal(buffer_tx, size + 4);
			return;
		}
Serial.printf("blefields[FW_DATA].uuid: %s\n", blefields[FW_DATA].uuid.toString().c_str());
		if ( pCharacteristic->getUUID().equals(blefields[FW_DATA].uuid) )
		{
			Serial.printf("Received FwData message with length %u\n", dlen);

			uint16_t partIndex;
			memcpy(&partIndex, &data[0], sizeof(partIndex));

			uint16_t partSize;
			memcpy(&partSize, &data[2], sizeof(partSize));

			uint8_t* payload = new uint8_t[256];
			memset(payload, 0, 256);
			memcpy(payload, &data[4], 256);

			uint32_t partCRC32;
			memcpy(&partCRC32, &data[260], 4);

			Serial.printf("Firmware part with index %4u has %3u bytes and crc32 %08X\n", partIndex, partSize, partCRC32);

			// TODO: DO SOMETHING WITH PAYLOAD
			delete[] payload;

			// notify success (0x00000000)
			uint32_t successCode = 0x00000000;
			serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);

			return;
		}

	}

	//private int8_t processOmnibusSpecialCases()
};


CBCharacteristic pbleCharacteristicCallbacks;

BLEAdvertisementData advert;
BLEAdvertising *pAdvertising;

void changeAdvName( uint8_t * name ){
	pAdvertising = pServer->getAdvertising();
	pAdvertising->stop();
	advert.setName(std::string((char*)name));
	pAdvertising->setAdvertisementData(advert);
	pAdvertising->start();

	return;
}


void serverbleInit() {

	Serial.printf("sizeof(BLEService): %d, NUM: %d, total:%d\n", sizeof(BLEService), NUMBER_OF_SERVICES, sizeof(BLEService) * NUMBER_OF_SERVICES);
	Serial.printf("sizeof(BLECharacteristic): %d, NUM: %d, total:%d\n", sizeof(BLECharacteristic), NUMBER_OF_CHARACTERISTICS, sizeof(BLECharacteristic) * NUMBER_OF_CHARACTERISTICS);
	Serial.printf("sizeof(BLE_FIELD): %d, NUM: %d, total:%d\n", sizeof(BLEService), MAX_BLE_FIELDS, sizeof(BLEService) * MAX_BLE_FIELDS);
	Serial.printf("sizeof(BLE2902): %d\n", sizeof(BLE2902));

	milestone("at the beginning of serverbleInit");

	// Create the BLE Device
	BLEDevice::init("VCD1701XXXX");

	milestone("after creating BLEDevice");

	// Create the BLE Server
	pServer = BLEDevice::createServer();
	milestone("after BLEDevice::createServer");

	pServer->setCallbacks(new serverCallbacks());
	milestone("after creating and setting serverCallbacks");


	int indexService = 0;
	int indexCharacteristic = 0;
	int i;
	for ( i = 0; i < MAX_BLE_FIELDS; i++ )
	{
		if ( blefields[i].type == TYPE_SERV )
		{
			pbleServices[indexService] = pServer->createService(blefields[i].uuid,blefields[i].numberOfCharacteristics*3 );	
			indexService++;
		}
		else if ( blefields[i].type == TYPE_CHAR )
		{
			pbleCharacteristics[indexCharacteristic] = pbleServices[blefields[i].indexServ]->createCharacteristic( blefields[i].uuid, blefields[i].properties );
			if ( blefields[i].descriptor2902 == 1 )
			{
				pbleCharacteristics[indexCharacteristic]->addDescriptor(new BLE2902());
			}
			pbleCharacteristics[indexCharacteristic]->setCallbacks(&pbleCharacteristicCallbacks);
			indexCharacteristic++;
		}
	}

	milestone("after creating services and characteristics");

	// Start the service
	for ( i = 0; i < NUMBER_OF_SERVICES; i++ )
	{
		pbleServices[i]->start();
	}

	milestone("after starting services");

	// Setup advertising
	pServer->getAdvertising()->addServiceUUID(BLEUUID((uint16_t)0xCD01));

	milestone("after starting advertising");

	printf("Waiting a client connection to notify...\r\n");

	xTaskCreate(serverbleTask,
			"TASK BLE",
			4096*4,//4096*4,
			NULL,
			1,
			NULL	
		   );

	milestone("after creating serverbleTask");

}

void serverbleStartAdvertising(void)
{
	Serial.println("start advertising...");
	pServer->getAdvertising()->start();
}

void serverbleTask(void *arg)
{
	while (1)
	{
		if (deviceBleConnected && !oldDeviceBleConnected) {
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
		vTaskDelay(500 / portTICK_PERIOD_MS);	
	}
}



