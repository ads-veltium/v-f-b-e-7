/* ========================================
 *
 * Copyright DRACO SYSTEMS , THE 2020 
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
 */
#include "control.h"
#include "helpers.h"

StaticTask_t xControlBuffer;
StaticTask_t xLEDBuffer;
StaticTask_t xUartBuffer;

static StackType_t xControlStack  [4096*6]     EXT_RAM_ATTR;
static StackType_t xLEDStack      [4096*2]     EXT_RAM_ATTR;
static StackType_t xUartStack     [4096*6]     EXT_RAM_ATTR;

static const char* TAG = "control.cpp";


#ifdef CONNECTED
static StackType_t xFirebaseStack [4096*6]     EXT_RAM_ATTR;
StaticTask_t xFirebaseBuffer ;
#endif

carac_Status   Status        EXT_RAM_ATTR;
carac_Update_Status UpdateStatus EXT_RAM_ATTR;

#ifdef CONNECTED

//Variables Firebase
carac_Comands  Comands       EXT_RAM_ATTR;
carac_Params   Params        EXT_RAM_ATTR;
carac_Coms     Coms          				 EXT_RAM_ATTR;
carac_Contador ContadorExt   				 EXT_RAM_ATTR;
carac_Firebase_Configuration ConfigFirebase  EXT_RAM_ATTR;
carac_circuito Circuitos  [MAX_GROUP_SIZE] 	 EXT_RAM_ATTR;
carac_group    ChargingGroup 				 EXT_RAM_ATTR;
carac_charger  charger_table  [MAX_GROUP_SIZE] EXT_RAM_ATTR;
carac_charger  temp_chargers[MAX_GROUP_SIZE] EXT_RAM_ATTR;
carac_Schedule Schedule                      EXT_RAM_ATTR;
uint8_t 	   temp_chargers_size 			 EXT_RAM_ATTR;
#endif

#ifdef DEMO_MODE_CURRENT
uint8_t FAKE_BASE_CURRENT = 20; // Valor medio FAKE de coriente del cargador. Ej= 20 Amperios
double FAKE_RANGE = 1.0; // Valor FAKE de oscilación de corriente +- sobre el BASE_CURRENT.
#endif

/* VARIABLES BLE */
uint8 device_ID[16] = {"VCD17010001"};
uint8 deviceSerNumFlash[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8 not_delete_group [4][10] = {{0xCD, 0x01, 0x21, 0x07, 0x16, 0x00, 0x00, 0x04, 0x98, 0x08},
								 {0xCD, 0x01, 0x21, 0x07, 0x16, 0x00, 0x00, 0x05, 0x29, 0x51},
								 {0xCD, 0x01, 0x21, 0x09, 0x16, 0x00, 0x00, 0x06, 0x14, 0x54},
								 {0xCD, 0x01, 0x21, 0x09, 0x16, 0x00, 0x00, 0x06, 0x14, 0x58}};
uint8 deviceSerNum[10] 		   = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};     //{0x00, 0x00, 0x00, 0x00, 0x0B, 0xCD, 0x17, 0x01, 0x00, 0x05};
TickType_t AuthTimer=0;
uint8 estado_inicial = 1;
uint8 estado_actual = ESTADO_ARRANQUE;
extern uint8 authSuccess;
uint8 user_index = 0;
extern int aut_semilla;
uint8 Zero = 0;
uint8 One = 1;
uint8 NextLine=0;
uint8 conexion_trigger=0;
uint8 user_index_arrived=0;

uint8 cnt_fin_bloque = 0;
int estado_recepcion = 0;
//uint8 buffer_tx_local[256]  EXT_RAM_ATTR;
uint8 buffer_rx_local[256]  EXT_RAM_ATTR;
uint8 record_received_buffer[550]    EXT_RAM_ATTR;
uint8 record_received_buffer_for_fb[550]    EXT_RAM_ATTR;

uint16 puntero_rx_local = 0;
uint32 TimeFromStart = 60; //tiempo para buscar el contador, hay que darle tiempo a conectarse
uint8 updateTaskrunning=0;

