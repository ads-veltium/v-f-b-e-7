
#include "../control.h"
#include "ble_rcs.h"
#include "ble_rcs_server.h"
#include "services/gap/ble_svc_gap.h"


StaticTask_t xBLEBuffer ;
static StackType_t xBLEStack[4096*2] EXT_RAM_ATTR;

//Update sistem files
File UpdateFile;
uint8_t  UpdateType  = 0;
uint16_t Conn_Handle = 0;

NimBLEDevice BLE_SERVER EXT_RAM_ATTR;

extern carac_Update_Status 			UpdateStatus;
extern carac_Comands                Comands;
#ifdef CONNECTED
	extern carac_Coms					Coms;
	extern carac_Firebase_Configuration ConfigFirebase;
#endif

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
BLEServer *pServer = (BLEServer*) heap_caps_malloc(sizeof(BLEServer), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
bool deviceBleConnected = false;
bool oldDeviceBleConnected = false;
bool deviceBleDisconnect = false;

uint8_t txValue = 0;

TaskHandle_t hdServerble = NULL;

// buffer for sending data downstream (to main CPU).
// must be large enough to hold maximum writeable characteristic (288 bytes)
// plus 4 header bytes, so 300 bytes should be enough.
// actually
//uint8 *buffer_tx=(uint8 *)ps_calloc(300,sizeof(uint8)); EXT_RAM_ATTR
uint8 buffer_tx[500] EXT_RAM_ATTR;


BLEService *pbleServices[NUMBER_OF_SERVICES];
//BLEService *pbleServices = (BLEService*) ps_malloc(sizeof(BLEService)*NUMBER_OF_SERVICES);
//BLECharacteristic *pbleCharacteristics = (BLECharacteristic*) ps_malloc(sizeof(BLECharacteristic)*NUMBER_OF_SERVICES);

BLECharacteristic *pbleCharacteristics[NUMBER_OF_CHARACTERISTICS];

#define PROP_RW NIMBLE_PROPERTY::READ |NIMBLE_PROPERTY::WRITE
#define PROP_RN NIMBLE_PROPERTY::READ |NIMBLE_PROPERTY::NOTIFY
#define PROP_R  NIMBLE_PROPERTY::READ
#define PROP_WN NIMBLE_PROPERTY::WRITE |NIMBLE_PROPERTY::NOTIFY


// VSC_SELECTOR       RW  16
// VSC_OMNIBUS        RW  16
// VSC_OMNINOT        RN  16
// VSC_RECORD         R  512
// VSC_SCHED_MATRIX   RW 168
// VSC_FW_COMMAND     WN 288
// VSC_NET_GROUP      N  451
// VSC_CHARGING_GROUP RW 451

BLE_FIELD blefields[MAX_BLE_FIELDS] =
{
	{TYPE_SERV, SERV_STATUS, BLEUUID((uint16_t)0xCD01),NUMBER_OF_CHARACTERISTICS, 0, 0, (indexCharacteristicsAll)0, (indexCharacteristics)0, 0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC001),NUMBER_OF_CHARACTERISTICS, PROP_RW, 0, SELECTOR,      BLE_CHA_SELECTOR,    0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC002),NUMBER_OF_CHARACTERISTICS, PROP_RW, 0, OMNIBUS,       BLE_CHA_OMNIBUS,     0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC003),NUMBER_OF_CHARACTERISTICS, PROP_RN, 0, RCS_HPT_STAT,  BLE_CHA_HPT_STAT,    1},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC004),NUMBER_OF_CHARACTERISTICS, PROP_RN, 0, RCS_INS_CURR,  BLE_CHA_INS_CURR,    1},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC005),NUMBER_OF_CHARACTERISTICS, PROP_R,  0, RCS_RECORD,    BLE_CHA_RCS_RECORD,  0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC006),NUMBER_OF_CHARACTERISTICS, PROP_RW, 0, RCS_SCH_MAT,   BLE_CHA_RCS_SCH_MAT, 0},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC007),NUMBER_OF_CHARACTERISTICS, PROP_WN, 0, FW_DATA,       BLE_CHA_FW_DATA,     1},

