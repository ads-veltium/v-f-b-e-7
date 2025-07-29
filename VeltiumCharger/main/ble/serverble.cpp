
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
static uint32_t UpdatefileSize;

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
uint8 err_code = 0;


TimerHandle_t xAdvertisingTimer;
bool advertisingFlag = false;
#define HOUR_TO_MS 3600000


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
	switch(handle){
		case STATUS_HPT_STATUS_CHAR_HANDLE: 
			pbleCharacteristics[BLE_CHA_HPT_STAT]->setValue(data, len);
			pbleCharacteristics[BLE_CHA_HPT_STAT]->notify();
			break;		
		case MEASURES_INST_CURRENT_CHAR_HANDLE: 
			pbleCharacteristics[BLE_CHA_INS_CURR]->setValue(data, len);
			pbleCharacteristics[BLE_CHA_INS_CURR]->notify();
			break;
#ifdef CONNECTED
		case MEASURES_INST_CURRENTB_CHAR_HANDLE: 
			pbleCharacteristics[BLE_CHA_INSB_CURR]->setValue(data, len);
			pbleCharacteristics[BLE_CHA_INSB_CURR]->notify();
			break;
		case MEASURES_INST_CURRENTC_CHAR_HANDLE: 
			pbleCharacteristics[BLE_CHA_INSC_CURR]->setValue(data, len);
			pbleCharacteristics[BLE_CHA_INSC_CURR]->notify();
			break;
		case CHARGING_GROUP_BLE_NET_DEVICES: 
			pbleCharacteristics[BLE_CHA_NET_GROUP]->setValue(data, len);
			pbleCharacteristics[BLE_CHA_NET_GROUP]->notify();
			break;
		case STATUS_COMS: 
			pbleCharacteristics[BLE_CHA_STATUS_COMS]->setValue(data, len);
			pbleCharacteristics[BLE_CHA_STATUS_COMS]->notify();
			break;
#endif
		case FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE:
			pbleCharacteristics[BLE_CHA_FW_DATA]->setValue(data, len);
			pbleCharacteristics[BLE_CHA_FW_DATA]->notify();
			break;
		default:
			// set characteristic value using selector mechanism
		    rcs_server_set_chr_value(handle, data, len);
            break;
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
		//static uint32_t UpdatefileSize;
		std::string rxValue = pCharacteristic->getValue();

		static uint8_t omnibus_packet_buffer[4 + RCS_CHR_OMNIBUS_SIZE];

		uint8_t* data = (uint8_t*)&rxValue[0];
		uint16_t dlen = (uint16_t)rxValue.length();

		if (pCharacteristic->getUUID().equals(blefields[SELECTOR].uuid)){
			// safety check: at least one byte for selector
			if (dlen < 1) {
				ESP_LOGW(TAG, "Error at write callback for Selector CHR: size less than 1");
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
				ESP_LOGW(TAG, "Error at write callback for Omnibus CHR: size less than 5");
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
				UpdateStatus.DescargandoArchivo = true;
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
					if(!SPIFFS.begin(1,"/spiffs",2,"PSOC5")){
						ESP_LOGE(TAG,"Problema en la inicialización SPIFFS /spiffs");
						SPIFFS.end();					
						SPIFFS.begin(1,"/spiffs",2,"PSOC5");
					}

					if(SPIFFS.exists(PSOC_UPDATE_FILE)){
						err_code = SPIFFS.remove(PSOC_UPDATE_FILE);
						if (err_code){
							ESP_LOGI(TAG,"Existe %s. Borrado con err_code %u",PSOC_UPDATE_FILE,err_code);
						}
						else {
							ESP_LOGE(TAG,"Error borrando %s",PSOC_UPDATE_FILE);
							SPIFFS.end();					
							SPIFFS.begin(1,"/spiffs",2,"PSOC5");
							SPIFFS.format();
						}
					}

					if(SPIFFS.exists(PSOC_UPDATE_OLD_FILE)){
						err_code = SPIFFS.remove(PSOC_UPDATE_OLD_FILE);
						if (err_code){
							ESP_LOGI(TAG,"Existe %s. Borrado con err_code %u",PSOC_UPDATE_OLD_FILE,err_code);
						}
						else{
							ESP_LOGE(TAG,"Error borrando %s",PSOC_UPDATE_OLD_FILE);
							SPIFFS.end();					
							SPIFFS.begin(1,"/spiffs",2,"PSOC5");
							SPIFFS.format();
						}
					}
					SPIFFS.begin(1,"/spiffs",2,"PSOC5");
					SPIFFS.format();

					UpdateFile = SPIFFS.open(PSOC_UPDATE_FILE, FILE_WRITE);

					if (!UpdateFile){
						ESP_LOGI(TAG,"!UpdateFile");

						ESP_LOGE(TAG,"No se pudo crear y abrir %s",PSOC_UPDATE_FILE);
					}
					uint8 open_file_retry_number=0;
					while (!UpdateFile && (open_file_retry_number++ <5)){
						ESP_LOGI(TAG,"Reintento %u de abrir %s",open_file_retry_number,PSOC_UPDATE_FILE);
						SPIFFS.end();					
						SPIFFS.begin(1,"/spiffs",2,"PSOC5");
						SPIFFS.format();
						UpdateFile = SPIFFS.open(PSOC_UPDATE_FILE, FILE_WRITE);
					}
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
				UpdateStatus.DescargandoArchivo = false;
				UpdateStatus.InstalandoArchivo = true;
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

					File InstalledFile = SPIFFS.open(PSOC_UPDATE_FILE, FILE_READ);

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
						SPIFFS.remove(PSOC_UPDATE_FILE);
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
				Configuracion.group_data.params[0] = ChargingGroup.Params.GroupMaster;

				if (ChargingGroup.Params.GroupActive && !payload[0]){
					ChargingGroup.StopOrder = true;
				}
				ChargingGroup.Params.GroupActive = payload[0];
				ChargingGroup.Params.CDP = payload[1];
				ChargingGroup.Params.ContractPower = payload[2] + payload[3] * 0x100;
				ChargingGroup.Params.potencia_max = payload[4] + payload[5] * 0x100;
				ChargingGroup.NewData = true;

				if(ChargingGroup.Params.GroupActive && !payload[0]){
					ChargingGroup.SendNewParams = true;
				}

#ifdef DEBUG_GROUPS
				Serial.printf("BLE received GROUPS_PARAMS=[");
				for (uint8 i=0; i<SIZE_OF_GROUP_PARAMS; i++){
					Serial.printf("%i",payload[i]);
					if (i<SIZE_OF_GROUP_PARAMS-1){
						Serial.printf(",");
					}
				}
				Serial.printf("]\n");
				Serial.printf("serverble - GROUPS_PARAMS: ChargingGroup.Params.GroupActive %i \n", ChargingGroup.Params.GroupActive);
				Serial.printf("serverble - GROUPS_PARAMS: ChargingGroup.Params.GroupMaster %i \n", ChargingGroup.Params.GroupMaster);
				Serial.printf("serverble - GROUPS_PARAMS: ChargingGroup.Params.potencia_max %i \n", ChargingGroup.Params.potencia_max);
				Serial.printf("serverble - GROUPS_PARAMS: ChargingGroup.Params.ContractPower %i \n", ChargingGroup.Params.ContractPower);
				Serial.printf("serverble - GROUPS_PARAMS: ChargingGroup.Params.CDP %i \n", ChargingGroup.Params.CDP);
				Serial.printf("serverble - GROUPS_PARAMS: ChargingGroup.Params.inst_max %i \n", ChargingGroup.Params.inst_max);
#endif
				memcpy(&Configuracion.group_data.params[1], payload,SIZE_OF_GROUP_PARAMS-1);
				Configuracion.Group_Guardar = true;
				delay (200);
				ChargingGroup.SendNewParams = true;
				return;
			}
			
			else if (handle == GROUPS_CIRCUITS) {
				uint8_t circuit_number = payload[0];
				size = circuit_number + 1;
#ifdef DEBUG_GROUPS
				Serial.printf("BLE received GROUPS_CIRCUITS=[");
				for (uint8 i = 0; i < size; i++){
					Serial.printf("%i", payload[i]);
					if (i < size - 1){
						Serial.printf(",");
					}
				}
				Serial.printf("]\n");

#endif
				ChargingGroup.Circuit_number = circuit_number;
				for (int i = 0; i < circuit_number; i++){
					Circuitos[i].numero = i + 1;
					Circuitos[i].Fases[0].numero = 1;
					Circuitos[i].Fases[1].numero = 2;
					Circuitos[i].Fases[2].numero = 3;
					Circuitos[i].limite_corriente = payload[i + 1];

#ifdef DEBUG_GROUPS
				Serial.printf("serverble - GROUPS_CIRCUITS: Circuito %i limite_corriente %i \n", i, Circuitos[i].limite_corriente);
#endif
				}
				ChargingGroup.NewData = true;
				memcpy(&Configuracion.group_data.circuits[0], payload, size);
				Configuracion.Group_Guardar = true;
				delay(200);
				ChargingGroup.SendNewCircuits = true;

				return;
			}

			else if (handle == GROUPS_OPERATIONS)
			{
				uint8_t operation = payload[0];
#ifdef DEBUG_GROUPS
				Serial.printf("BLE received GROUPS_OPERATIONS. operation=[%i]\n", operation);
#endif
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
#ifdef DEBUG_GROUPS
						Serial.printf("serverble - GROUPS_OPERATIONS: ChargingGroup.Conected %i \n", ChargingGroup.Conected);
						Serial.printf("serverble - GROUPS_OPERATIONS: ChargingGroup.Params.GroupMaster %i \n", ChargingGroup.Params.GroupMaster);
						Serial.printf("serverble - GROUPS_OPERATIONS: ChargingGroup.Params.GroupActive %i \n", ChargingGroup.Params.GroupActive);
						Serial.printf("serverble - GROUPS_OPERATIONS: ChargingGroup.Params.CDP %i \n", ChargingGroup.Params.CDP);
						Serial.printf("serverble - GROUPS_OPERATIONS: ChargingGroup.Params.potencia_max %i \n", ChargingGroup.Params.potencia_max);
						Serial.printf("serverble - GROUPS_OPERATIONS: ChargingGroup.Params.ContractPower %i \n", ChargingGroup.Params.ContractPower);
#endif
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
			ESP_LOGI(TAG,"Handle request = [0x%X]",handle);
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
			SendScheduleMatrixToPSOC5(data);
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
#ifdef DEBUG_UPDATE
			Serial.printf("Received FwData message with length and size %u %u\n", dlen, partSize);
#endif
			uint8_t* payload = new uint8_t[partSize];
			memset(payload, 0, partSize);
			memcpy(payload, &data[4], partSize);

			uint32_t partCRC32;
			memcpy(&partCRC32, &data[260], 4);
			
			uint32_t checksum=crc32(partSize, payload,0xFFFFFFFFUL);		
			if(!Testing){
				if(checksum!= partCRC32){
					ESP_LOGE(TAG,"checksum calculado=%u crc32 recibido=%u",checksum, partCRC32);
					successCode = 0x00000002;
					serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);
					delete[] payload;
					if(UpdateType == PSOC_UPDATE){
						UpdateFile.close();
						ESP_LOGI(TAG,"Borrando %s",PSOC_UPDATE_FILE);
						SPIFFS.remove(PSOC_UPDATE_FILE);
						SPIFFS.end();
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
						ESP_LOGE(TAG,"ESP update file writing error");
						successCode = 0x00000002;
					}
				#else
					if(UpdateFile.write(payload,partSize)!=partSize){
						ESP_LOGE(TAG,"ESP update file writing error");
						successCode = 0x00000002;
					}
				#endif
			}
			else if(UpdateType == PSOC_UPDATE){
				uint8 retry_number=0;
				uint16 write_result = UpdateFile.write(payload,partSize);
				ESP_LOGI(TAG,"Writing to file. partSize:%u - written:%u", partSize, write_result);
				while ((write_result != partSize)&&(retry_number++ < 15)){
						write_result = UpdateFile.write(payload,partSize);
						ESP_LOGW(TAG,"Reintento %i de write: %u", retry_number, write_result);
				}
				if(write_result!=partSize){
					ESP_LOGE(TAG,"Writing Error en %s",PSOC_UPDATE_FILE);
					successCode = 0x00000002;
					UpdateFile.close();
					ESP_LOGI(TAG,"Borrando %s",PSOC_UPDATE_FILE);
					SPIFFS.remove(PSOC_UPDATE_FILE);
					SPIFFS.end();
					UpdateStatus.DescargandoArchivo = false;	
				}
			}
			
			delete[] payload;

			
			serverbleNotCharacteristic((uint8_t*)&successCode, sizeof(successCode), FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE);

			return;
		}
#ifdef CONNECTED 
		if ( pCharacteristic->getUUID().equals(blefields[RCS_CHARGING_GROUP].uuid) ){

			uint8 size = uint8(data[0]);
#ifdef DEBUG_GROUPS
			Serial.printf("Me ha llegado un grupo con %i cargadores !\n", size);
			Serial.printf("Datos recibidos: [%s]\n", data);
#endif
			if(size< 10){
				sprintf(&Configuracion.group_data.group[0],"0%i",(char)size);
			}
			else{
				sprintf(&Configuracion.group_data.group[0],"%i",(char)size);
			}

			if(size>0){
				uint16_t i;
				for(i=0;i<size;i++){
					memcpy(&Configuracion.group_data.group[2+(i*9)],&data[1+(i*9)],8);
					Configuracion.group_data.group[10+(i*9)] = (char)data[9+(i*9)];
				}
				Configuracion.group_data.group[2+(i*9)] = '\0'; 
#ifdef DEBUG_GROUPS
				Serial.printf("serverble - Almacenamiento en flash del grupo. Buffer=[%s] Size=%u\n", Configuracion.group_data.group,size);
#endif
				Configuracion.Group_Guardar = true;
				delay(500);
			}
			else{
				memset(Configuracion.group_data.group, 0, MAX_GROUP_BUFFER_SIZE);
				sprintf(&Configuracion.group_data.group[0],"00_CLEAN");
				Configuracion.group_data.group[8]='\0';
				Configuracion.Group_Guardar = true;
				delay(500);
			}
			Get_Stored_Group_Data();
			delay(500);
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
	pAdvertising->setMaxInterval(320);
	pAdvertising->setMinInterval(160);
	pAdvertising->setMaxPreferred(24);
	pAdvertising->setMinPreferred(12);
	pAdvertising->addServiceUUID(BLEUUID((uint16_t)0xCD01));
	pAdvertising->start();
	
	return;
}

static uint8 advTimerCallback_counter = 0;

void vAdvertisingTimerCallback(TimerHandle_t xTimer) {
    xTimerStop(xAdvertisingTimer, 0); // Detener el timer
	xTimerReset(xAdvertisingTimer, 0); // Resetear el timer

	advTimerCallback_counter++; 
	
	if(advTimerCallback_counter >= 24)
	{
		advertisingFlag = true; // Activar el flag
		advTimerCallback_counter = 0;
	}
	else xTimerStart(xAdvertisingTimer, 0);
	ESP_LOGI(TAG, "Valor de AdvTimerCallback_counter: %d\n", advTimerCallback_counter);
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

	// Timer de 1 hora. Se itera con el 24 veces debido al mal funcionamiento del timer al utilizar un periodo mayor a 1 h.
    xAdvertisingTimer = xTimerCreate("AdvertisingTimer",
									pdMS_TO_TICKS(HOUR_TO_MS), // 1h
									pdFALSE,
									(void *)0,
									vAdvertisingTimerCallback);

	xTimerStart(xAdvertisingTimer, 0);
	ESP_LOGI(TAG, "Temporizador disparado. Periodo: %u ms\n", pdTICKS_TO_MS(xTimerGetPeriod(xAdvertisingTimer)));

	#ifdef DEBUG_BLE
	milestone("after creating serverbleTask");
	#endif
}

void serverbleStartAdvertising(void)
{
	ESP_LOGI(TAG, "start advertising...");
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
				ConexTimer = xTaskGetTickCount();
			}
		}

		//disconnected
		//if(!deviceBleConnected && !oldDeviceBleConnected)
		if(pServer->getConnectedCount()==0)
		{
			if (advertisingFlag) {
				ESP_LOGI(TAG, "Reinicio rutinario de advertising");

				pServer->getAdvertising()->stop();
				pServer->getAdvertising()->start();

				advertisingFlag = false; // Resetear el flag
				xTimerStart(xAdvertisingTimer, 0); // Iniciar el timer nuevamente
			}

			if (!ble_gap_adv_active())
			{
				ESP_LOGI(TAG, "Se ha detectado que el advertising está desactivado cuando no hay ningún dispositivo conectado. Activando advertising...");
				pServer->getAdvertising()->start();
			} 
		}
		
		// disconnecting
		if (!deviceBleConnected && oldDeviceBleConnected){
			ESP_LOGI(TAG, "Disconnectig");
			// vTaskDelay(500/portTICK_PERIOD_MS); // give the bluetooth stack the chance to get things ready
			// pServer->startAdvertising(); //Está activado por defcto advertiseOnDisconnect.
			Status.LastConn = xTaskGetTickCount();
			if (UpdateStatus.DescargandoArchivo){
				UpdateStatus.DescargandoArchivo = false;
				if (UpdateType == PSOC_UPDATE){
					xTaskCreate(RemoveUpdateFileTask,"REMOVE FILE", 4096*2, NULL, 1,NULL);
				}
			}
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
			ESP_LOGI(TAG, "Desconectando dispositivo BLE por inactividad de 10 minutos.");
			pServer->disconnect(Conn_Handle);
			deviceBleDisconnect= false;
		}
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
	//vTaskDelay(pdMS_TO_TICKS(500)); // ADS Eliminado 
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

/************************************************
 * Delete Update File Task
 * **********************************************/
void RemoveUpdateFileTask(void *arg){
	uint8 err_code;
	ESP_LOGI(TAG,"RemoveUpdateFileTask: Borrado de fichero de actualización");
	UpdateFile.close();
	if(SPIFFS.exists(PSOC_UPDATE_FILE)){
		err_code = SPIFFS.remove(PSOC_UPDATE_FILE);
		ESP_LOGI(TAG,"Existe %s. Borrado con err_code %u",PSOC_UPDATE_FILE,err_code);
	}
	SPIFFS.end();
	ESP.restart(); // Se reinicia para evitar problemas con el SPIFFS en caso de apagado. 
	vTaskDelete(NULL);
}