// initial serial number
uint8 initialSerNum[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8 mainFwUpdateActive = 0;
uint8 emergencyState = 0;

uint8 dispositivo_inicializado = 0;
uint8 PSOC_inicializado =0;
uint8 cnt_timeout_inicio = 0;
uint16 cnt_repeticiones_inicio = 500;	//1000;

uint8 status_hpt_anterior[2] = {'F','F' };
uint16 inst_current_anterior = 0x0000;
uint16 cnt_diferencia = 1;
uint8 HPT_estados[9][3] = {"0V", "A1", "A2", "B1", "B2", "C1", "C2", "E1", "F1"};

#ifdef IS_UNO_KUBO
uint8 ESP_version_firmware[11] = {"VBLE3_0614"};	   
#else
uint8 ESP_version_firmware[11] = {"VBLE0_0610"};	
#endif

uint8 PSOC_version_firmware[11] ;		
TickType_t Last_Block=0; 
uint8 systemStarted = 0;
uint8 Record_Num =4;
uint8 Bloqueo_de_carga = 1;
uint8 Bloqueo_de_carga_old = 1; // ADS - Anañido para reducir envíos al PSoC
bool Testing = false;
int TimeoutMainDisconnect = 0;
extern bool deviceBleDisconnect;
uint8 psocUpdateCounter = 0;

void startSystem(void);
void modifyCharacteristic(uint8* data, uint16 len, uint16 attrHandle);


#ifdef CONNECTED
void broadcast_a_grupo(char* Mensaje, uint16_t size);
uint8_t check_in_group(const char* ID, carac_charger* group,uint8_t size);
bool remove_from_group(const char* ID ,carac_charger* group, uint8_t *size);
bool add_to_group(const char* ID, IPAddress IP, carac_charger* group, uint8_t *size);
IPAddress get_IP(const char* ID);
#endif

// Variables registros de carga
boolean new_record_received;
boolean record_pending_for_write;
boolean ask_for_new_record;
uint8_t record_index;
uint8_t record_lap;
uint8_t last_record_in_mem;
uint8_t last_record_lap;
TickType_t ask_for_new_record_start_timeout = 0;
boolean records_trigger = false;

uint8_t update_flag = 0;

/*******************************************************************************
 * Rutina de atencion a inerrupcion de timer (10mS)
 ********************************************************************************/

hw_timer_t * timer10ms = NULL;
HardwareSerialMOD serialLocal(2);
unsigned long StartTime = millis();

void IRAM_ATTR onTimer10ms() 
{
	if ( cnt_fin_bloque )
	{
		--cnt_fin_bloque;
	}
}

void configTimers ( void )
{
	//1 tick take 1/(80MHZ/80) = 1us so we set divider 80 and count up
	timer10ms = timerBegin(0, 100, true);
	timerAttachInterrupt(timer10ms, &onTimer10ms, true);
	// every 10 millis
	timerAlarmWrite(timer10ms, 10000, true);
	timerAlarmEnable(timer10ms);
}

void MAIN_RESET_Write( uint8_t val )
{
	DRACO_GPIO_Reset_MCU(val);
	CyBtldr_EndBootloadOperation(&serialLocal);
	return;
}

void controlTask(void *arg) {		
	
	// Inicia el timer de 10mS y 1 segundo
	configTimers();
	bool LastUserCon = false;
	bool Iface_Con = BLE;
	uint8 old_inicializado = 0;
	serialLocal.begin(115200, SERIAL_8N1, 34, 4); // pins: rx, tx

	// INICIALIZO ELEMENTOS PARA AUTENTICACION
	InitializeAuthsystem();

	MAIN_RESET_Write(1);        // Permito el arranque del micro principal

	while (1){
		if (mainFwUpdateActive == 0 || emergencyState == 1){
			switch (estado_actual){

			case ESTADO_ARRANQUE:
				if (!PSOC_inicializado){
					if (--cnt_timeout_inicio == 0){
						SendToPSOC5(0, BLOQUE_INICIALIZACION);
						cnt_timeout_inicio = TIMEOUT_INICIO;

						if (--cnt_repeticiones_inicio == 0){
							cnt_repeticiones_inicio = 750; // 1000;
							Configuracion.data.count_reinicios_malos++;
							if (Configuracion.data.count_reinicios_malos > 10){
#ifdef DEBUG
								printf("Entramos en modo EMERGENCIA!!\n");
#endif
								memcpy(device_ID, Configuracion.data.device_ID, sizeof(Configuracion.data.device_ID));
								memcpy(deviceSerNum, deviceSerNumFlash, sizeof(deviceSerNum));
								convertSN();
								changeAdvName(device_ID);
								dev_auth_init((void const *)&deviceSerNumFlash);
								std::string sversion = std::to_string(Configuracion.data.FirmwarePSOC);
								std::string svelt = std::to_string(Configuracion.data.velt_v);
								std::string version_velt = "VELT" + svelt + "_0" + sversion;

								uint8 version_psoc[version_velt.length()];

								memcpy(version_psoc, version_velt.data(), version_velt.length());

								modifyCharacteristic(version_psoc, 10, VERSIONES_VERSION_FIRMWARE_CHAR_HANDLE);
								modifyCharacteristic(ESP_version_firmware, 10, VERSIONES_VERSION_FIRM_BLE_CHAR_HANDLE);

								estado_actual = ESTADO_NORMAL;
								dispositivo_inicializado = 2;
								emergencyState = 1;
							}
							else{
								setMainFwUpdateActive(1);
								updateTaskrunning = 1;
								Serial.println("Enviando firmware al PSOC5 por falta de comunicacion!");
								
								xTaskCreate(UpdateTask, "TASK UPDATE", 4096 * 5, NULL, 5, NULL);
							}
						}
					}
				}
				else{
					serialLocal.flush();
					estado_actual = ESTADO_INICIALIZACION;
				}
				break;

			case ESTADO_INICIALIZACION:{
				uint8_t data[2] = {0};
#ifdef IS_UNO_KUBO
				data[0] = serverbleGetConnected() || ConfigFirebase.ClientAuthenticated;
#else
				data[0] = serverbleGetConnected();
#endif
				data[1] = dispositivo_inicializado;

				SendToPSOC5(data, 2, BLOQUE_STATUS);
				estado_actual = ESTADO_NORMAL;
				break;
			}

			case ESTADO_NORMAL:
				// Alguien se está intentando conectar
				if (AuthTimer != 0){
					uint32_t Transcurrido = pdTICKS_TO_MS(xTaskGetTickCount() - AuthTimer);
					if (Transcurrido > DEFAULT_BLE_AUTH_TIMEOUT && !authSuccess){
						AuthTimer = 0;
						ESP_LOGI(TAG, "Desconexion por autentenicacion fallida");
						deviceBleDisconnect = true;
					}
				}

				if (serverbleGetConnected()){
					Iface_Con = BLE;
				}
#ifdef IS_UNO_KUBO
				else if (ConfigFirebase.ClientAuthenticated){
					Iface_Con = COMS;
				}
#endif
				if (Testing){
					uint8_t data[2] = {0};
					data[0] = 1;
					data[1] = 4;
					SendToPSOC5(data, 2, BLOQUE_STATUS);
					LastUserCon = 1;
				}

				if ((Iface_Con == BLE && LastUserCon != serverbleGetConnected()) || old_inicializado != dispositivo_inicializado){
					LastUserCon = serverbleGetConnected();
					old_inicializado = dispositivo_inicializado;
					conexion_trigger = 1;
				}

				if (conexion_trigger && user_index_arrived){
					SendStatusToPSOC5(serverbleGetConnected(), dispositivo_inicializado, 0);
					conexion_trigger = 0;
					user_index_arrived = 0;
				}
#ifdef CONNECTED
				else if (Iface_Con == COMS && LastUserCon != ConfigFirebase.ClientAuthenticated){
					SendStatusToPSOC5(ConfigFirebase.ClientAuthenticated, dispositivo_inicializado, 1);
					LastUserCon = ConfigFirebase.ClientAuthenticated;
				}

				if (Comands.start){
					SendToPSOC5(One, CHARGING_BLE_MANUAL_START_CHAR_HANDLE);
					Comands.start = 0;
#ifdef DEBUG
					Serial.println("Sending START");
#endif
				}

				else if (Comands.stop){
					SendToPSOC5(One, CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE);
#ifdef DEBUG
					Serial.println("Sending STOP");
#endif
				}

				else if (Comands.Newdata){
#ifdef DEBUG
					Serial.printf("Enviando corriente %i\n", Comands.desired_current);
#endif
					SendToPSOC5(Comands.desired_current, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
				}

				else if (Comands.reset){
#ifdef DEBUG
					Serial.println("Reiniciando por comando externo! No preocuparse");
#endif
					MAIN_RESET_Write(0);
					delay(500);
					ESP.restart();
				}

				if (ConfigFirebase.InternetConection && new_record_received){
					new_record_received = false;
					if (!serverbleGetConnected()){
						record_pending_for_write = true;
					}
				}

				if (records_trigger){
					records_trigger = false;
					record_index = last_record_in_mem;
					record_lap = last_record_lap;
					ask_for_new_record = true;
				}

				if (ask_for_new_record && (xTaskGetTickCount()-ask_for_new_record_start_timeout>500)){
					uint8_t buffer[2];
					buffer[0] = (uint8)(record_index & 0x00FF);
					buffer[1] = (uint8)((record_index >> 8) & 0x00FF);
#ifdef DEBUG
					ESP_LOGI(TAG,"Pidiendo registro %i al PSoC", record_index);
#endif
					SendToPSOC5(buffer, 2, RECORDING_REC_INDEX_CHAR_HANDLE);
					ask_for_new_record_start_timeout = xTaskGetTickCount();
				}
#endif
				if (++TimeoutMainDisconnect > 100000){
#ifdef DEBUG
					Serial.println("Main reset detected");
#endif
					MAIN_RESET_Write(0);
					ESP.restart();
				}

				break;
			default:
				break;
			}
		}
		else{
			if (!updateTaskrunning && psocUpdateCounter<=100){
				// Poner el micro principal en modo bootload

				ESP_LOGI(TAG, "Envio actualización al PSOC");
				SendToPSOC5(Zero, BOOT_LOADER_LOAD_SW_APP_CHAR_HANDLE);

				psocUpdateCounter++;
			}
			else if(psocUpdateCounter>100)
			{
				ESP_LOGW(TAG, "ERROR: RESPUESTA DEL PSOC NO RECIBIDA, REINICIANDO...");
				psocUpdateCounter=0;
				Status.error_code = 0x60;
				vTaskDelay(pdMS_TO_TICKS(5000));

				MAIN_RESET_Write(0);
				delay(500);
				ESP.restart();
			}
		}

		delay(50);
	}
}

void startSystem(void){

#ifdef DEBUG
	Serial.printf("startSystem(): device SN = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",
				  deviceSerNum[0], deviceSerNum[1], deviceSerNum[2], deviceSerNum[3], deviceSerNum[4],
				  deviceSerNum[5], deviceSerNum[6], deviceSerNum[7], deviceSerNum[8], deviceSerNum[9]);
#endif
	dev_auth_init((void const*)&deviceSerNum);

	Configuracion.data.FirmwareESP  = ParseFirmwareVersion((char *)(ESP_version_firmware));
	Configuracion.data.FirmwarePSOC = ParseFirmwareVersion((char *)(PSOC_version_firmware));
	
	memcpy(Configuracion.data.device_ID,device_ID,sizeof(Configuracion.data.device_ID));

	sprintf(Configuracion.data.deviceSerNum,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		deviceSerNum[0], deviceSerNum[1], deviceSerNum[2], deviceSerNum[3], deviceSerNum[4],
		deviceSerNum[5], deviceSerNum[6], deviceSerNum[7], deviceSerNum[8], deviceSerNum[9]);

	
	#ifdef CONNECTED
		//Get Device FirebaseDB ID
		sprintf(ConfigFirebase.Device_Db_ID,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		deviceSerNum[0], deviceSerNum[1], deviceSerNum[2], deviceSerNum[3], deviceSerNum[4],
		deviceSerNum[5], deviceSerNum[6], deviceSerNum[7], deviceSerNum[8], deviceSerNum[9]);

		memcpy(ConfigFirebase.Device_Ser_num,ConfigFirebase.Device_Db_ID,20);

		memcpy(&ConfigFirebase.Device_Db_ID[20],&device_ID[3],8);
		memcpy(ConfigFirebase.Device_Id,&device_ID[3],8);

		//compatibilidad con versiones anteriores de firmwware
		if(Configuracion.data.FirmwarePSOC <= 510){
			dispositivo_inicializado = 2;
		}

		Coms.StartConnection = true;
	#endif
}

void proceso_recepcion(void *arg){

	uint8 byte;
	uint16 longitud_bloque = 0, len;
	uint16 tipo_bloque = 0x0000;

	for(;;){
		delay(updateTaskrunning ? 1000:5);
		if(!updateTaskrunning){
			uint8_t ui8Tmp;
			len = serialLocal.available();
			
			if(len == 0){
				if(cnt_fin_bloque == 0){
					puntero_rx_local = 0;
					estado_recepcion = ESPERANDO_HEADER;
				}
				continue;
			}
			
			cnt_fin_bloque = TIMEOUT_RX_BLOQUE;
			
			switch(estado_recepcion){
				case ESPERANDO_HEADER:
					serialLocal.read(&byte, 1);
					if(byte != HEADER_RX)
						break;
					estado_recepcion = ESPERANDO_TIPO;
					if(--len == 0)
						break;
					break;
				case ESPERANDO_TIPO:
					if(len < 2)
						break;
					serialLocal.read(&ui8Tmp,1);
					tipo_bloque = 256 * ui8Tmp;
					serialLocal.read(&ui8Tmp,1);
					tipo_bloque += ui8Tmp;
					estado_recepcion = ESPERANDO_LONGITUD;
					len -= 2;
					if(len == 0)
						break;
					break;
				case ESPERANDO_LONGITUD:
					serialLocal.read(&ui8Tmp,1);
					longitud_bloque = ui8Tmp;
					if(longitud_bloque == 0)
						longitud_bloque = 256;
					puntero_rx_local = 0;
					estado_recepcion = RECIBIENDO_BLOQUE;
					if(--len == 0)
						break;
					break;
				case RECIBIENDO_BLOQUE:
					do{
						if(longitud_bloque > 0)
						{
							serialLocal.read(&buffer_rx_local[puntero_rx_local],1);
							puntero_rx_local++;
							if(--longitud_bloque == 0)
							{
								procesar_bloque(tipo_bloque);
								puntero_rx_local = 0;
								estado_recepcion = ESPERANDO_HEADER;
								break;
							}
						}
						else
						{
							serialLocal.read(&byte, 1);
						}
						len--;							
					}while(len >0);						
					break;
				default:
					puntero_rx_local = 0;
					estado_recepcion = ESPERANDO_HEADER;
					break;
			}
		}
	}
}

void procesar_bloque(uint16 tipo_bloque){
#ifdef DEBUG_RX_UART
		size_t buffer_size=sizeof(buffer_rx_local);
		size_t pos=0;
		char buffer_debug[1024];
		ESP_LOGI(TAG,"Received UART block. Type=[0x%X] Buffer lentgh=[%i]",tipo_bloque,buffer_size);
		for (size_t i = 0; i < buffer_size; i++) {
    		pos += snprintf(buffer_debug + pos, sizeof(buffer_debug) - pos, "%d ", buffer_rx_local[i]);
    		if (pos > sizeof(buffer_debug)) {
        		ESP_LOGE(TAG, "El buffer_debug es demasiado pequeño para contener todos los datos. %i",pos);
        		break;
    		}
		}
		if (pos > 0 && buffer_debug[pos - 1] == ' ') {
    		buffer_debug[pos - 1] = '\0'; 
		}
		ESP_LOGI(TAG,"Received UART DATA=[%s]", buffer_debug);
#endif

	switch(tipo_bloque){	
		case BLOQUE_INICIALIZACION:
			if (!systemStarted && (buffer_rx_local[239]==0x36 || buffer_rx_local[238]==0x36)) { //Compatibilidad con 509
				memcpy(device_ID, buffer_rx_local, 11);
				changeAdvName(device_ID);
				modifyCharacteristic(device_ID, 11, VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[11], 1, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[12], 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[18], 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[24], 1, CHARGING_INSTANT_DELAYED_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[25], 1, CHARGING_START_STOP_START_MODE_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[26], 168, SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[194], 1, VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[195], 1, VCD_NAME_USERS_USER_TYPE_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[196], 1, VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE);
				user_index = buffer_rx_local[196];
				modifyCharacteristic(&buffer_rx_local[197], 10, VERSIONES_VERSION_FIRMWARE_CHAR_HANDLE);
				memcpy(PSOC_version_firmware, &buffer_rx_local[197],10);
				
				//Guardamos la versión de CPU del psoc en flash del ESP32
				Configuracion.data.velt_v = PSOC_version_firmware[4]-48;

				modifyCharacteristic(ESP_version_firmware, 10, VERSIONES_VERSION_FIRM_BLE_CHAR_HANDLE);

				modifyCharacteristic(&buffer_rx_local[207], 2, RECORDING_REC_CAPACITY_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[209], 2, RECORDING_REC_LAST_CHAR_HANDLE);
				last_record_in_mem = (buffer_rx_local[209] + buffer_rx_local[210]*0x100);
				ESP_LOGI(TAG,"Last record in mem. Index = %i",last_record_in_mem);
				record_index = last_record_in_mem;
				modifyCharacteristic(&buffer_rx_local[211], 1, RECORDING_REC_LAPS_CHAR_HANDLE);
				last_record_lap = buffer_rx_local[211];
				ESP_LOGI(TAG,"Last record in mem. Lap = %i",last_record_lap);
				record_lap = last_record_lap;
				
				//comprobar si el dato tiene sentido, sino cargar el de mi memoria
				if(memcmp("AA", &buffer_rx_local[212],2) && memcmp("WA", &buffer_rx_local[212],2) && memcmp("MA", &buffer_rx_local[212],2)){
					if(memcmp("AA", Configuracion.data.autentication_mode,2) && memcmp("WA", Configuracion.data.autentication_mode,2) && memcmp("MA", Configuracion.data.autentication_mode,2)){
						memcpy(&buffer_rx_local[212],Configuracion.data.autentication_mode,2);
					}
					else{
						//Si ninguno de los dos es correcto, lo damos por malo y ponemos el de defecto
						memcpy(&buffer_rx_local[212],"WA",2);
						SendToPSOC5(&buffer_rx_local[212],2,CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
						break;
					}
				}
				else{
					memcpy(Configuracion.data.autentication_mode, &buffer_rx_local[212],2);
				}

				modifyCharacteristic(&buffer_rx_local[212], 2, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);

				memcpy(deviceSerNum, &buffer_rx_local[219], 10);			
				modifyCharacteristic(&buffer_rx_local[229], 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P1_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[231], 1, LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[232], 1, DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[233], 1, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[234], 2, COMS_FW_UPDATEMODE_CHAR_HANDLE);		
				modifyCharacteristic(&buffer_rx_local[241], 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P2_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[243], 2, TIME_DATE_COUNTRY_CHAR_HANDLE);			

				/************************ Set configuration data **********************/
				Configuracion.data.potencia_contratada1 = buffer_rx_local[229]+buffer_rx_local[230]*0x100;
				Configuracion.data.potencia_contratada2 = buffer_rx_local[241]+buffer_rx_local[242]*0x100;
				Configuracion.data.install_current_limit   = buffer_rx_local[11];	
				Configuracion.data.CDP   = buffer_rx_local[232];		

				#ifdef CONNECTED	
					/************************ Set firebase Params **********************/
					memcpy(Params.autentication_mode, &buffer_rx_local[212],2);
					Params.install_current_limit = buffer_rx_local[11];
					Params.potencia_contratada1 = buffer_rx_local[229]+buffer_rx_local[230]*0x100;
					Params.potencia_contratada2 = buffer_rx_local[241]+buffer_rx_local[242]*0x100;
					Params.CDP 	  =  buffer_rx_local[232];

					if((buffer_rx_local[232] >> 1) & 0x01){
						Params.Tipo_Sensor    = (buffer_rx_local[232]  >> 4)& 0x01;
						SendToPSOC5(1, SEARCH_EXTERNAL_METER);
					}
					else{
						Params.Tipo_Sensor    = 0;
					}

					if(((buffer_rx_local[0] >> 5) & 0x03) > 0){
						Status.Photovoltaic=true;
					}else{
						Status.Photovoltaic=false;
					}
					
					if(Params.Tipo_Sensor && !ContadorExt.MedidorConectado){
						Coms.ETH.medidor = true;
						Update_Status_Coms(0,MED_BUSCANDO_GATEWAY);
						Bloqueo_de_carga = 1;
						SendToPSOC5(Bloqueo_de_carga,BLOQUEO_CARGA);
#ifdef DEBUG_GROUPS
						printf("procesar_bloque - BLOQUE_INICIALIZACION: Bloqueo_de_carga = %i enviado al PSoC\n", Bloqueo_de_carga);
#endif
					}
					
					ESP_LOGI(TAG,"Params.Fw_Update_mode=%s",Params.Fw_Update_mode);
					memcpy(Params.Fw_Update_mode, &buffer_rx_local[234],2);
					Params.Fw_Update_mode[2]='\0';
					ESP_LOGI(TAG,"Params.Fw_Update_mode=%s",Params.Fw_Update_mode);
					modifyCharacteristic(&buffer_rx_local[234], 2, COMS_FW_UPDATEMODE_CHAR_HANDLE);
					Comands.desired_current = buffer_rx_local[233];
					Coms.Wifi.ON = buffer_rx_local[236];
					Coms.ETH.ON = buffer_rx_local[237];	
					modifyCharacteristic(&buffer_rx_local[236], 1, COMS_CONFIGURATION_WIFI_ON);

#endif

				startSystem();
				systemStarted = 1;

				cnt_timeout_inicio = TIMEOUT_INICIO;
				PSOC_inicializado = 1;

				#ifdef CONNECTED
					if(!Configuracion.data.Data_cleared){
						bool borrar = true;
						for(uint8_t i=0;i<4; i++){
							//Comprobar si tengo que mantener mis datos o borrarlos
							if(!memcmp(not_delete_group[i], deviceSerNum,10)){
								borrar = false;
								break;
							}
						}
						
						if(borrar){
							//Borrar datos de los grupos
							SendToPSOC5(41,CLEAR_FLASH_SPACE);
							SendToPSOC5(45,CLEAR_FLASH_SPACE);
							SendToPSOC5(3,CLEAR_FLASH_SPACE);
							SendToPSOC5(6,CLEAR_FLASH_SPACE);
						}


						//Borrar datos del apn
						SendToPSOC5(46,CLEAR_FLASH_SPACE);
						SendToPSOC5(47,CLEAR_FLASH_SPACE);
						SendToPSOC5(48,CLEAR_FLASH_SPACE);
						SendToPSOC5(23,CLEAR_FLASH_SPACE);
						SendToPSOC5(24,CLEAR_FLASH_SPACE);
					}
				#endif

			}
		break;

		case BLOQUE_STATUS:
#ifdef CONNECTED
			if(buffer_rx_local[45]==0x36 && systemStarted){
#else
			if(buffer_rx_local[25]==0x36 && systemStarted){
#endif			
				//Comprobar HPT
				bool valido = false;
				for(int i=0;i<9;i++){
					if(!memcmp(&buffer_rx_local[1], HPT_estados[i], 2)){
						valido=true;
						break;
					}
				}
				if(!valido){
					break;
				}
				
				TimeoutMainDisconnect = 0;	
				//Leds
				modifyCharacteristic(buffer_rx_local, 1, LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE);

				//Pasar datos a Status
				memcpy(Status.HPT_status, &buffer_rx_local[1], 2);
				Status.HPT_status[2]='\0';            
				//Hilo piloto
				if((memcmp(&buffer_rx_local[1], status_hpt_anterior, 2) != 0)){
					
					if(dispositivo_inicializado !=2){
						SendToPSOC5(1,BLOQUE_APN);
						delay(150);
						dispositivo_inicializado = 2;
						Configuracion.data.count_reinicios_malos = 0;
					}

#ifdef DEBUG
					Serial.printf("Hilo piloto de %c %c a %c %c\n", status_hpt_anterior[0], status_hpt_anterior[1], buffer_rx_local[1], buffer_rx_local[2]);
#endif

					modifyCharacteristic(&buffer_rx_local[1], 2, STATUS_HPT_STATUS_CHAR_HANDLE);
					if(serverbleGetConnected()){
						if(buffer_rx_local[1]!= 'E' && buffer_rx_local[1]!= 'F'){
							Bloqueo_de_carga = true;
							serverbleNotCharacteristic(&buffer_rx_local[1], 2, STATUS_HPT_STATUS_CHAR_HANDLE); 
						}
						else{
							serverbleNotCharacteristic(&buffer_rx_local[1], 1, STATUS_HPT_STATUS_CHAR_HANDLE); 
						}
					}
#ifdef CONNECTED
					ConfigFirebase.WriteStatus = true;
					if (!memcmp(&buffer_rx_local[1], "C",1) || !memcmp(status_hpt_anterior, "B",1)){
						ConfigFirebase.WriteUser = true;
						ConfigFirebase.ResetUser = false;
					}

					if (!memcmp(&buffer_rx_local[1], "A",1) || !memcmp(status_hpt_anterior, "0",1)){
						ChargingGroup.ChargPerm = false;
						ChargingGroup.AskPerm = false;
					}

					//Si sale de C2 bloquear la siguiente carga
					if(memcmp(&buffer_rx_local[1],"C2",2) && memcmp(&buffer_rx_local[1],"B2",2)){
						if(Params.Tipo_Sensor || ChargingGroup.Conected){
							if(!memcmp(status_hpt_anterior, "C",1) || !memcmp(status_hpt_anterior, "B",1)){
								Bloqueo_de_carga = true;
								ChargingGroup.ChargPerm = false;
								ChargingGroup.AskPerm = false;
								SendToPSOC5(Bloqueo_de_carga, BLOQUEO_CARGA);
#ifdef DEBUG_GROUPS
								printf("procesar_bloque - BLOQUE_STATUS: Bloqueo_de_carga = %i enviado al PSoC\n", Bloqueo_de_carga);
#endif
							}				
							
						}				
					}
#endif
					memcpy(status_hpt_anterior, &buffer_rx_local[1], 2);
				}

				//Medidas
				modifyCharacteristic(&buffer_rx_local[3],  2, STATUS_ICP_STATUS_CHAR_HANDLE);						
				modifyCharacteristic(&buffer_rx_local[5],  2, STATUS_RCD_STATUS_CHAR_HANDLE);			
				modifyCharacteristic(&buffer_rx_local[7], 2, STATUS_CONN_LOCK_STATUS_CHAR_HANDLE);			
				modifyCharacteristic(&buffer_rx_local[9], 1, MEASURES_MAX_CURRENT_CABLE_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[10], 2, DOMESTIC_CONSUMPTION_DOMESTIC_CURRENT_CHAR_HANDLE);
				Status.Measures.consumo_domestico = buffer_rx_local[10] + (buffer_rx_local[11] * 0x100);
				modifyCharacteristic(&buffer_rx_local[12], 1, DOMESTIC_CONSUMPTION_REAL_CURRENT_LIMIT_CHAR_HANDLE);
				Status.current_limit = buffer_rx_local[12];
				modifyCharacteristic(&buffer_rx_local[13], 1, ERROR_STATUS_ERROR_CODE_CHAR_HANDLE);	

				//FaseA	
				Status.Measures.instant_current = buffer_rx_local[14] + (buffer_rx_local[15] * 0x100);
#ifdef DEMO_MODE_CURRENT
				// ADS - Añadido para trucar la corriente. 
				// La corriente leida es igual a la consigna enviada menos un valor aleatorio entre 0 y FAKE_RANGE(1)
				FAKE_BASE_CURRENT = Comands.desired_current;
				uint32_t random_value = esp_random();
				double normalized_value = static_cast<double>(random_value)/static_cast<double>(UINT32_MAX);
				double scaled_value = normalized_value - FAKE_RANGE;
				double random_current = (static_cast<double>(FAKE_BASE_CURRENT) + scaled_value);
        		if(!memcmp(Status.HPT_status, "C2",2)){
					Status.Measures.instant_current = 100*random_current;
				}
				else {
					Status.Measures.instant_current = 0;
				}
#endif
				if(((Status.Measures.instant_current) != (inst_current_anterior)) && (serverbleGetConnected())&& (--cnt_diferencia == 0))
				{
					modifyCharacteristic(&buffer_rx_local[14], 2, MEASURES_INST_CURRENT_CHAR_HANDLE);
					cnt_diferencia = 2; // A.D.S. Cambiado de 30 a 5
					inst_current_anterior = Status.Measures.instant_current;
					serverbleNotCharacteristic(&buffer_rx_local[14], 2, MEASURES_INST_CURRENT_CHAR_HANDLE); 
#ifdef CONNECTED
					if(Status.Trifasico){
						serverbleNotCharacteristic(&buffer_rx_local[24],2,MEASURES_INST_CURRENTB_CHAR_HANDLE);
						serverbleNotCharacteristic(&buffer_rx_local[34],2,MEASURES_INST_CURRENTC_CHAR_HANDLE);
					}
					ConfigFirebase.WriteStatus = true;
#endif
				}

				modifyCharacteristic(&buffer_rx_local[16], 2, MEASURES_INST_VOLTAGE_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[18], 2, MEASURES_ACTIVE_POWER_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[20], 4, MEASURES_ACTIVE_ENERGY_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[46], 4, PHOTOVOLTAIC_TOTAL_POWER);
				modifyCharacteristic(&buffer_rx_local[50], 4, PHOTOVOLTAIC_NET_POWER);
				modifyCharacteristic(&buffer_rx_local[54], 1, MEASURES_APPARENT_POWER_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[55], 4, EXTERNAL_METER_POWER);
					
					Status.error_code = buffer_rx_local[13];
					
#ifdef CONNECTED					
					Status.ICP_status 		= (memcmp(&buffer_rx_local[3],  "CL" ,2) == 0 )? true : false;
					Status.DC_Leack_status  = (memcmp(&buffer_rx_local[5], "TR" ,2) >  0 )? true : false;
					Status.Con_Lock   		= (memcmp(&buffer_rx_local[7], "UL",3) == 0 )? true : false;			

					Status.Measures.max_current_cable = buffer_rx_local[9];					
					Status.Measures.instant_voltage   = buffer_rx_local[16] + (buffer_rx_local[17] * 0x100);
					Status.Measures.active_power      = buffer_rx_local[18] + (buffer_rx_local[19] * 0x100);
					Status.Measures.active_energy     = buffer_rx_local[20] + (buffer_rx_local[21] * 0x100) +(buffer_rx_local[22] * 0x1000) +(buffer_rx_local[23] * 0x10000);

					int8_t buffer_total[4];
					int8_t buffer_net[4];
					int8_t buffer_external_meter[4];

					buffer_total[0] = buffer_rx_local[46];
					buffer_total[1] = buffer_rx_local[47];
					buffer_total[2] = buffer_rx_local[48];
					buffer_total[3] = buffer_rx_local[49];

					buffer_net[0] = buffer_rx_local[50];
					buffer_net[1] = buffer_rx_local[51];
					buffer_net[2] = buffer_rx_local[52];
					buffer_net[3] = buffer_rx_local[53];
					
					buffer_external_meter[0] = buffer_rx_local[55];
					buffer_external_meter[1] = buffer_rx_local[56];
					buffer_external_meter[2] = buffer_rx_local[57];
					buffer_external_meter[3] = buffer_rx_local[58];

					memcpy(&Status.total_power,buffer_total,4);
					memcpy(&Status.net_power,buffer_net,4);
					memcpy(&Status.external_meter_power,buffer_external_meter,4);
					
					ContadorExt.Power=Status.external_meter_power;  // Copiamos el valor de potencia recibido del medidor
#ifdef DEMO_MODE_CURRENT
					ContadorExt.Power=1000*ContadorExt.Power;
#endif
					
#ifdef DEBUG_MEDIDOR
					ESP_LOGI(TAG,"Potencia del contador recibida del PSoC=%i",Status.external_meter_power);
#endif

					Status.Trifasico= buffer_rx_local[44]==3;
					
					if(Status.Trifasico){
						Status.MeasuresB.instant_current = buffer_rx_local[24] + (buffer_rx_local[25] * 0x100);
						Status.MeasuresB.instant_voltage = buffer_rx_local[26] + (buffer_rx_local[27] * 0x100);
						Status.MeasuresB.active_power    = buffer_rx_local[28] + (buffer_rx_local[29] * 0x100);
						Status.MeasuresB.active_energy   = buffer_rx_local[30] + (buffer_rx_local[31] * 0x100) +(buffer_rx_local[32] * 0x1000) +(buffer_rx_local[33] * 0x10000);


						Status.MeasuresC.instant_current = buffer_rx_local[34] + (buffer_rx_local[35] * 0x100);
						Status.MeasuresC.instant_voltage = buffer_rx_local[36] + (buffer_rx_local[37] * 0x100);
						Status.MeasuresC.active_power    = buffer_rx_local[38] + (buffer_rx_local[39] * 0x100);
						Status.MeasuresC.active_energy   = buffer_rx_local[40] + (buffer_rx_local[41] * 0x100) +(buffer_rx_local[42] * 0x1000) +(buffer_rx_local[43] * 0x10000);

					}
#endif
			}
		break;
		case BLOQUE_DATE_TIME:{
			modifyCharacteristic(buffer_rx_local, 6, TIME_DATE_DATE_TIME_CHAR_HANDLE);
			Status.Time.hora = buffer_rx_local[3];
			aut_semilla = (((int)buffer_rx_local[4]) * 0x100) + (int)buffer_rx_local[5];
			modifyCharacteristic(&buffer_rx_local[6], 6, TIME_DATE_CONNECTION_DATE_TIME_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[12], 6, TIME_DATE_DISCONNECTION_DATE_TIME_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[18], 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[24], 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);
#ifdef CONNECTED
			int TimeStamp = (Convert_To_Epoch(&buffer_rx_local[6]));
			if (TimeStamp != Status.Time.connect_date_time){
				Status.Time.connect_date_time = TimeStamp;
				ConfigFirebase.WriteTime = true;
			}
			TimeStamp = Convert_To_Epoch(&buffer_rx_local[12]);
			if (TimeStamp != Status.Time.disconnect_date_time){
				Status.Time.disconnect_date_time = TimeStamp;
				ConfigFirebase.WriteTime = true;
			}
			TimeStamp = Convert_To_Epoch(&buffer_rx_local[18]);
			if (TimeStamp != Status.Time.charge_start_time){
				Status.Time.charge_start_time = TimeStamp;
				ConfigFirebase.WriteTime = true;
			}
			TimeStamp = Convert_To_Epoch(&buffer_rx_local[24]);
			if (TimeStamp != Status.Time.charge_stop_time){
				Status.Time.charge_stop_time = TimeStamp;
				ConfigFirebase.WriteTime = true;
			}
			TimeStamp = Convert_To_Epoch(buffer_rx_local);
			if (TimeStamp != Status.Time.actual_time){
				Status.Time.actual_time = TimeStamp;
			}
#endif
		}
		break;

		case MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);
			Configuracion.data.install_current_limit = buffer_rx_local[0];
#ifdef DEBUG
			Serial.printf("Instalation current limit Changed to %i\n",Configuracion.data.install_current_limit);
#endif
#ifdef CONNECTED
			Params.install_current_limit = buffer_rx_local[0];
#endif
		}
		break;

		case MEASURES_CURRENT_COMMAND_CHAR_HANDLE:{
#ifdef CONNECTED
			Comands.desired_current = buffer_rx_local[0];
			Comands.Newdata = false;
#ifdef DEBUG
			Serial.printf("control - procesar_bloque: MEASURES_CURRENT_COMMAND_CHAR_HANDLE - Current command received = %i\n", Comands.desired_current);
#endif
#endif

			modifyCharacteristic(buffer_rx_local, 1, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
		}
		break;

		case TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
		}
		break;

		case TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);
		}
		break;

		case CHARGING_INSTANT_DELAYED_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, CHARGING_INSTANT_DELAYED_CHAR_HANDLE);
		}
		break;

		case CHARGING_START_STOP_START_MODE_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, CHARGING_START_STOP_START_MODE_CHAR_HANDLE);
		}
		break;

		case CHARGING_BLE_MANUAL_START_CHAR_HANDLE:{
			#ifdef CONNECTED
			Comands.start=0;
			#endif
			#ifdef DEBUG
			Serial.println("Start recibido");
			#endif
			modifyCharacteristic(buffer_rx_local, 1, CHARGING_BLE_MANUAL_START_CHAR_HANDLE);
		}
		break;

		case CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE:{
			#ifdef CONNECTED
			Comands.stop=0;
			#endif
			#ifdef DEBUG
			Serial.println("Stop recibido");
			#endif
			modifyCharacteristic(buffer_rx_local, 1, CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE);
		}
		break;

		case SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 168, SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);
		}
		break;
		
		case VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE:{
		
			modifyCharacteristic(buffer_rx_local, 1, VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE);
		}
		break;
		
		case VCD_NAME_USERS_USER_TYPE_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, VCD_NAME_USERS_USER_TYPE_CHAR_HANDLE);
			ESP_LOGI(TAG,"Recibido user_type %i",buffer_rx_local[0]);
		} 
		break;
		
		case VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE);
			user_index = buffer_rx_local[0];
			ESP_LOGI(TAG,"Recibido user_index=%i",user_index);
			user_index_arrived = 1;
		} 
		break;
		
		case TEST_LAUNCH_RCD_PE_TEST_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, TEST_LAUNCH_RCD_PE_TEST_CHAR_HANDLE);
		} 
		break;
		
		case TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE);
		} 
		break;
		
		case MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_CHAR_HANDLE);
		} 
		break;
		
		case RESET_RESET_CHAR_HANDLE:{
			delay(600);
			ESP.restart();
		} 
		break;

		case CHARGE_USER_ID:{
#ifdef DEBUG
			ESP_LOGI(TAG,"Recibido charge_user_id=%i",buffer_rx_local[0]);
#endif
			modifyCharacteristic(buffer_rx_local, 1, CHARGE_USER_ID);
		}
		break;
		
		case ENERGY_RECORD_RECORD_CHAR_HANDLE:{
#ifdef DEBUG
			ESP_LOGI(TAG,"Energy record received from PSoC");
#endif
			memcpy(record_received_buffer,buffer_rx_local,20);
			memcpy(record_received_buffer_for_fb,buffer_rx_local,sizeof(buffer_rx_local));

			int j = 20;
			bool end = false;
			for(int i =20;i<512;i+=4){		
				if(j<252){
					if(buffer_rx_local[j]==255 && buffer_rx_local[j+1]==255){
						end = true;
					}
					int PotLeida=buffer_rx_local[j]*0x100+buffer_rx_local[j+1];
					if(PotLeida >0 && PotLeida < 5900 && !end){
						record_received_buffer[i]   = buffer_rx_local[j];
						record_received_buffer[i+1] = buffer_rx_local[j+1];
						record_received_buffer[i+2] = 0;
						record_received_buffer[i+3] = 0;
					}
					else{
						record_received_buffer[i]   = 0;
						record_received_buffer[i+1] = 0;
						record_received_buffer[i+2] = 0;
						record_received_buffer[i+3] = 0;
					}

					j+=2;
				}
				else{
					record_received_buffer[i]   = 0;
					record_received_buffer[i+1] = 0;
					record_received_buffer[i+2] = 0;
					record_received_buffer[i+3] = 0;
				}				
			}
			new_record_received = true;
			ask_for_new_record = false;

#ifdef CONNECTED
			//Si no estamos conectados por ble
			
			if(ConfigFirebase.InternetConection){
				if(Coms.ETH.ON || Coms.Wifi.ON){
					if(!memcmp(Status.HPT_status, "A1",2) || !memcmp(Status.HPT_status, "A2",2) || !memcmp(Status.HPT_status, "0V",2)){
						if(!ConfigFirebase.UserReseted){
							ConfigFirebase.ResetUser = true;
						}
					}
				}
			}

#endif
			modifyCharacteristic(record_received_buffer, 512, ENERGY_RECORD_RECORD_CHAR_HANDLE);
		} 
		break;
		
		case RECORDING_REC_LAST_CHAR_HANDLE:{
#ifdef DEBUG
			Serial.printf("Me ha llegado una nueva recarga! %i\n", (buffer_rx_local[0] + buffer_rx_local[1] * 0x100));
#endif
			last_record_in_mem = (buffer_rx_local[0] + buffer_rx_local[1]*0x100);
			record_index = last_record_in_mem;
			modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_LAST_CHAR_HANDLE);
		} 
		break;
		
		case RECORDING_REC_INDEX_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_INDEX_CHAR_HANDLE);
		} 
		break;
		
		case RECORDING_REC_LAPS_CHAR_HANDLE:{
			uint8_t buffer[2];
			uint8_t received_lap;
#ifdef DEBUG
			Serial.printf("Me ha llegado una nueva vuelta! %i\n", (buffer_rx_local[0] + buffer_rx_local[1] * 0x100));
#endif
			received_lap = (buffer_rx_local[0] + buffer_rx_local[1] * 0x100);
			if ((received_lap == last_record_lap) || (received_lap == last_record_lap+1)) {
				last_record_lap = received_lap;
			}
			record_lap = last_record_lap;
			buffer[0] = (uint8)(last_record_in_mem & 0x00FF);
			buffer[1] = (uint8)((last_record_in_mem >> 8) & 0x00FF);

			modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_LAPS_CHAR_HANDLE);
			SendToPSOC5(buffer, 2, RECORDING_REC_INDEX_CHAR_HANDLE);
		} 
		break;