#ifdef CONNECTED
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC008),NUMBER_OF_CHARACTERISTICS, PROP_RN, 0, RCS_INSB_CURR, BLE_CHA_INSB_CURR,   1},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC009),NUMBER_OF_CHARACTERISTICS, PROP_RN, 0, RCS_INSC_CURR, BLE_CHA_INSC_CURR,   1},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC00A),NUMBER_OF_CHARACTERISTICS, PROP_RN, 0, RCS_NET_GROUP, BLE_CHA_NET_GROUP,   1},
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC00B),NUMBER_OF_CHARACTERISTICS, PROP_RW, 0, RCS_CHARGING_GROUP, BLE_CHA_CHARGING_GROUP,   1},
#endif

} ;


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
#ifdef CONNECTED
	if (handle == MEASURES_INST_CURRENTB_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_INSB_CURR]->setValue(data, len);
		pbleCharacteristics[BLE_CHA_INSB_CURR]->notify();
		return;
	}
	if (handle == MEASURES_INST_CURRENTC_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_INSC_CURR]->setValue(data, len);
		pbleCharacteristics[BLE_CHA_INSC_CURR]->notify();
		return;
	}
	if (handle == CHARGING_GROUP_BLE_NET_DEVICES) {
		pbleCharacteristics[BLE_CHA_NET_GROUP]->setValue(data, len);
		pbleCharacteristics[BLE_CHA_NET_GROUP]->notify();
		return;
	}
	if (handle == CHARGING_GROUP_BLE_CHARGING_GROUP) {
		pbleCharacteristics[BLE_CHA_CHARGING_GROUP]->setValue(data, len);
		pbleCharacteristics[BLE_CHA_CHARGING_GROUP]->notify();
		return;
	}
#endif
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
#ifdef CONNECTED
	if (handle == MEASURES_INST_CURRENTB_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_INSB_CURR]->setValue(data, len);
		return;
	}
	if (handle == MEASURES_INST_CURRENTC_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_INSC_CURR]->setValue(data, len);
		return;
	}

	if (handle == MEASURES_INST_CURRENTB_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_INSB_CURR]->setValue(data, len);
		return;
	}
	if (handle == MEASURES_INST_CURRENTC_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_INSC_CURR]->setValue(data, len);
		return;
	}
#endif
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

void serverbleSetConnected(bool value){
	deviceBleConnected = value;
}

bool serverbleGetConnected(void)
{
	return deviceBleConnected;
}

class serverCallbacks: public BLEServerCallbacks 
{
	void onConnect(BLEServer* pServer, ble_gap_conn_desc *desc) 
	{
		deviceConnectInd();		
		Conn_Handle = desc->conn_handle;

	};

	void onDisconnect(BLEServer* pServer) 
	{
		deviceBleConnected = false;
		deviceDisconnectInd();
		buffer_tx[0] = HEADER_TX;
		buffer_tx[1] = (uint8)(BLOQUE_STATUS >> 8);
		buffer_tx[2] = (uint8)(BLOQUE_STATUS);
		buffer_tx[3] = 2;
		buffer_tx[4] = deviceBleConnected;
		buffer_tx[5] = ESTADO_NORMAL;
		controlSendToSerialLocal(buffer_tx, 6);

		Conn_Handle=0;
	}
};

class CBCharacteristic: public BLECharacteristicCallbacks 
{
	void onWrite(BLECharacteristic *pCharacteristic) 
	{
		std::string rxValue = pCharacteristic->getValue();

		//Serial.printf("onWrite: char = %s\n", pCharacteristic->getUUID().toString().c_str());

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

			#ifdef DEBUG_BLE
			Serial.printf("Received write request for selector %u\n", selector);
			#endif
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

			#ifdef DEBUG_BLE
			Serial.printf("Received omnibus write request for selector %u\n", selector);
			Serial.printf("Received omnibus write request for handle   %u\n", handle);
			#endif

			// special cases
			if (handle == AUTENTICACION_TOKEN_CHAR_HANDLE) {
				setAuthToken(payload, size);
				return;
			}
			if (handle == RESET_RESET_CHAR_HANDLE) {
				if( isMainFwUpdateActive() )
				{
					vTaskDelay(pdMS_TO_TICKS(200));	
					MAIN_RESET_Write(0);
					vTaskDelay(pdMS_TO_TICKS(100));	
					ESP.restart();
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
				String Signature;
				char signature[11];
				memcpy(signature, &payload[0], 10);
				signature[10] = '\0';
				Signature= String(signature);
				uint32_t fileSize;
				memcpy(&fileSize, &payload[10], sizeof(fileSize));

				#ifdef DEBUG_BLE
				Serial.printf("Received FwUpdate Prolog\n");
				Serial.printf("Firmware file with signature %s has %u bytes\n", signature, fileSize);
				#endif

				uint32_t successCode = 0x00000000;
				//Empezar el sistema de actualizacion
				UpdateStatus.DescargandoArchivo=1;
				if(strstr (signature,"VBLE")){
					UpdateType= VBLE_UPDATE;
					#ifndef UPDATE_COMPRESSED
						Serial.println("Updating VBLE");			
						if(!Update.begin(fileSize)){
							Serial.println("File too big");
							Update.end();
							successCode = 0x00000001;
						};
					#else 
						Serial.println("Updating VBLE Compressed");	
						if (SPIFFS.begin(1,"/spiffs",1,"ESP32")){}
							vTaskDelay(pdMS_TO_TICKS(50));
							SPIFFS.format();
						}
						vTaskDelay(pdMS_TO_TICKS(50));
						UpdateFile = SPIFFS.open("/VBLE2.bin.gz", FILE_WRITE);
					#endif
				}
				else if(strstr (signature,"VELT")){
					Serial.println("Updating VELT");
					UpdateType= VELT_UPDATE;
					SPIFFS.end();
					if(!SPIFFS.begin(1,"/spiffs",1,"PSOC5")){
						SPIFFS.end();					
						SPIFFS.begin(1,"/spiffs",1,"PSOC5");
					}
					if(SPIFFS.exists("/FreeRTOS_V6.cyacd")){
						vTaskDelay(pdMS_TO_TICKS(50));
						SPIFFS.format();
					}

					vTaskDelay(pdMS_TO_TICKS(50));
					UpdateFile = SPIFFS.open("/FreeRTOS_V6.cyacd", FILE_WRITE);
				}
				else{
					successCode = 0x00000040;
				}
				
				// notify success (0x00000000)	
				serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);

				return;
			}
			if (handle == FWUPDATE_BIRD_EPILOG_PSEUDO_CHAR_HANDLE) {
				// epilog
				

				uint32_t fullCRC32;
				memcpy(&fullCRC32, &payload[0], 4);

				#ifdef DEBUG_BLE
				Serial.printf("Firmware file has global crc32 %08X\n", fullCRC32);
				Serial.printf("Received FwUpdate Epilog\n");
				#endif
				// notify success (0x00000000)
				uint32_t successCode = 0x00000000;
				serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);

