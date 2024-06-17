
#include "../control.h"
#include "../group_control.h"
#include "ble_rcs.h"
#include "ble_rcs_server.h"
#include "services/gap/ble_svc_gap.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <esp_system.h>
#include <esp_timer.h>

static const char* TAG = "serverble";

StaticTask_t xBLEBuffer ;
static StackType_t xBLEStack[4096*2] EXT_RAM_ATTR;

//Update sistem files
File UpdateFile;
uint8_t  UpdateType  = 0;
uint16_t Conn_Handle = 0;
uint8 RaspberryTest[6] ={139,96,111,50,166,220};
//uint8 RaspberryTest2[6] ={227 ,233, 58 ,1 ,95 ,228 };
uint8 RaspberryTest2[6] ={179 ,186, 112 ,1 ,95 ,228 };
//uint8 RaspberryTest2[6] ={153 ,117, 207 ,235 ,39 ,184 };

uint8 authChallengeReply[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8 authChallengeQuery[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8 authSuccess = 0;
int aut_semilla = 0x0000;

extern uint8 dispositivo_inicializado;

extern uint8 device_ID[16];
extern bool Testing;
extern carac_Status                 Status;
NimBLEDevice BLE_SERVER EXT_RAM_ATTR;
extern carac_Update_Status 			UpdateStatus;
extern carac_Comands                Comands;
extern TickType_t AuthTimer;
extern uint8 deviceSerNum[10];
extern uint8 initialSerNum[10];
extern uint8 deviceSerNumFlash[10];
extern uint8 emergencyState;
static TickType_t ConexTimer;
int count = 0;

#ifdef CONNECTED

	extern carac_group ChargingGroup;
	extern carac_charger net_group[MAX_GROUP_SIZE];
	extern uint8_t net_group_size;
	extern carac_charger charger_table[MAX_GROUP_SIZE];
	extern carac_circuito Circuitos[MAX_GROUP_SIZE];
	void coap_put( char* Topic, char* Message);
	extern carac_Contador   ContadorExt;
	extern carac_Coms					Coms;
	extern carac_Firebase_Configuration ConfigFirebase;
	
#endif

uint64_t startTime;
uint64_t endTime;

uint8 backdoor_selector = 0;

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


static unsigned long crc32(int n, unsigned char c[], int checksum)
{
	int i, j, b;
	int r;
	int b0,b1,b2,b3;

	r = checksum;
	for (i = 0; i < n; i++) {
		b = c[i];
		b&=0xFF;
		for (j = 0; j < 8; j++){
			boolean flag = ((b&1)^(r &1))!=0;
			r = r>>1;
			r=r&0x7FFFFFFFUL;

			if(flag){
				r^=0xEDB88320UL;
			}

			b= b>>1;
		}
	}
	r= r ^ 0xFFFFFFFFUL;

	b0 = (r&0x000000ff) << 24u;
	b1 = (r&0x0000ff00) << 8;
	b2 = (r&0x00ff0000) >> 8;
	b3 = (r&0xff000000) >> 24u;

	return (b0|b1|b2|b3);
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
// BLE_CHA_STATUS_COMS RN 16

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
	{TYPE_CHAR, SERV_STATUS, BLEUUID((uint16_t)0xC00C),NUMBER_OF_CHARACTERISTICS, PROP_RN, 0, RCS_STATUS_COMS, BLE_CHA_STATUS_COMS,   1},
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
	if (handle == STATUS_COMS) {
		pbleCharacteristics[BLE_CHA_STATUS_COMS]->setValue(data, len);
		pbleCharacteristics[BLE_CHA_STATUS_COMS]->notify();
		return;
	}
#endif
	if (handle == FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE) {
		pbleCharacteristics[BLE_CHA_FW_DATA]->setValue(data, len);
		pbleCharacteristics[BLE_CHA_FW_DATA]->notify();
		return;
	}
	// set characteristic value using selector mechanism
	rcs_server_set_chr_value(handle, data, len);
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

	if (handle == CHARGING_GROUP_BLE_NET_DEVICES) {
		pbleCharacteristics[BLE_CHA_NET_GROUP]->setValue(data, len);
		return;
	}
	if (handle == CHARGING_GROUP_BLE_CHARGING_GROUP) {
		pbleCharacteristics[BLE_CHA_CHARGING_GROUP]->setValue(data, len);
		return;
	}
	if (handle == STATUS_COMS) {
		pbleCharacteristics[BLE_CHA_STATUS_COMS]->setValue(data, len);
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

bool serverbleGetConnected(void){
	return deviceBleConnected;
}

class serverCallbacks: public BLEServerCallbacks {
	void onConnect(BLEServer* pServer, ble_gap_conn_desc *desc) {
		ESP_LOGI(TAG,"Connected client address: %s", NimBLEAddress(desc->peer_ota_addr).toString().c_str());
		if(!memcmp(desc->peer_id_addr.val, RaspberryTest,6) || !memcmp(desc->peer_id_addr.val, RaspberryTest2,6) ){
			Testing = true;
			setAuthToken(authChallengeReply, 8);
		}
		else{
			Testing = false;
#ifdef DEBUG_BLE
			startTime = esp_timer_get_time();
#endif
			deviceConnectInd();	
		}
		Conn_Handle = desc->conn_handle;
	};

	void onDisconnect(BLEServer* pServer) {
		ESP_LOGI(TAG,"serverCallbacks: onDisconnect");
		deviceBleConnected = false;
		Testing = false;
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

class CBCharacteristic: public BLECharacteristicCallbacks {
	void onWrite(BLECharacteristic *pCharacteristic) {
		ConexTimer = xTaskGetTickCount();
		static uint32_t UpdatefileSize;
		std::string rxValue = pCharacteristic->getValue();

		static uint8_t omnibus_packet_buffer[4 + RCS_CHR_OMNIBUS_SIZE];

		uint8_t* data = (uint8_t*)&rxValue[0];
		uint16_t dlen = (uint16_t)rxValue.length();

		if (pCharacteristic->getUUID().equals(blefields[SELECTOR].uuid)){
			// safety check: at least one byte for selector
			if (dlen < 1) {
				Serial.println("Error at write callback for Selector CHR: size less than 1");
				uint8_t empty_packet[4] = {0, 0, 0, 0};
				pbleCharacteristics[BLE_CHA_OMNIBUS]->setValue(empty_packet, 4);
				return;
			}
			// characteristic selector
			uint8_t selector = data[0];
			// payload to be write to omnibus characteristic
			uint8_t* payload = rcs_server_get_data_for_selector(selector);
			// size of payload to be written to omnibus characteristic
			uint8_t  pldsize = rcs_get_size(selector);

			if(dispositivo_inicializado != 2){
				return;
			}
#ifdef DEBUG_BLE
			ESP_LOGI(TAG,"Received READ request for selector %u",selector);
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
			if (!authorizedOK()){
				// no authentication, only allowed operation is matrix read
				uint16_t handle = rcs_handle_for_idx(selector);
				if (handle != AUTENTICACION_MATRIX_CHAR_HANDLE) {
					ESP_LOGW(TAG,"Bad atuhentication token. Will return dummy data when read");
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

		if (pCharacteristic->getUUID().equals(blefields[OMNIBUS].uuid)){
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
			ESP_LOGI(TAG, "Received WRITE reques for selector %u", selector);
#endif

			// special cases
			if (handle == AUTENTICACION_TOKEN_CHAR_HANDLE) {
				setAuthToken(payload, size);
				return;
			}
			/*if (handle == RESET_RESET_CHAR_HANDLE) {
				MAIN_RESET_Write(0);
				vTaskDelay(pdMS_TO_TICKS(500));	
				ESP.restart();
				return;
			}*/
			
			if (handle == FWUPDATE_BIRD_PROLOG_PSEUDO_CHAR_HANDLE) {
				
				// prolog
				String Signature;
				char signature[11];
				memcpy(signature, &payload[0], 10);
				signature[10] = '\0';
				Signature= String(signature);
				
				memcpy(&UpdatefileSize, &payload[10], sizeof(UpdatefileSize));

				#ifdef DEBUG_BLE
				Serial.printf("Received FwUpdate Prolog\n");
				Serial.printf("Firmware file with signature %s has %u bytes\n", signature, UpdatefileSize);
				#endif

				uint32_t successCode = 0x00000000;
				//Empezar el sistema de actualizacion
				UpdateStatus.DescargandoArchivo=1;
				if(strstr (signature,"VBLE")){
					UpdateType= ESP_UPDATE;
					Update.end();
					#ifdef DEBUG_BLE
					Serial.println("Updating ESP");
					#endif			
					if(!Update.begin(UpdatefileSize)){
						Serial.println("File too big");
						Update.end();
						successCode = 0x00000001;
					};
				}
				else if(strstr (signature,"VELT")){
					#ifdef DEBUG_BLE
					Serial.println("Updating PSOC");
					#endif
					UpdateType= PSOC_UPDATE;				

					SPIFFS.end();
					UpdateFile.close();
					if(!SPIFFS.begin(1,"/spiffs",1,"PSOC5")){
						SPIFFS.end();					
						SPIFFS.begin(1,"/spiffs",1,"PSOC5");
					}

					//Almacenar la version anterior de firmware
					if(SPIFFS.exists("/FW_PSoC6_v7.cyacd2")){
						if(SPIFFS.exists("/FW_PSoC6_v7_old.cyacd2")){
							SPIFFS.remove("/FW_PSoC6_v7_old.cyacd2");
						}
						//Comprobar si nos van a entrar las dos versiones de firmware
						File dir = SPIFFS.open("/");
						if(dir.size() + UpdatefileSize > 0x80000){
							#ifdef DEBUG
							printf("No entran los dos documentos! Borro el anterior!\n");
							#endif
							SPIFFS.remove("/FW_PSoC6_v7.cyacd2");
						}
						else{
							SPIFFS.rename("/FW_PSoC6_v7.cyacd2", "/FW_PSoC6_v7_old.cyacd2");
						}
					}else{
						#ifdef DEBUG_BLE
							Serial.println("No existe el fichero FW!!");
						#endif
					}

					vTaskDelay(pdMS_TO_TICKS(50));
					UpdateFile = SPIFFS.open("/FW_PSoC6_v7.cyacd2", FILE_WRITE);
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
				#endif

				
				// notify success (0x00000000)
				uint32_t successCode = 0x00000000;
				serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);

				//Terminar el sistema de actualizacion
				UpdateStatus.DescargandoArchivo=0;
				UpdateStatus.InstalandoArchivo=1;
			    if(UpdateType == ESP_UPDATE){
					#ifndef UPDATE_COMPRESSED
						if(Update.end()){	
							#ifdef CONNECTED
							Comands.reset = true;
							#else
							MAIN_RESET_Write(0);						
							ESP.restart();
							#endif
						}
					#else 
						Serial.println("Decompressing");
						xTaskCreate(UpdateCompressedTask, "DECOMPRESS_TASK", 4096*4, NULL,1,NULL);
					#endif
					
				}
				else if(UpdateType == PSOC_UPDATE){
					
					UpdateFile.close();

					File InstalledFile = SPIFFS.open("/FW_PSoC6_v7.cyacd2", FILE_READ);

					if(UpdatefileSize==InstalledFile.size()){
						#ifdef DEBUG_BLE
							Serial.printf("El fichero guardado está entero!\n");
						#endif
						InstalledFile.close();
						SPIFFS.end();
						setMainFwUpdateActive(1);
						if(emergencyState==1){
							if(!SPIFFS.begin(1,"/spiffs",1,"ESP32")){
								SPIFFS.end();					
								SPIFFS.begin(1,"/spiffs",1,"ESP32");
							}
							Configuracion.data.count_reinicios_malos=0;
							emergencyState = 0;
							MAIN_RESET_Write(0);
							delay(200);						
							ESP.restart();
						}
					}else{
						#ifdef DEBUG_BLE
							Serial.printf("El fichero guardado no es correcto!\n");
						#endif
						InstalledFile.close();
						SPIFFS.remove("/FW_PSoC6_v7.cyacd2");
						successCode = 0x00000002;
						serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);
						MAIN_RESET_Write(0);
						delay(200);						
						ESP.restart();
					}
				}					
				return;
			}

			// check if we are authenticated, and leave if we aren't
			if (!authorizedOK()) {
				return;
			}
#ifdef CONNECTED
			if(handle == COMS_CONFIGURATION_WIFI_START_PROV){
				Coms.StartProvisioning = true;
				return;
			}
			else if(handle == COMS_CONFIGURATION_ETH_ON){
				SendToPSOC5(payload[0],COMS_CONFIGURATION_ETH_ON);
				return;
			}
			else if(handle == COMS_CONFIGURATION_WIFI_ON){
				SendToPSOC5(payload[0],COMS_CONFIGURATION_WIFI_ON);
				return;
			}

			else if (handle == GROUPS_PARAMS) {
				if(!ChargingGroup.Conected && payload[0]){
					ChargingGroup.Params.GroupMaster = true;
				}
				uint8_t sendBuffer[7];
				sendBuffer[0] = ChargingGroup.Params.GroupMaster;

				if(ChargingGroup.Params.GroupActive && !payload[0]){
					ChargingGroup.SendNewParams = true;
				}
				memcpy(&sendBuffer[1], payload,6);
				
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(handle >> 8);
				buffer_tx[2] = (uint8)(handle);
				buffer_tx[3] = 7;
				memcpy(&buffer_tx[4], sendBuffer, 7);
				controlSendToSerialLocal(buffer_tx, 7 + 4);

				delay(100);
				ChargingGroup.SendNewParams = true;
				return;
			}
			else if (handle == GROUPS_CIRCUITS) {
                
				uint8_t circuit_number = payload[0];
				size = circuit_number +1;
				buffer_tx[0] = HEADER_TX;
				buffer_tx[1] = (uint8)(handle >> 8);
				buffer_tx[2] = (uint8)(handle);
				buffer_tx[3] = circuit_number +1;
				memcpy(&buffer_tx[4], payload, size);
				controlSendToSerialLocal(buffer_tx, size + 4);
				delay(100);
				ChargingGroup.SendNewCircuits = true;
				
				return;
			}
			else if (handle == GROUPS_OPERATIONS) {
				uint8_t operation = payload[0];
				switch(operation){
					//Creacion
					case 1:
						
						if(!ChargingGroup.Conected){
							ChargingGroup.Params.GroupMaster = true;
							ChargingGroup.Creando = true;
						}

						//Actualizar net devices
						uint8_t net_buffer[452];
						net_buffer[0] = net_group_size +1;
						memcpy(&net_buffer[1], ConfigFirebase.Device_Id, 8);
						net_buffer[9] = 0;
						for(int i =0;i< net_group_size; i++){
							memcpy(&net_buffer[i*9+10], net_group[i].name,8);
							net_buffer[i*9+18]=0;
						}
						serverbleNotCharacteristic(net_buffer,net_group_size*9 +10, CHARGING_GROUP_BLE_NET_DEVICES);
					break;

					//modificacion
					case 2:
						if(!ChargingGroup.Conected){
							ChargingGroup.Params.GroupMaster = true;
						}
						
						//Actualizar net devices
						uint8_t group_buffer[452];
						group_buffer[0] = net_group_size +1;
						memcpy(&group_buffer[1], ConfigFirebase.Device_Id, 8);
						group_buffer[9] = 0;
						for(int i =0;i< net_group_size; i++){
							memcpy(&group_buffer[i*9+10], net_group[i].name,8);
							group_buffer[i*9+18]=0;
						}
						serverbleNotCharacteristic(group_buffer,net_group_size*9 +10, CHARGING_GROUP_BLE_NET_DEVICES);

						//Actualizar group_devices
						group_buffer[0] = ChargingGroup.Charger_number;
						for(int i =0;i< ChargingGroup.Charger_number; i++){
							memcpy(&group_buffer[i*9+1], charger_table[i].name,8);
							group_buffer[i*9+9]=(charger_table[i].Circuito << 2) + charger_table[i].Fase;
						}
						serverbleSetCharacteristic(group_buffer,ChargingGroup.Charger_number*9+1 ,CHARGING_GROUP_BLE_CHARGING_GROUP);

						//Actualizar params
						group_buffer[0] = ChargingGroup.Params.GroupActive;
						group_buffer[1] = ChargingGroup.Params.CDP;
						group_buffer[2] = (uint8)(ChargingGroup.Params.ContractPower & 0x00FF);
						group_buffer[3] = (uint8)((ChargingGroup.Params.ContractPower >>8) & 0x00FF);
						group_buffer[4] = (uint8)(ChargingGroup.Params.potencia_max & 0x00FF);
						group_buffer[5] = (uint8)((ChargingGroup.Params.potencia_max >>8) & 0x00FF);

						serverbleSetCharacteristic(group_buffer,6 ,GROUPS_PARAMS);

						//Actualizar circuits
						group_buffer[0] = ChargingGroup.Circuit_number;
						for(uint8_t i=0;i< ChargingGroup.Circuit_number;i++){ 
							group_buffer[i+1] = Circuitos[i].limite_corriente;
						}
						serverbleSetCharacteristic(group_buffer,ChargingGroup.Circuit_number +1 ,GROUPS_CIRCUITS);
					break;

					//borrado
					case 3:
						broadcast_a_grupo("Delete group", 12);
						New_Control("Delete", 7);
					break;

					//lectura
					case 4:
						printf("Tengo que leer el grupo!\n");
					break;

					default:
						printf("Me ha llegado una operacion de grupo rara %i\n", operation);
					break;
				}
				return;
			}
			
#endif			

			if (handle == POLICY){
				#ifdef DEBUG
					Serial.printf("Nueva policy recibida! %c%c%c\n", payload[0],payload[1],payload[2]);
				#endif

				//Si el valor que me llega no es ninguno estandar, descartarlo
				if(memcmp(payload, "ALL",3) && memcmp(payload,"AUT",3) && memcmp(payload, "NON",3)){
					#ifdef DEBUG
						printf("Me ha llegado un valor turbio en policy!!\n %c%c%c", payload[0],payload[1],payload[2]);
					#endif
					return;
				}
				memcpy(Configuracion.data.policy, payload,3);
				modifyCharacteristic((uint8_t*)&Configuracion.data.policy, 3, POLICY);
				return;
			} 

			if (handle == DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE){
				backdoor_selector = payload [0];
				ESP_LOGI(TAG,"SET backdoor_selector=%i",backdoor_selector);
				modifyCharacteristic((uint8_t*)&backdoor_selector, 1, DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE);
				switch (backdoor_selector){
#ifdef CONNECTED
				case BACKDOOR_ACTION_DELETE_GROUP:
					modifyCharacteristic((uint8_t*)&ChargingGroup.Params.GroupActive, 1, DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE);
					modifyCharacteristic((uint8_t*)&ChargingGroup.Params.GroupMaster, 1, DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE);
					break;
#endif
				default:
				break;
				}
			}
			if ((handle == TEST_LAUNCH_MCB_TEST_CHAR_HANDLE) && (backdoor_selector != 0)) {
				ESP_LOGI(TAG,"CONFIRM backdoor selection");
				switch (backdoor_selector){
#ifdef CONNECTED
					case BACKDOOR_ACTION_DELETE_GROUP:
						ESP_LOGI(TAG,"Confirmed action: Borrado de grupo");
						broadcast_a_grupo("Delete group", 12);
						New_Control("Delete", 7);
						modifyCharacteristic((uint8_t*)&ChargingGroup.Params.GroupActive, 1, DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE);
						modifyCharacteristic((uint8_t*)&ChargingGroup.Params.GroupMaster, 1, DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE);
						break;
#endif
					default:
						ESP_LOGI(TAG,"No action");
					break;					
				}
				backdoor_selector = BACKDOOR_ACTION_NO_ACTION;
				modifyCharacteristic((uint8_t*)&backdoor_selector, 1, DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE);
				ESP_LOGI(TAG,"RESET backdoor_selector=%i",backdoor_selector);
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
			uint16_t size = 24;	// for schedule matrix
			
			buffer_tx[0] = HEADER_TX;
			buffer_tx[1] = (uint8)(SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE >> 8);
			buffer_tx[2] = (uint8)(SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);
			buffer_tx[3] = size +1;

			//domingo
			buffer_tx[4] = 0;
			memcpy(&buffer_tx[5], &data[0], size);
			controlSendToSerialLocal(buffer_tx, size + 5);
			delay(10);
			//lunes
			buffer_tx[4] = 1;
			memcpy(&buffer_tx[5], &data[24], size);
			controlSendToSerialLocal(buffer_tx, size + 5);
			delay(10);
			//martes
			buffer_tx[4] = 2;
			memcpy(&buffer_tx[5], &data[48], size);
			controlSendToSerialLocal(buffer_tx, size + 5);
			delay(10);
			//miercoles
			buffer_tx[4] = 3;
			memcpy(&buffer_tx[5], &data[72], size);
			controlSendToSerialLocal(buffer_tx, size + 5);
			delay(10);
			//jueves
			buffer_tx[4] = 4;
			memcpy(&buffer_tx[5], &data[96], size);
			controlSendToSerialLocal(buffer_tx, size + 5);
			delay(10);
			//viernes
			buffer_tx[4] = 5;
			memcpy(&buffer_tx[5], &data[120], size);
			controlSendToSerialLocal(buffer_tx, size + 5);
			delay(10);
			//sabado
			buffer_tx[4] = 6;
			memcpy(&buffer_tx[5], &data[144], size);
			controlSendToSerialLocal(buffer_tx, size + 5);

			return;
		}

		if ( pCharacteristic->getUUID().equals(blefields[FW_DATA].uuid) )
		{
			
			uint32_t successCode = 0x00000000;
			uint16_t partIndex;
			memcpy(&partIndex, &data[0], sizeof(partIndex));

			uint16_t partSize;
			memcpy(&partSize, &data[2], sizeof(partSize));
			ConexTimer = xTaskGetTickCount();
			Serial.printf("Received FwData message with length and size %u %u\n", dlen, partSize);
			uint8_t* payload = new uint8_t[partSize];
			memset(payload, 0, partSize);
			memcpy(payload, &data[4], partSize);

			uint32_t partCRC32;
			memcpy(&partCRC32, &data[260], 4);
			
			uint32_t checksum=crc32(partSize, payload,0xFFFFFFFFUL);

			if(!Testing){
				if(checksum!= partCRC32){
					successCode = 0x00000002;
					serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);
					delete[] payload;
					if(UpdateType == PSOC_UPDATE){
						SPIFFS.end();
						UpdateFile.close();
					}
					else{
						Update.end();
					}
					UpdateStatus.DescargandoArchivo = false;
					return;
				}
			}	
		

			//Escribir los datos en la actualizacion
			if(UpdateType == ESP_UPDATE){
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
			else if(UpdateType == PSOC_UPDATE){	
				uint16 write_result = UpdateFile.write(payload,partSize);
				Serial.printf("partSize: %u - write_result: %u\n", partSize, write_result);

				if(write_result == 0 && partSize > 0){
					write_result = UpdateFile.write(payload,partSize);
					Serial.printf("Reintento de write: %u\n", write_result);
				}
				if(write_result!=partSize){
					ESP_LOGE(TAG,"Writing Error");
					successCode = 0x00000002;
					SPIFFS.end();
					UpdateFile.close();	
				}

			}
			
			delete[] payload;

			
			serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);

			return;
		}
#ifdef CONNECTED 
		if ( pCharacteristic->getUUID().equals(blefields[RCS_CHARGING_GROUP].uuid) ){
			
			uint8 size = uint8(data[0]);
			printf("Me ha llegado un grupo con %i cargadores !\n", size);

			char sendBuffer[252];
			if(size< 10){
				sprintf(&sendBuffer[0],"0%i",(char)size);
			}

			else{
				sprintf(&sendBuffer[0],"%i",(char)size);
			}

			if(size>0){
				if(size>25){
					//Envio de la primera parte
					for(uint8_t i = 0;i < 25; i++){
						memcpy(&sendBuffer[2+(i*9)],&data[1+(i*9)],8);
						sendBuffer[10+(i*9)] = (char)data[9+(i*9)];
					}

					SendToPSOC5(sendBuffer,227,GROUPS_DEVICES_PART_1); 
					delay(250);

					//Envio de la segunda mitad
					if(size - 25< 10){
						sprintf(&sendBuffer[0],"0%i",(char)size-25);
					}
					else{
						sprintf(&sendBuffer[0],"%i",(char)size-25);
					}
					for(uint8_t i=0;i<(size - 25);i++){
						memcpy(&sendBuffer[2+(i*9)],&data[1+((i+25)*9)],8);
						sendBuffer[10+(i*9)] = (char)data[9+((i+25)*9)];
					}

					SendToPSOC5(sendBuffer,(size - 25)*9+2,GROUPS_DEVICES_PART_2); 

				}
				else{
					//El grupo entra en una parte, asique solo mandamos una
					for(uint8_t i=0;i<size;i++){
						memcpy(&sendBuffer[2+(i*9)],&data[1+(i*9)],8);
						sendBuffer[10+(i*9)] = (char)data[9+(i*9)];

					}
					SendToPSOC5(sendBuffer,size*9+2,GROUPS_DEVICES_PART_1); 
					
				}
			}
			else{
				for(int i=0;i<=250;i++){
					sendBuffer[i]=(char)0;
				}
				SendToPSOC5(sendBuffer,250,GROUPS_DEVICES_PART_1); 
				SendToPSOC5(sendBuffer,250,GROUPS_DEVICES_PART_2); 

			}
			delay(250);
			SendToPSOC5(2,SEND_GROUP_DATA);
			delay(250);
			ChargingGroup.SendNewGroup = true;
		}
#endif
	}

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
			// pbleServices[indexService] = pServer->createService(blefields[i].uuid,blefields[i].numberOfCharacteristics*3 );	
			pbleServices[indexService] = pServer->createService(blefields[i].uuid );	
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
	while (1){
		// Si transucrren DEFAULT_BLE_INACTIVE_TIMEOUT minutos sin que nadie haga nada por bluetooth,
		// y hay alguien conectado, expulsamos a quien esté conectado
		if (deviceBleConnected){

			if (pdTICKS_TO_MS(xTaskGetTickCount() - ConexTimer) > DEFAULT_BLE_INACTIVE_TIMEOUT){
				ESP_LOGI(TAG, "Desconexion por inactividad");
				deviceBleDisconnect = true;
			}
		}

		// disconnecting
		if (!deviceBleConnected && oldDeviceBleConnected) {
			ESP_LOGI(TAG, "Disconnectig");
			// vTaskDelay(500/portTICK_PERIOD_MS); // give the bluetooth stack the chance to get things ready
			// pServer->startAdvertising(); //Está activado por defcto advertiseOnDisconnect. 
			Status.LastConn = xTaskGetTickCount();
			UpdateStatus.DescargandoArchivo=0;
			oldDeviceBleConnected = deviceBleConnected;
		}
		// connecting
		if (deviceBleConnected && !oldDeviceBleConnected) {
			ESP_LOGI(TAG, "Connected");
			// do stuff here on connecting
			oldDeviceBleConnected = deviceBleConnected;
			serverbleNotCharacteristic((uint8_t *)Status.HPT_status, 2, STATUS_HPT_STATUS_CHAR_HANDLE);
			uint8 buffer[2];
			buffer[0] = (uint8)(Status.Measures.instant_current & 0x00FF);
			buffer[1] = (uint8)((Status.Measures.instant_current >> 8) & 0x00FF);
			serverbleNotCharacteristic(buffer, 2, MEASURES_INST_CURRENT_CHAR_HANDLE);
#ifdef CONNECTED
			buffer[0] = (uint8)(Status.MeasuresB.instant_current & 0x00FF);
			buffer[1] = (uint8)((Status.MeasuresB.instant_current >> 8) & 0x00FF);
			serverbleNotCharacteristic(buffer, 2, MEASURES_INST_CURRENTB_CHAR_HANDLE);
			buffer[0] = (uint8)(Status.MeasuresC.instant_current & 0x00FF);
			buffer[1] = (uint8)((Status.MeasuresC.instant_current >> 8) & 0x00FF);
			serverbleNotCharacteristic(buffer, 2, MEASURES_INST_CURRENTC_CHAR_HANDLE);
#endif
			//Coger el tiempo desde la conexion
			ConexTimer = xTaskGetTickCount();
		}
		if (deviceBleDisconnect){
			pServer->disconnect(Conn_Handle);
			deviceBleDisconnect= false;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void deviceConnectInd(void){
	if (dispositivo_inicializado == 2){
		const void *outputvec1;
		// This event is received when device is connected over GATT level
		uint32_t aut_semilla = esp_random();
		srand(aut_semilla);

		for (int i = 0; i < 8; i += 2){
			uint32_t random = esp_random();
			authChallengeQuery[i] = (uint8_t)random;
			authChallengeQuery[i + 1] = (uint8_t)(random >> 8);
		}
		authChallengeQuery[8]='\0';

		outputvec1 = dev_auth_challenge(authChallengeQuery);

		memcpy(authChallengeReply, outputvec1, 8);

#ifdef DEBUG_BLE
		size_t length = strlen((const char *)authChallengeQuery);
		char hexStringQuery[length * 2 + 1];							
		for (size_t i = 0; i < length; ++i){
			snprintf(&hexStringQuery[i * 2], 3, "%02X", authChallengeQuery[i]); 
		}
		ESP_LOGI(TAG, "authChallengeQuery=0x%s", hexStringQuery); 

		length = strlen((const char *)authChallengeReply);
		char hexStringReply[length * 2 + 1];		
		for (size_t i = 0; i < length; ++i){
			snprintf(&hexStringReply[i * 2], 3, "%02X", authChallengeReply[i]); 
		}
		ESP_LOGI(TAG, "authChallengeReply=0x%s", hexStringReply); 
#endif

		// Delay para dar tiempo a conectar
		// vTaskDelay(pdMS_TO_TICKS(250));
		modifyCharacteristic(authChallengeQuery, 8, AUTENTICACION_MATRIX_CHAR_HANDLE);
		ESP_LOGI(TAG, "Sending authentication");
		// vTaskDelay(pdMS_TO_TICKS(500));
		AuthTimer = xTaskGetTickCount();
	}
	AuthTimer = xTaskGetTickCount();
}

void deviceDisconnectInd ( void ){
	authSuccess = 0;
	memset(authChallengeQuery, 0x00, 8);
	memset(authChallengeReply , 0x00, 8);
	//memset(authChallengeQuery, 0x00, 8);
	modifyCharacteristic(authChallengeQuery, 8, AUTENTICACION_MATRIX_CHAR_HANDLE);
	vTaskDelay(pdMS_TO_TICKS(500));
}

uint8_t setAuthToken ( uint8_t *data, int len ){

#ifdef DEBUG_BLE
	size_t length = strlen((const char *)data); // Obtener la longitud de la cadena
	char hexString[length * 2 + 1];							  // Crear una cadena para almacenar los bytes en formato hexadecimal
	for (size_t i = 0; i < length; ++i){
		snprintf(&hexString[i * 2], 3, "%02X", data[i]); // Convertir cada byte a formato hexadecimal y agregarlo a la cadena
	}
	ESP_LOGI(TAG, "data recieved=0x%s", hexString); // Imprimir la cadena completa
#endif

	if(!authSuccess){
		if(!memcmp(authChallengeReply, data, 8) || !memcmp(deviceSerNum, initialSerNum, 10)){
			ESP_LOGI(TAG,"authChallengeReply igual a data received");

#ifdef DEBUG_BLE
			endTime = esp_timer_get_time();
			ESP_LOGI(TAG, "Tiempo de autorizacion: %llu microsegundos", (endTime - startTime));
#endif
			AuthTimer=0;
			ESP_LOGI(TAG,"Auth success");
			deviceBleConnected = true;
			authSuccess = 1;
		}
		else{
			size_t length = strlen((const char *)data); 
			char hexStringData[length * 2 + 1];				
			for (size_t i = 0; i < length; ++i){
				snprintf(&hexStringData[i * 2], 3, "%02X", data[i]); 
			}
			length = strlen((const char *)authChallengeReply); 
			char hexStringReply[length * 2 + 1];				
			for (size_t i = 0; i < length; ++i){
				snprintf(&hexStringReply[i * 2], 3, "%02X", authChallengeReply[i]); 
			}
			ESP_LOGE(TAG, "data received=0x%s distinto de authChallengeReply=0x%s", hexStringData, hexStringReply);
		}
	}
	return 1;
}

uint8_t authorizedOK ( void ){
	return (authSuccess || getMainFwUpdateActive() );
}

void updateCharacteristic(uint8_t* data, uint16_t len, uint16_t attrHandle){
	serverbleSetCharacteristic ( data,len,attrHandle);
	return;
}

void modifyCharacteristic(uint8* data, uint16 len, uint16 attrHandle){
	serverbleSetCharacteristic ( data,len,attrHandle);
	return;
}

void InitializeAuthsystem(){
	srand(aut_semilla);

	modifyCharacteristic(authChallengeQuery, 8, AUTENTICACION_MATRIX_CHAR_HANDLE);
}