#ifdef CONNECTED
		case SEARCH_EXTERNAL_METER:{
			ContadorExt.MedidorConectado = true;
			Configuracion.data.medidor485 = buffer_rx_local[0];
			#ifdef DEBUG
			printf("El medidor RS485 se ha encontrado %d\n", (buffer_rx_local[0]));
			#endif
			if(buffer_rx_local[0]){//Se ha encontrado
				ContadorExt.GatewayConectado = true;
				Update_Status_Coms(MED_BUSCANDO_GATEWAY);
				delay(20);
				Update_Status_Coms(MED_BUSCANDO_MEDIDOR);
				delay(20);
				Update_Status_Coms(MED_LEYENDO_MEDIDOR);	
			}
			else{//NO se ha encontrado
				Update_Status_Coms(MED_BUSCANDO_GATEWAY);
				delay(20);
				Update_Status_Coms(0,MED_BLOCK);
				ContadorExt.GatewayConectado = false;
				ContadorExt.MedidorConectado = false;
			}
		} 
		break;
#endif		

		case CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE:{
			
			//comprobar si el dato tiene sentido, sino cargar el de mi memoria
			if(memcmp("AA", buffer_rx_local,2) && memcmp("WA", buffer_rx_local,2) && memcmp("MA", buffer_rx_local,2)){
				if(memcmp("AA", Configuracion.data.autentication_mode,2) && memcmp("WA", Configuracion.data.autentication_mode,2) && memcmp("MA", Configuracion.data.autentication_mode,2)){
					memcpy(buffer_rx_local,Configuracion.data.autentication_mode,2);
				}
				else{
					//Si ninguno de los dos es correcto, lo damos por malo y ponemos el de defecto
					memcpy(buffer_rx_local,"WA",2);
					SendToPSOC5(buffer_rx_local,2,CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
					break;
				}
			}
			else{
				memcpy(Configuracion.data.autentication_mode, buffer_rx_local,2);
			}

			modifyCharacteristic(buffer_rx_local, 2, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
			memcpy(Configuracion.data.autentication_mode, buffer_rx_local,2);

#ifdef DEBUG
			ESP_LOGI(TAG,"Nuevo autentication_mode= %c%c",buffer_rx_local[0], buffer_rx_local[1]);
#endif
#ifdef CONNECTED
			memcpy(Params.autentication_mode, buffer_rx_local, 2);
#endif
		} 
		break;
		
		case DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P1_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P1_CHAR_HANDLE);
			Configuracion.data.potencia_contratada1 =buffer_rx_local[0]+buffer_rx_local[1]*0x100;
			#ifdef DEBUG
			Serial.println("Potencia contratada 1 cambiada a: ");
			Serial.println(buffer_rx_local[0]+buffer_rx_local[1]*0x100);
			#endif

			#ifdef CONNECTED
				Params.potencia_contratada1=buffer_rx_local[0]+buffer_rx_local[1]*0x100;
			#endif
		}
		break;

		case DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P2_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P2_CHAR_HANDLE);
			Configuracion.data.potencia_contratada2 =buffer_rx_local[0]+buffer_rx_local[1]*0x100;
			#ifdef DEBUG
			Serial.println("Potencia contratada 2 cambiada a: ");
			Serial.println(buffer_rx_local[0]+buffer_rx_local[1]*0x100);
			#endif

			#ifdef CONNECTED
				Params.potencia_contratada2=buffer_rx_local[0]+buffer_rx_local[1]*0x100;
			#endif

		}
		break;

		case TIME_DATE_COUNTRY_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 2, TIME_DATE_COUNTRY_CHAR_HANDLE);
		}
		break;
		
		case TIME_DATE_CALENDAR_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 41, TIME_DATE_CALENDAR_CHAR_HANDLE);
		}
		break;

		case ERROR_STATUS_ERROR_CODE_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, ERROR_STATUS_ERROR_CODE_CHAR_HANDLE);

			#ifdef CONNECTED
				Status.error_code=buffer_rx_local[0];
			#endif
		} 
		break;
		
		case BOOT_LOADER_LOAD_SW_APP_CHAR_HANDLE:{
			ESP_LOGI(TAG, "PSOC A BOOTLOADER, ACTUALIZO.");
			psocUpdateCounter = 0;
			vTaskDelay(pdMS_TO_TICKS(2500));

			BaseType_t update_err = xTaskCreate(UpdateTask,"TASK UPDATE",4096*3,NULL,5,NULL);
			if (update_err == pdPASS) {
				Serial.println("Tarea creada exitosamente.");
			} else {
				Serial.print("Error al crear la tarea. Código de error: ");
				Serial.println(update_err); // Imprime el valor numérico del error
			}
			updateTaskrunning=1;
		} 
		break;
				
		case DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
			Configuracion.data.CDP = buffer_rx_local[0];
			#ifdef CONNECTED
				Params.CDP = buffer_rx_local[0];			
				if((Params.CDP >> 1) & 0x01){
					Params.Tipo_Sensor = ((Params.CDP >> 4) & 0x01);
					//Bloquear la carga hasta que encontremos el medidor
					if (Params.Tipo_Sensor){
						ESP_LOGI(TAG,"CDP con medidor");
						if (!Configuracion.data.medidor485){
							// Buscar medidor cuando lo pulsan en la APP
							ESP_LOGI(TAG,"Enviando peticion de búsqueda de medidor 485 al PSoC");
							Coms.ETH.medidor = true;
							Update_Status_Coms(0, MED_BUSCANDO_GATEWAY);
							// Bloqueo_de_carga = 1;
							// SendToPSOC5(Bloqueo_de_carga,BLOQUEO_CARGA);
							SendToPSOC5(1, SEARCH_EXTERNAL_METER);
						}
					}
					else{
						ESP_LOGI(TAG,"CDP con CURVE");
						Coms.ETH.medidor = false;
						Configuracion.data.medidor485 = 0;
						ContadorExt.MedidorConectado = 0;
					}
				}
				else{
					Params.Tipo_Sensor = 0;
					Coms.ETH.medidor = false;
					Configuracion.data.medidor485 = 0;
					ContadorExt.MedidorConectado = 0;
				}

				if(((buffer_rx_local[0] >> 5) & 0x03) > 0){
					Status.Photovoltaic=true;
				}else{
					Status.Photovoltaic=false;
				}

				if(!Params.Tipo_Sensor){
					Coms.ETH.medidor = false;
					Update_Status_Coms(0,MED_BLOCK);
					ContadorExt.ConexionPerdida = false;
					if(Coms.ETH.DHCP && !ChargingGroup.Params.GroupActive){
						Coms.ETH.restart = true;
					}
				}