				//Terminar el sistema de actualizacion
				UpdateStatus.DescargandoArchivo=0;
				UpdateStatus.InstalandoArchivo=1;
			    if(UpdateType == VBLE_UPDATE){
					#ifndef UPDATE_COMPRESSED
						if(Update.end()){	
							Comands.reset = true;
						}
					#else 
						Serial.println("Decompressing");
						xTaskCreate(UpdateCompressedTask, "DECOMPRESS_TASK", 4096*4, NULL,1,NULL);
					#endif
					
				}
				else if(UpdateType == VELT_UPDATE){
					
					UpdateFile.close();
					setMainFwUpdateActive(1);
				}				
				return;
			}

			// check if we are authenticated, and leave if we aren't
			if (!authorizedOK()) {
				return;
			}
#ifdef CONNECTED
			if(handle == COMS_CONFIGURATION_WIFI_START_PROV){
				if(payload[0]==1){
					Coms.StartProvisioning = true;
				}
				else if(payload[0]==2){
					Coms.StartSmartconfig = true;
				}
				return;
			}
			else if(handle == COMS_CONFIGURATION_ETH_ON){
				SendToPSOC5(payload[0],COMS_CONFIGURATION_ETH_ON);
			}
			else if(handle == COMS_CONFIGURATION_WIFI_ON){
				SendToPSOC5(payload[0],COMS_CONFIGURATION_WIFI_ON);
			}
#endif			
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

		if ( pCharacteristic->getUUID().equals(blefields[FW_DATA].uuid) )
		{
			//Serial.printf("Received FwData message with length %u\n", dlen);

			/*uint16_t partIndex;
			memcpy(&partIndex, &data[0], sizeof(partIndex));*/

			uint16_t partSize;
			memcpy(&partSize, &data[2], sizeof(partSize));

			uint8_t* payload = new uint8_t[256];
			memset(payload, 0, 256);
			memcpy(payload, &data[4], 256);

			/*uint32_t partCRC32;
			memcpy(&partCRC32, &data[260], 4);*/

			//Serial.printf("Firmware part with index %4u has %3u bytes and crc32 %08X\n", partIndex, partSize, partCRC32);

			// notify success (0x00000000)
			uint32_t successCode = 0x00000000;

			//Escribir los datos en la actualizacion
			if(UpdateType == VBLE_UPDATE){
				#ifndef UPDATE_COMPRESSED
					if(Update.write(payload,partSize)!=partSize){
						Serial.println("Writing Error");
						successCode = 0x00000002;
						
					}
				#else
					if(UpdateFile.write(payload,partSize)!=partSize){
						Serial.println("Writing Error");
						successCode = 0x00000002;
					}
				#endif
			}
			else if(UpdateType == VELT_UPDATE){
				if(UpdateFile.write(payload,partSize)!=partSize){
					Serial.println("Writing Error");
					successCode = 0x00000002;
					SPIFFS.end();
					UpdateFile.close();
				}
			}	

			// TODO: DO SOMETHING WITH PAYLOAD
			delete[] payload;

			
			serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);

			return;
		}

	}

	//private int8_t processOmnibusSpecialCases()
};