#ifdef DEBUG_BLE
					Serial.printf("New CDP %i %i \n", Params.CDP, Params.Tipo_Sensor);
#endif
#endif
		} 
		break;

#ifdef CONNECTED
		case COMS_CONFIGURATION_WIFI_ON:{
			Coms.Wifi.ON =  buffer_rx_local[0];
			#ifdef DEBUG
			Serial.print("WIFI ON:");
			Serial.println(Coms.Wifi.ON);
			#endif
			modifyCharacteristic(buffer_rx_local , 1, COMS_CONFIGURATION_WIFI_ON);
		} 
		break;
		
		case COMS_CONFIGURATION_ETH_ON:{

			if(Coms.ETH.medidor && !Coms.ETH.ON && buffer_rx_local[0] && !Coms.ETH.conectado){
				Coms.ETH.restart = true;
			}
			else if (Coms.ETH.medidor && Coms.ETH.ON && !buffer_rx_local[0] && !Coms.ETH.conectado){
				Coms.ETH.restart = true;
			}

			Coms.ETH.ON  =  buffer_rx_local[0];

			uint8_t ip_Array[4] = { 0,0,0,0};
			if(Coms.ETH.ON ){
				
				ip_Array[0] = ip4_addr1(&Coms.ETH.IP);
				ip_Array[1] = ip4_addr2(&Coms.ETH.IP);
				ip_Array[2] = ip4_addr3(&Coms.ETH.IP);
				ip_Array[3] = ip4_addr4(&Coms.ETH.IP);

				if(ComprobarConexion()){
					ConfigFirebase.LastConnState = ConfigFirebase.FirebaseConnState;
					ConfigFirebase.FirebaseConnState = DISCONNECTED;
					Coms.ETH.Internet = true;
					Coms.ETH.Wifi_Perm = false;
				}	
			}
			else{
				Coms.ETH.Wifi_Perm = true;
				Coms.ETH.Internet = false;
				if(!Coms.Wifi.ON){
					ConfigFirebase.InternetConection = false;
				}
			}

			
			modifyCharacteristic(&ip_Array[0], 4, COMS_CONFIGURATION_LAN_IP);
			
			
			#ifdef DEBUG
			Serial.print("ETH ON:");
			Serial.println(Coms.ETH.ON);
			#endif
			
			modifyCharacteristic(buffer_rx_local,  1, COMS_CONFIGURATION_ETH_ON);
		} 
		break;

		case COMS_CONFIGURATION_ETH_AUTO:{
			Coms.ETH.Auto    =  buffer_rx_local[0];
			#ifdef DEBUG
			Serial.print("ETH Auto :");
			Serial.println(Coms.ETH.Auto);
			#endif
		} 
		break;
		
		case COMS_FW_UPDATEMODE_CHAR_HANDLE:{
			memcpy(Params.Fw_Update_mode,buffer_rx_local,2);
			Params.Fw_Update_mode[2]='\0';
#ifdef DEBUG
			ESP_LOGI(TAG,"Nuevo Fw_Update_mode= %c%c",buffer_rx_local[0], buffer_rx_local[1]);
#endif
		} 
		break;
		
		case COMS_CONFIGURATION_LAN_IP:{

			ip4_addr adress;
			IP4_ADDR(&adress, buffer_rx_local[0],buffer_rx_local[1],buffer_rx_local[2],buffer_rx_local[3]);
			ip4_addr_copy(Coms.ETH.IP,adress);
			IP4_ADDR(&adress, buffer_rx_local[4],buffer_rx_local[5],buffer_rx_local[6],buffer_rx_local[7]);
			ip4_addr_copy(Coms.ETH.Gateway,adress);
			IP4_ADDR(&adress, buffer_rx_local[8],buffer_rx_local[9],buffer_rx_local[10],buffer_rx_local[11]);
			ip4_addr_copy(Coms.ETH.Mask,adress);
			
		} 
		break;

		case GROUPS_DEVICES_PART_1:{
			Serial.printf("GROUPS_DEVICES_PART_1 : AQUÍ NO SE HACE NADA\n");
		}
		break;
		
        //Segunda parte del grupo de cargadores
        case GROUPS_DEVICES_PART_2:{
			Serial.printf("GROUPS_DEVICES_PART_2 : AQUÍ NO SE HACE NADA\n");
		}
		break;
	
		case GROUPS_PARAMS:{	
			Serial.printf("GROUPS_PARAMS : AQUÍ NO SE HACE NADA\n");
		}
		break;
		
		case GROUPS_CIRCUITS:{
			Serial.printf("GROUPS_CIRCUITS : AQUÍ NO SE HACE NADA\n");
		}
		break;

		case BLOQUEO_CARGA:{
#ifdef DEBUG_GROUPS
			printf("control - procesar_bloque: BLOQUEO_CARGA. Recibido de PSoC BLOQUEO_CARGA. Bloqueo_de_carga = %i\n", Bloqueo_de_carga);
#endif
			Bloqueo_de_carga_old = Bloqueo_de_carga;
			Bloqueo_de_carga = true;
			if(dispositivo_inicializado != 2){
				Bloqueo_de_carga = true;
			}

			//si somos parte de un grupo, debemos pedir permiso al maestro
			else if(ChargingGroup.Params.GroupActive || ChargingGroup.Finding ){
				if(ChargingGroup.Conected){
					if(ChargingGroup.ChargPerm){
						Bloqueo_de_carga = false;
						ChargingGroup.AskPerm = false;
#ifdef DEBUG_GROUPS
						Serial.printf("control - procesar_bloque: BLOQUEO_CARGA. A CARGAR !\n");
						Serial.printf("control - procesar_bloque: BLOQUEO_CARGA. Bloqueo_de_carga = %i\n",Bloqueo_de_carga);
						Serial.printf("control - procesar_bloque: BLOQUEO_CARGA. ChargingGroup.AskPerm = %i\n",ChargingGroup.AskPerm);
#endif
					}
					else if(!ChargingGroup.AskPerm ){
						ChargingGroup.AskPerm = true;
#ifdef DEBUG_GROUPS
						Serial.printf("control - procesar_bloque: BLOQUEO_CARGA. ChargingGroup.AskPerm = %i\n",ChargingGroup.AskPerm);
#endif
					}
				}
			}
			//Si tenemos un medidor conectado, hasta que no nos conectemos a él no permitimos la carga
			else if(Params.Tipo_Sensor || (ChargingGroup.Params.GroupMaster && ContadorConfigurado())){
				if(ContadorExt.MedidorConectado){
					Bloqueo_de_carga = false;
#ifdef DEBUG_GROUPS
					Serial.printf("procesar_bloque - BLOQUEO_CARGA. MedidorConectado. Bloqueo_de_carga = false\n");
#endif
				}
			}

			//si no se cumple nada de lo anterior, permitimos la carga
			else{
#ifdef DEBUG_GROUPS
				Serial.printf("procesar_bloque - BLOQUEO_CARGA. else final. Bloqueo_de_carga = false\n");
#endif
				Bloqueo_de_carga = false;
			}
			if (Bloqueo_de_carga != Bloqueo_de_carga_old){
				SendToPSOC5(Bloqueo_de_carga, BLOQUEO_CARGA);
#ifdef DEBUG_GROUPS
				Serial.printf("procesar_bloque - BLOQUEO_CARGA. Se envía Bloqueo_de_carga = %i al PSoC\n", Bloqueo_de_carga);
#endif
			}
		}
		break;

#endif
		case CLEAR_FLASH_SPACE:
			Serial.printf("Se ha borrado la caracteristica %i de la flash!\n", buffer_rx_local[0]);
			Configuracion.data.Data_cleared = 1;
		break;

		default:
		break;
	}
}

/************************************************
 * Update Task
 * **********************************************/
void UpdateTask(void *arg){

	UpdateStatus.InstalandoArchivo = true;
#ifdef DEBUG
	Serial.println("\nComenzando actualizacion del PSoC!!");
#endif

	int Nlinea = 0;
	uint8_t err = 0;
	String Buffer;

	uint32_t siliconID;
	uint8_t siliconRev;
	uint8_t packetChkSumType;
	uint8_t appID;
	uint8_t bootloaderEntered = 0;
	uint32_t applicationStartAddr = 0xffffffff;
	uint32_t applicationSize = 0;
	uint32_t applicationDataLines = 255;
	uint32_t applicationDataLinesSeen = 0;
	uint32_t prodID;
	uint8_t arrayId;
	uint32_t address;
	uint16_t rowSize;
	uint8_t checksum;
	uint32_t blVer=0;
	uint8_t rowData[512];
	File file;
	Serial.println(Configuracion.data.count_reinicios_malos);
	if(Configuracion.data.count_reinicios_malos > 5){
		if(SPIFFS.exists(PSOC_UPDATE_OLD_FILE)){
			Serial.println("Se ha intentado 10 veces y existe un FW_Old, se prueba con este");
			if(SPIFFS.exists(PSOC_UPDATE_FILE)){
				err = SPIFFS.remove(PSOC_UPDATE_FILE);
				Serial.printf("Removing %s with errcode %u\n",PSOC_UPDATE_FILE,err);
			}
			err = SPIFFS.rename(PSOC_UPDATE_OLD_FILE, PSOC_UPDATE_FILE);
			Serial.printf("Renaming %s to %s with ReturnValue: %u\n",PSOC_UPDATE_OLD_FILE,PSOC_UPDATE_FILE,err);
		}
	}

	err = CyBtldr_RunAction(PROGRAM, &serialLocal, NULL, PSOC_UPDATE_FILE, "PSOC5");
	Serial.printf("Actualizacion terminada: err = %d\n", err);
	if (!err){
		if(SPIFFS.exists(PSOC_UPDATE_OLD_FILE)){
			Serial.printf("Borrando PSOC_UPDATE_OLD_FILE %d\n", err);

			err = SPIFFS.remove(PSOC_UPDATE_OLD_FILE);
			Serial.printf("Removing oldfile with errcode %d", err);
		}
		err = SPIFFS.rename(PSOC_UPDATE_FILE, PSOC_UPDATE_OLD_FILE);
		Serial.print(String("Renaming ") + PSOC_UPDATE_FILE + " to " + PSOC_UPDATE_OLD_FILE + " with, returnValue: " + err);
	}
	UpdateStatus.InstalandoArchivo = false;
	updateTaskrunning = 0;
	setMainFwUpdateActive(0);
	update_flag = 0;
	UpdateStatus.InstalandoArchivo = false;
	if (!UpdateStatus.ESP_UpdateAvailable){
		Serial.println("Reiniciando en 4 segundos!");
		vTaskDelay(pdMS_TO_TICKS(4000));
		MAIN_RESET_Write(0);
		delay(500);
		ESP.restart();
	}
	else{
		
		UpdateStatus.PSOC_UpdateAvailable = false;
		UpdateStatus.DobleUpdate = true;
		ESP_LOGI(TAG,"VELT Instalado. ESP_UpdateAvailable así que DobleUpdate pasa a 1");
	}

	vTaskDelete(NULL);
}