CBCharacteristic pbleCharacteristicCallbacks;

BLEAdvertisementData advert EXT_RAM_ATTR;
BLEAdvertising *pAdvertising EXT_RAM_ATTR;

void changeAdvName( uint8_t * name ){
	ble_svc_gap_device_name_set(std::string((char*)name).c_str());

	pAdvertising = pServer->getAdvertising();
	pAdvertising->stop();

	advert.setFlags(0x06);
	advert.setName(std::string((char*)name));
	advert.setCompleteServices((BLEUUID((uint16_t)0xCD01)));
	advert.setPreferredParams(6,128);
	
	#ifdef DEBUG_BLE
	Serial.println(advert.getPayload().c_str());
	#endif

	pAdvertising->setAdvertisementData(advert);
	pAdvertising->setName(std::string((char*)name));
	pAdvertising->setMaxInterval(1600);
	pAdvertising->setMinInterval(32);
	pAdvertising->setMaxPreferred(128);
	pAdvertising->setMinPreferred(6);
	pAdvertising->addServiceUUID(BLEUUID((uint16_t)0xCD01));
	pAdvertising->start();

	return;
}


void serverbleInit() {

	#ifdef DEBUG_BLE
	Serial.printf("sizeof(BLEService): %d, NUM: %d, total:%d\n", sizeof(BLEService), NUMBER_OF_SERVICES, sizeof(BLEService) * NUMBER_OF_SERVICES);
	Serial.printf("sizeof(BLECharacteristic): %d, NUM: %d, total:%d\n", sizeof(BLECharacteristic), NUMBER_OF_CHARACTERISTICS, sizeof(BLECharacteristic) * NUMBER_OF_CHARACTERISTICS);
	Serial.printf("sizeof(BLE_FIELD): %d, NUM: %d, total:%d\n", sizeof(BLEService), MAX_BLE_FIELDS, sizeof(BLEService) * MAX_BLE_FIELDS);

	milestone("at the beginning of serverbleInit");

	#endif
	// Create the BLE_SERVER Device
	BLE_SERVER.init("VCD1701XXXX");
	BLE_SERVER.setMTU(512);

	#ifdef DEBUG_BLE
	milestone("after creating BLEDevice");
	#endif
	// Create the BLE_SERVER Server
	pServer = BLE_SERVER.createServer();

	#ifdef DEBUG_BLE
	milestone("after BLEDevice::createServer");
	#endif

	pServer->setCallbacks(new serverCallbacks());

	#ifdef DEBUG_BLE
	milestone("after creating and setting serverCallbacks");
	#endif

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
				pbleCharacteristics[indexCharacteristic]->createDescriptor (blefields[i].uuid, blefields[i].properties);
			}
			pbleCharacteristics[indexCharacteristic]->setCallbacks(&pbleCharacteristicCallbacks);
			indexCharacteristic++;
		}
	}

	#ifdef DEBUG_BLE
	milestone("after creating services and characteristics");
	#endif 

	// Start the service
	for ( i = 0; i < NUMBER_OF_SERVICES; i++ )
	{
		pbleServices[i]->start();
	}

	#ifdef DEBUG_BLE
	milestone("after starting services");
	#endif

	// Setup advertising
	pServer->getAdvertising()->addServiceUUID(BLEUUID((uint16_t)0xCD01));

	#ifdef DEBUG_BLE
	milestone("after starting advertising");
	printf("Waiting a client connection to notify...\r\n");
	#endif

	xTaskCreateStatic(serverbleTask,"TASK BLE_SERVER",4096*2,NULL,PRIORIDAD_BLE,xBLEStack,&xBLEBuffer); 

	#ifdef DEBUG_BLE
	milestone("after creating serverbleTask");
	#endif
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
			printf(" disconnecting \r\n");
			pServer->stopAdvertising();
			
			while(!pServer->getAdvertising()->isAdvertising()){
				delay(100);
				pServer->startAdvertising(); // restart advertising
			}
			printf("start advertising again\r\n");
			
			UpdateStatus.DescargandoArchivo=0;
			oldDeviceBleConnected = deviceBleConnected;
		}
		// connecting
		if (deviceBleConnected && !oldDeviceBleConnected) {
			printf(" connecting \r\n");
			// do stuff here on connecting
			oldDeviceBleConnected = deviceBleConnected;
		}

		if(deviceBleDisconnect){
			printf("desconectandome del intruso!!!\n");
			pServer->disconnect(Conn_Handle);
			deviceBleDisconnect= false;
		}
		vTaskDelay(pdMS_TO_TICKS(250));	
	}
}