void controlInit(void){
	xTaskCreateStatic(LedControl_Task,"TASK LEDS",4096*2,NULL,PRIORIDAD_LEDS,xLEDStack,&xLEDBuffer); 
	Configuracion.init();
	dispositivo_inicializado = 1;
	//Freertos estatico
	xTaskCreateStatic(controlTask,"TASK CONTROL",4096*6,NULL,PRIORIDAD_CONTROL,xControlStack,&xControlBuffer); 
	xTaskCreateStatic(proceso_recepcion,"TASK UART",4096*6,NULL,PRIORIDAD_UART,xUartStack,&xUartBuffer); 
#ifdef CONNECTED
	xTaskCreate(ComsTask,"Task Coms",4096*4,NULL,PRIORIDAD_COMS,NULL);
	vTaskDelay(pdMS_TO_TICKS(500));
	xTaskCreateStatic(Firebase_Conn_Task,"TASK FIREBASE", 4096*6,NULL,PRIORIDAD_FIREBASE,xFirebaseStack , &xFirebaseBuffer );
#endif
}


#ifdef CONNECTED
void Get_Stored_Group_Data(){
  char n[2];
  char ID[9];
  uint8 buffer_group[MAX_GROUP_BUFFER_SIZE];

  ChargingGroup.NewData = true;
  temp_chargers_size = 0;
  // Almacenado temporal del grupo
  for (uint8_t i = 0; i < ChargingGroup.Charger_number; i++){
    memcpy(temp_chargers[temp_chargers_size].name, charger_table[i].name, 9);
    memcpy(temp_chargers[temp_chargers_size].HPT, charger_table[i].HPT, 3);
    temp_chargers[temp_chargers_size].Current = charger_table[i].Current;
    temp_chargers[temp_chargers_size].CurrentB = charger_table[i].CurrentB;
    temp_chargers[temp_chargers_size].CurrentC = charger_table[i].CurrentC;
    temp_chargers[temp_chargers_size].Delta = charger_table[i].Delta;
    temp_chargers[temp_chargers_size].Consigna = charger_table[i].Consigna;
    temp_chargers[temp_chargers_size].Delta_timer = charger_table[i].Delta_timer;
    temp_chargers[temp_chargers_size].Fase = charger_table[i].Fase;
    temp_chargers[temp_chargers_size].Circuito = charger_table[i].Circuito;

    temp_chargers_size++;
  }

  // Obtencion de datos grupo desde la flash
  Configuracion.Group_Cargar = true;
  delay (500);
  memcpy(buffer_group,Configuracion.group_data.group,MAX_GROUP_BUFFER_SIZE);
  memcpy(n,Configuracion.group_data.group, 2);

#ifdef DEBUG_GROUPS
  Serial.printf("Buffer group leido de configuracion=[%s]\n", buffer_group);
#endif

  ChargingGroup.Charger_number = 0;
  uint8_t limit = atoi(n);

  for (uint8_t i = 0; i < limit; i++){
    for (uint8_t j = 0; j < 8; j++){
      ID[j] = (char)buffer_group[2 + i * 9 + j];
    }
    ID[8] = '\0';
    add_to_group(ID, get_IP(ID), charger_table, &ChargingGroup.Charger_number);
#ifdef DEBUG_GROUPS
    Serial.printf("Crudo fase y circuito PSoC %s %i %i %i\n", ID, uint8_t(buffer_group[10 + i * 9]), uint8_t(buffer_group[10 + i * 9]) & 0x03, uint8_t(buffer_group[10 + i * 9]) >> 2);
#endif
    charger_table[i].Fase = (buffer_group[10 + i * 9]) & 0x03;
    charger_table[i].Circuito = (buffer_group[10 + i * 9]) >> 2;

    uint8_t index = check_in_group(ID, temp_chargers, temp_chargers_size);
    if (index != 255){
      memcpy(charger_table[i].HPT, temp_chargers[index].HPT, 3);
      charger_table[i].Current = temp_chargers[index].Current;
      charger_table[i].CurrentB = temp_chargers[index].CurrentB;
      charger_table[i].CurrentC = temp_chargers[index].CurrentC;
      charger_table[i].Delta = temp_chargers[index].Delta;
      charger_table[i].Consigna = temp_chargers[index].Consigna;
      charger_table[i].Delta_timer = temp_chargers[index].Delta_timer;
    }

    if (!memcmp(ID, ConfigFirebase.Device_Id, 8)){
      charger_table[i].Conected = true;
    }
  }

  // si llega un grupo en el que no estoy, significa que me han sacado de el
  // cierro el coap y borro el grupo
  if (check_in_group(ConfigFirebase.Device_Id, charger_table, ChargingGroup.Charger_number) == 255){
    if (ChargingGroup.Conected){
#ifdef DEBUG_GROUPS
      Serial.printf("control - procesar_bloque:  - No estoy en el grupo !! \n");
      print_table(charger_table, "No en grupo table 1", ChargingGroup.Charger_number);
#endif
      ChargingGroup.DeleteOrder = true;
    }
  }

  // Ponerme el primero en el grupo para indicar que soy el maestro
  if (ChargingGroup.Params.GroupMaster){
    if (memcmp(charger_table[0].name, ConfigFirebase.Device_Id, 8)){
      if (ChargingGroup.Charger_number > 0 && check_in_group(ConfigFirebase.Device_Id, charger_table, ChargingGroup.Charger_number) != 255){
        while (memcmp(charger_table[0].name, ConfigFirebase.Device_Id, 8)){
          carac_charger OldMaster = charger_table[0];
          remove_from_group(OldMaster.name, charger_table, &ChargingGroup.Charger_number);
          add_to_group(OldMaster.name, OldMaster.IP, charger_table, &ChargingGroup.Charger_number);
          charger_table[ChargingGroup.Charger_number - 1].Fase = OldMaster.Fase;
          charger_table[ChargingGroup.Charger_number - 1].Circuito = OldMaster.Circuito;
          charger_table[ChargingGroup.Charger_number - 1].Current = OldMaster.Current;
          charger_table[ChargingGroup.Charger_number - 1].CurrentB = OldMaster.CurrentB;
          charger_table[ChargingGroup.Charger_number - 1].CurrentC = OldMaster.CurrentC;
          charger_table[ChargingGroup.Charger_number - 1].Consigna = OldMaster.Consigna;
          charger_table[ChargingGroup.Charger_number - 1].Delta = OldMaster.Delta;
          charger_table[ChargingGroup.Charger_number - 1].Delta_timer = OldMaster.Delta_timer;
          memcpy(charger_table[ChargingGroup.Charger_number - 1].HPT, OldMaster.HPT, 3);
        }
        ChargingGroup.SendNewGroup = true;
      }
    }
    // si soy el maestro, avisar a los nuevos de que son parte de mi grupo
    broadcast_a_grupo("Start client", 12);
  }
#ifdef DEBUG_GROUPS
  print_table(charger_table, "Grupo en FLASH", ChargingGroup.Charger_number);
#endif

}
#endif

#ifdef CONNECTED
void Get_Stored_Group_Params(){
	uint8 buffer_params[SIZE_OF_GROUP_PARAMS];

	ChargingGroup.NewData = true;
	Configuracion.Group_Cargar = true;
	delay(500);
	memcpy(buffer_params, Configuracion.group_data.params, SIZE_OF_GROUP_PARAMS);

	ChargingGroup.Params.GroupMaster = buffer_params[0];
	if (ChargingGroup.Params.GroupActive && !buffer_params[1]){
		ChargingGroup.StopOrder = true;
	}
	ChargingGroup.Params.GroupActive = buffer_params[1];
	ChargingGroup.Params.CDP = buffer_params[2];
	ChargingGroup.Params.ContractPower = buffer_params[3] + buffer_params[4] * 0x100;
	ChargingGroup.Params.potencia_max = buffer_params[5] + buffer_params[6] * 0x100;

#ifdef DEBUG_GROUPS
	Serial.printf("control - Get_Stored_Group_Params: ChargingGroup.Params.GroupActive %i \n", ChargingGroup.Params.GroupActive);
	Serial.printf("control - Get_Stored_Group_Params: ChargingGroup.Params.GroupMaster %i \n", ChargingGroup.Params.GroupMaster);
	Serial.printf("control - Get_Stored_Group_Params: ChargingGroup.Params.potencia_max %i \n", ChargingGroup.Params.potencia_max);
	Serial.printf("control - Get_Stored_Group_Params: ChargingGroup.Params.ContractPower %i \n", ChargingGroup.Params.ContractPower);
	Serial.printf("control - Get_Stored_Group_Params: ChargingGroup.Params.CDP %i \n", ChargingGroup.Params.CDP);
	Serial.printf("control - Get_Stored_Group_Params: ChargingGroup.Params.inst_max %i \n", ChargingGroup.Params.inst_max);
#endif
}
#endif

#ifdef CONNECTED
void Get_Stored_Group_Circuits(){
	uint8 buffer_circuits[MAX_GROUP_SIZE];
	ChargingGroup.NewData = true;
	
	Configuracion.Group_Cargar = true;
	delay(500);
	memcpy(buffer_circuits, Configuracion.group_data.circuits, MAX_GROUP_SIZE);

	int numero_circuitos = buffer_circuits[0];
	ChargingGroup.Circuit_number = numero_circuitos;
	for (int i = 0; i < numero_circuitos; i++){
		Circuitos[i].numero = i + 1;
		Circuitos[i].Fases[0].numero = 1;
		Circuitos[i].Fases[1].numero = 2;
		Circuitos[i].Fases[2].numero = 3;
		Circuitos[i].limite_corriente = buffer_circuits[i + 1];
#ifdef DEBUG_GROUPS
		Serial.printf("Get_Stored_Group_Circuits: GROUPS_CIRCUITS: Circuito %i limite_corriente %i \n", i, Circuitos[i].limite_corriente);
#endif
	}
}

uint8 checkSpiffsBug()
{
	uint32_t checkPattern = random(0, UINT32_MAX);
    uint32_t readcheckPattern = 0;
	uint8 spiffs_bug;

    // Inicializar la memoria no volátil (NVS)
    esp_err_t nvs_init_result = nvs_flash_init_partition("nvs");
    if (nvs_init_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_init_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Manejar errores de inicialización si es necesario
        // En este ejemplo, simplemente se borran los datos y se vuelve a intentar
        ESP_ERROR_CHECK(nvs_flash_erase_partition("nvs"));
        nvs_init_result = nvs_flash_init_partition("nvs");
    }
    ESP_ERROR_CHECK(nvs_init_result);
    // Abrir el espacio de almacenamiento en la memoria no volátil
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("nvs", NVS_READWRITE, &nvs_handle));
    // Obtener el tipo de arranque
    nvs_set_u32(nvs_handle, "pattern", checkPattern);
    nvs_commit(nvs_handle);
    nvs_get_u32(nvs_handle, "pattern", &readcheckPattern);
    ESP_LOGW(TAG, " **************************** -- FLASH -- WritePattern %u / ReadPattern %u", checkPattern, readcheckPattern);
	//Serial.println(" **************************** -- FLASH -- WritePattern %u / ReadPattern %u", checkPattern, readcheckPattern);

    if (checkPattern == readcheckPattern) spiffs_bug = 0;
    else spiffs_bug = 1;
    // Cerrar el espacio de almacenamiento en la memoria no volátil
    nvs_close(nvs_handle);
	return spiffs_bug;
}
#endif