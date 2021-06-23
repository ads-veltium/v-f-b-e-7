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

StaticTask_t xControlBuffer ;
StaticTask_t xLEDBuffer ;

static StackType_t xControlStack  [4096*6]     EXT_RAM_ATTR;
static StackType_t xLEDStack      [4096*2]     EXT_RAM_ATTR;

#ifdef CONNECTED
static StackType_t xFirebaseStack [4096*6]     EXT_RAM_ATTR;
StaticTask_t xFirebaseBuffer ;
#endif

//Variables Firebase
carac_Update_Status UpdateStatus EXT_RAM_ATTR;
carac_Comands  Comands       EXT_RAM_ATTR;
carac_Status   Status        EXT_RAM_ATTR;
carac_Params   Params        EXT_RAM_ATTR;
#ifdef CONNECTED
carac_Coms     Coms          EXT_RAM_ATTR;
carac_Contador ContadorExt   EXT_RAM_ATTR;
carac_Firebase_Configuration ConfigFirebase EXT_RAM_ATTR;

#ifdef USE_GROUPS
carac_group    ChargingGroup EXT_RAM_ATTR;
#endif
#endif


/* VARIABLES BLE */
uint8 device_ID[16] = {"VCD17010001"};
uint8 deviceSerNum[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};     //{0x00, 0x00, 0x00, 0x00, 0x0B, 0xCD, 0x17, 0x01, 0x00, 0x05};
uint8 authChallengeReply[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// DACO changed size by crash
//uint8 authChallengeQuery[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};      //{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
uint8 authChallengeQuery[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};      //{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

TickType_t AuthTimer=0;
uint8 contador_cent_segundos = 0, contador_cent_segundos_ant = 0;
uint32 cont_seg = 0, cont_seg_ant = 0, cont_min_ant = 0, cont_min=0, cont_hour=0, cont_hour_ant=0;
uint8 estado_inicial = 1;
uint8 estado_actual = ESTADO_ARRANQUE;
uint8 authSuccess = 0;
int aut_semilla = 0x0000;
uint8 NextLine=0;
uint8_t ConnectionState;

uint8 cnt_fin_bloque = 0;
int estado_recepcion = 0;
uint8 buffer_tx_local[256]  EXT_RAM_ATTR;
uint8 buffer_rx_local[256]  EXT_RAM_ATTR;
uint8 record_buffer[550] EXT_RAM_ATTR;
uint16 puntero_rx_local = 0;
uint32 TimeFromStart = 60; //tiempo para buscar el contador, hay que darle tiempo a conectarse
uint8 updateTaskrunning=0;
uint8 cnt_timeout_tx = 0;

// initial serial number
uint8 initialSerNum[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8 mainFwUpdateActive = 0;

uint8 dispositivo_inicializado = 1;
uint8 PSOC_inicializado =0;
uint8 cnt_timeout_inicio = 0;
uint16 cnt_repeticiones_inicio = 500;	//1000;

uint8 status_hpt_anterior[2] = {'F','F' };
uint16 inst_current_anterior = 0x0000;
uint16 cnt_diferencia = 1;
uint8 HPT_estados[9][3] = {"0V", "A1", "A2", "B1", "B2", "C1", "C2", "E1", "F1"};

#ifdef USE_COMS
uint8 version_firmware[11] = {"VBLE2_0512"};	
#else
uint8 version_firmware[11] = {"VBLE0_0512"};	
#endif
uint8 PSOC5_version_firmware[11] ;		

uint8 systemStarted = 0;
uint8 Record_Num =4;
uint8 Bloqueo_de_carga = 1;
int TimeoutMainDisconnect = 0;
extern bool deviceBleDisconnect;
void startSystem(void);

void StackEventHandler( uint32 eventCode, void *eventParam );
void modifyCharacteristic(uint8* data, uint16 len, uint16 attrHandle);
void proceso_recepcion(void);
int Convert_To_Epoch(uint8_t DateTime[6]);
void Disable_VELT1_CHARGER_services(void);
void broadcast_a_grupo(char* Mensaje, uint16_t size);


#ifdef CONNECTED
uint8_t check_in_group(const char* ID, carac_chargers* group);
bool remove_from_group(const char* ID ,carac_chargers* group);
bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);
IPAddress get_IP(const char* ID);
#endif


/*******************************************************************************
 * Rutina de atencion a inerrupcion de timer (10mS)
 ********************************************************************************/

hw_timer_t * timer10ms = NULL;
hw_timer_t * timer1s = NULL;
HardwareSerialMOD serialLocal(2);
unsigned long StartTime = millis();
void IRAM_ATTR onTimer10ms() 
{
	if(++contador_cent_segundos >= 100)	// 1 segundo
	{
		contador_cent_segundos = 0;
	}
	if ( cnt_fin_bloque )
	{
		--cnt_fin_bloque;
	}
}

void IRAM_ATTR onTimer1s() 
{

	if(++cont_seg>=60){// 1 minuto
		cont_seg=0;
		cont_min++;
	}
	if(cont_min>=60){// 1 hora
		cont_hour++;
	}
}

void configTimers ( void )
{
	//1 tick take 1/(80MHZ/80) = 1us so we set divider 80 and count up
	timer10ms = timerBegin(0, 100, true);
	timer1s = timerBegin(1, 100, true);
	timerAttachInterrupt(timer10ms, &onTimer10ms, true);
	timerAttachInterrupt(timer1s, &onTimer1s, true);
	// every 10 millis
	timerAlarmWrite(timer10ms, 10000, true);
	timerAlarmWrite(timer1s, 1000000, true);
	timerAlarmEnable(timer10ms);
	timerAlarmEnable(timer1s);

}

void MAIN_RESET_Write( uint8_t val )
{
	DRACO_GPIO_Reset_MCU(val);
	CyBtldr_EndBootloadOperation(&serialLocal);
	return;
}

void controlTask(void *arg) 
{	
	
	// Inicia el timer de 10mS i 1 segundo
	configTimers();
	bool LastUserCon = false;
	bool Iface_Con = BLE;
	uint8 old_inicializado = 0;
	serialLocal.begin(115200, SERIAL_8N1, 34, 4); // pins: rx, tx

	// INICIALIZO ELEMENTOS PARA AUTENTICACION
	srand(aut_semilla);

	modifyCharacteristic(authChallengeQuery, 8, AUTENTICACION_MATRIX_CHAR_HANDLE);

	MAIN_RESET_Write(1);        // Permito el arranque del micro principal
	
	while(1)
	{
		// Eventos 10 mS
		if(contador_cent_segundos != contador_cent_segundos_ant)
		{
			contador_cent_segundos_ant = contador_cent_segundos;

			if(mainFwUpdateActive == 0){
				switch (estado_actual){
					case ESTADO_ARRANQUE:
						if(!PSOC_inicializado){
							proceso_recepcion();
							if(--cnt_timeout_inicio == 0){
								cnt_timeout_inicio = TIMEOUT_INICIO;
								buffer_tx_local[0] = HEADER_TX;
								buffer_tx_local[1] = (uint8)(BLOQUE_INICIALIZACION >> 8);
								buffer_tx_local[2] = (uint8)BLOQUE_INICIALIZACION;
								buffer_tx_local[3] = 1;
								buffer_tx_local[4] = 0;
								serialLocal.write(buffer_tx_local, 5);
								if(--cnt_repeticiones_inicio == 0){
									cnt_repeticiones_inicio = 750;		//1000;								
									mainFwUpdateActive = 1;
									updateTaskrunning=1;
									Serial.println("Enviando firmware al PSOC5 por falta de comunicacion!");
									xTaskCreate(UpdateTask,"TASK UPDATE",4096,NULL,1,NULL);
								}
							}
						}
						else{
							serialLocal.flush();
							estado_actual = ESTADO_INICIALIZACION;
						}
						break;
					case ESTADO_INICIALIZACION:
						cnt_timeout_tx = TIMEOUT_TX_BLOQUE;
						
						buffer_tx_local[0] = HEADER_TX;
						buffer_tx_local[1] = (uint8)(BLOQUE_STATUS >> 8);
						buffer_tx_local[2] = (uint8)BLOQUE_STATUS;
						buffer_tx_local[3] = 2;
						#ifdef USE_COMS
						buffer_tx_local[4] = serverbleGetConnected() || ConfigFirebase.ClientConnected;
						#else
						buffer_tx_local[4] = serverbleGetConnected();
						#endif
						buffer_tx_local[5] = dispositivo_inicializado;
						serialLocal.write(buffer_tx_local, 6);
						estado_actual = ESTADO_NORMAL;
						break;
					case ESTADO_NORMAL:
						proceso_recepcion();

						//Alguien se está intentando conectar	
						if(AuthTimer !=0){
							uint32_t Transcurrido = pdTICKS_TO_MS(xTaskGetTickCount()-AuthTimer);
							if(Transcurrido > 10000 && !authSuccess){
								#ifdef DEBUG
								printf("Auth request fallada, me desconecto!!\n");
								#endif
								AuthTimer=0;
								deviceBleDisconnect = true;
							}
						}

					    if(serverbleGetConnected()){
							Iface_Con = BLE;
						}
#ifdef USE_COMS
						else if(ConfigFirebase.ClientConnected){
							Iface_Con = COMS;
						}
#endif
						if(old_inicializado != dispositivo_inicializado){
							cnt_timeout_tx = TIMEOUT_TX_BLOQUE2 ;
							buffer_tx_local[0] = HEADER_TX;
							buffer_tx_local[1] = (uint8)(BLOQUE_STATUS >> 8);
							buffer_tx_local[2] = (uint8)BLOQUE_STATUS;
							buffer_tx_local[3] = 2;
							buffer_tx_local[4] = serverbleGetConnected(); 
							buffer_tx_local[5] = dispositivo_inicializado;
							serialLocal.write(buffer_tx_local, 6);
							LastUserCon = serverbleGetConnected();
							old_inicializado = dispositivo_inicializado;
						}
						if(Testing){
							cnt_timeout_tx = TIMEOUT_TX_BLOQUE2 ;
							buffer_tx_local[0] = HEADER_TX;
							buffer_tx_local[1] = (uint8)(BLOQUE_STATUS >> 8);
							buffer_tx_local[2] = (uint8)BLOQUE_STATUS;
							buffer_tx_local[3] = 2;
							buffer_tx_local[4] = 1; 
							buffer_tx_local[5] = 4;
							serialLocal.write(buffer_tx_local, 6);
							LastUserCon = 1 ;
						}

						else if(Iface_Con == BLE && LastUserCon != serverbleGetConnected()){
							cnt_timeout_tx = TIMEOUT_TX_BLOQUE2 ;
							buffer_tx_local[0] = HEADER_TX;
							buffer_tx_local[1] = (uint8)(BLOQUE_STATUS >> 8);
							buffer_tx_local[2] = (uint8)BLOQUE_STATUS;
							buffer_tx_local[3] = 2;
							buffer_tx_local[4] = serverbleGetConnected(); 
							buffer_tx_local[5] = dispositivo_inicializado;
							serialLocal.write(buffer_tx_local, 6);
							LastUserCon = serverbleGetConnected() ;
						}
#ifdef USE_COMS
						else if(Iface_Con == COMS && LastUserCon != ConfigFirebase.ClientConnected){
							cnt_timeout_tx = TIMEOUT_TX_BLOQUE2 ;
							buffer_tx_local[0] = HEADER_TX;
							buffer_tx_local[1] = (uint8)(BLOQUE_STATUS >> 8);
							buffer_tx_local[2] = (uint8)BLOQUE_STATUS;
							buffer_tx_local[3] = 2;
							buffer_tx_local[4] = ConfigFirebase.ClientConnected;
							buffer_tx_local[5] = dispositivo_inicializado;
							serialLocal.write(buffer_tx_local, 6);
							LastUserCon = ConfigFirebase.ClientConnected;
						}
#endif
						if(cnt_timeout_tx == 0)
						{
#ifdef CONNECTED
							if (Comands.start){
								SendToPSOC5(1, CHARGING_BLE_MANUAL_START_CHAR_HANDLE);   
								#ifdef DEBUG
								Serial.println("Sending START");         
								#endif
							}

							else if (Comands.stop){
								SendToPSOC5(1, CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE);
								#ifdef DEBUG
								Serial.println("Sending STOP");
								#endif
							}

							else if (Comands.Newdata){
								#ifdef DEBUG
								printf("Enviando corriente %i\n", Comands.desired_current);
								#endif
								SendToPSOC5(Comands.desired_current, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
							}
							else if (Comands.reset){
								#ifdef DEBUG
								Serial.println("Reiniciando por comando externo! No preocuparse");
								#endif
								MAIN_RESET_Write(0);						
								ESP.restart();
							}	
#endif
						}
						if(++TimeoutMainDisconnect>30000){
							#ifdef DEBUG
							Serial.println("Main reset detected");
							#endif
							MAIN_RESET_Write(0);						
							ESP.restart();
						}
						
						else
							cnt_timeout_tx--;
						break;
					default:
						break;
				}
			}
			else{			
				if(!updateTaskrunning){
					//Poner el micro principal en modo bootload
					cnt_timeout_tx = TIMEOUT_TX_BLOQUE;
					buffer_tx_local[0] = HEADER_TX;
					buffer_tx_local[1] = (uint8)(BOOT_LOADER_LOAD_SW_APP_CHAR_HANDLE >> 8);
					buffer_tx_local[2] = (uint8)BOOT_LOADER_LOAD_SW_APP_CHAR_HANDLE;
					buffer_tx_local[3] = 1;
					buffer_tx_local[4] = 0;

					serialLocal.write(buffer_tx_local, 5);
					vTaskDelay(pdMS_TO_TICKS(500));
					xTaskCreate(UpdateTask,"TASK UPDATE",4096,NULL,1,NULL);
					updateTaskrunning=1;
				}				
			}

		}
		// Eventos 1 segundo
		if(cont_seg != cont_seg_ant){
			cont_seg_ant = cont_seg;
		}

		// Eventos 1 minuto
		if(cont_min != cont_min_ant)
		{
			cont_min_ant = cont_min;
		}

		// Eventos 1 Hora
		if(cont_hour != cont_hour_ant)
		{
			cont_hour_ant = cont_hour;
		}

		delay(10);	
	}
}

void startSystem(void){
	
	#ifdef DEBUG
	Serial.printf("startSystem(): device SN = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",
		deviceSerNum[0], deviceSerNum[1], deviceSerNum[2], deviceSerNum[3], deviceSerNum[4],
		deviceSerNum[5], deviceSerNum[6], deviceSerNum[7], deviceSerNum[8], deviceSerNum[9]);
	#endif
	dev_auth_init((void const*)&deviceSerNum);

	#ifdef CONNECTED
		//Get Device FirebaseDB ID
		sprintf(ConfigFirebase.Device_Db_ID,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",deviceSerNum[0], deviceSerNum[1], deviceSerNum[2], deviceSerNum[3], deviceSerNum[4],
			deviceSerNum[5], deviceSerNum[6], deviceSerNum[7], deviceSerNum[8], deviceSerNum[9]);
		memcpy(ConfigFirebase.Device_Ser_num,ConfigFirebase.Device_Db_ID,20);
		memcpy(&ConfigFirebase.Device_Db_ID[20],&device_ID[3],8);
		memcpy(ConfigFirebase.Device_Id,&device_ID[3],8);


		//Init firebase values
		UpdateStatus.ESP_Act_Ver = ParseFirmwareVersion((char *)(version_firmware));
		UpdateStatus.PSOC5_Act_Ver = ParseFirmwareVersion((char *)(PSOC5_version_firmware));

		//compatibilidad con versiones anteriores de firmwware
		if(UpdateStatus.PSOC5_Act_Ver <= 510){
			dispositivo_inicializado = 2;
		}

		Coms.StartConnection = true;
	#endif

}

void proceso_recepcion(void)
{
	static uint8 byte;
	static uint16 longitud_bloque = 0, len;
	static uint16 tipo_bloque = 0x0000;

	if(!isMainFwUpdateActive()){
		uint8_t ui8Tmp;
		len = serialLocal.available();
		if(len == 0)
		{
			if(cnt_fin_bloque == 0)
			{
				puntero_rx_local = 0;
				estado_recepcion = ESPERANDO_HEADER;
			}
			return;
		}
		cnt_fin_bloque = TIMEOUT_RX_BLOQUE;
		switch(estado_recepcion)
		{
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

void procesar_bloque(uint16 tipo_bloque){
	static int LastRecord =0;

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
				modifyCharacteristic(&buffer_rx_local[197], 10, VERSIONES_VERSION_FIRMWARE_CHAR_HANDLE);
				memcpy(PSOC5_version_firmware, &buffer_rx_local[197],10);
				modifyCharacteristic(version_firmware, 10, VERSIONES_VERSION_FIRM_BLE_CHAR_HANDLE);

				modifyCharacteristic(&buffer_rx_local[207], 2, RECORDING_REC_CAPACITY_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[209], 2, RECORDING_REC_LAST_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[211], 1, RECORDING_REC_LAPS_CHAR_HANDLE);

				modifyCharacteristic(&buffer_rx_local[212], 2, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
				memcpy(deviceSerNum, &buffer_rx_local[219], 10);			
				modifyCharacteristic(&buffer_rx_local[229], 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[231], 1, LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[232], 1, DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[233], 1, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[234], 2, COMS_FW_UPDATEMODE_CHAR_HANDLE);					

				#ifdef CONNECTED
					/************************ Set firebase Params **********************/
					memcpy(Params.autentication_mode, &buffer_rx_local[212],2);
					Params.inst_current_limit = buffer_rx_local[11];
					Params.potencia_contratada=buffer_rx_local[229]+buffer_rx_local[230]*0x100;
					Params.CDP 	  =  buffer_rx_local[232];
					
					if((buffer_rx_local[232] >> 1) && 0x01){
						Params.Tipo_Sensor    = (buffer_rx_local[232]  >> 4);
					}
					else{
						Params.Tipo_Sensor    = 0;
					}
					
					memcpy(Params.Fw_Update_mode, &buffer_rx_local[234],2);
					Comands.desired_current = buffer_rx_local[233];
					Coms.ETH.Auto = buffer_rx_local[240];
					Coms.Wifi.ON = buffer_rx_local[236];
					Coms.ETH.ON = buffer_rx_local[237];	
					modifyCharacteristic(&buffer_rx_local[236], 1, COMS_CONFIGURATION_WIFI_ON);
					if(Coms.ETH.Auto && !Params.Tipo_Sensor){
						modifyCharacteristic(&buffer_rx_local[237], 1, COMS_CONFIGURATION_ETH_ON);	
					}

				#endif

				startSystem();
				systemStarted = 1;

				cnt_timeout_inicio = TIMEOUT_INICIO;
				PSOC_inicializado = 1;
			}

		break;

		case BLOQUE_STATUS:
#ifdef CONNECTED
			if(buffer_rx_local[45]==0x36){
#else
			if(buffer_rx_local[25]==0x36){
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

				//Hilo piloto
				if((memcmp(&buffer_rx_local[1], status_hpt_anterior, 2) != 0)){
					dispositivo_inicializado = 2;
					#ifdef DEBUG
					Serial.printf("%c %c \n",buffer_rx_local[1],buffer_rx_local[2]);
					#endif
					memcpy(status_hpt_anterior, &buffer_rx_local[1], 2);
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

					//Si sale de C2 bloquear la siguiente carga
					if(memcmp(&buffer_rx_local[1],"C2",2) && memcmp(&buffer_rx_local[1],"B2",2)){
						Bloqueo_de_carga = Params.Tipo_Sensor;
						SendToPSOC5(Bloqueo_de_carga, BLOQUEO_CARGA);
					}
#endif
				}

				//Medidas
				modifyCharacteristic(&buffer_rx_local[3],  2, STATUS_ICP_STATUS_CHAR_HANDLE);						
				modifyCharacteristic(&buffer_rx_local[5],  2, STATUS_RCD_STATUS_CHAR_HANDLE);			
				modifyCharacteristic(&buffer_rx_local[7], 2, STATUS_CONN_LOCK_STATUS_CHAR_HANDLE);			
				modifyCharacteristic(&buffer_rx_local[9], 1, MEASURES_MAX_CURRENT_CABLE_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[10], 2, DOMESTIC_CONSUMPTION_DOMESTIC_CURRENT_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[12], 1, DOMESTIC_CONSUMPTION_REAL_CURRENT_LIMIT_CHAR_HANDLE);
				modifyCharacteristic(&buffer_rx_local[13], 1, ERROR_STATUS_ERROR_CODE_CHAR_HANDLE);	

				//FaseA	
				Status.Measures.instant_current = buffer_rx_local[14] + (buffer_rx_local[15] * 0x100);
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
				
					//Pasar datos a Status
					memcpy(Status.HPT_status, &buffer_rx_local[1], 2);
					Status.error_code = buffer_rx_local[13];
					
#ifdef CONNECTED					
					Status.ICP_status 		= (memcmp(&buffer_rx_local[3],  "CL" ,2) == 0 )? true : false;
					Status.DC_Leack_status  = (memcmp(&buffer_rx_local[5], "TR" ,2) >  0 )? true : false;
					Status.Con_Lock   		= (memcmp(&buffer_rx_local[7], "UL",3) == 0 )? true : false;			

					Status.Measures.max_current_cable = buffer_rx_local[9];					
					Status.Measures.instant_voltage   = buffer_rx_local[16] + (buffer_rx_local[17] * 0x100);
					Status.Measures.active_power      = buffer_rx_local[18] + (buffer_rx_local[19] * 0x100);
					Status.Measures.active_energy     = buffer_rx_local[20] + (buffer_rx_local[21] * 0x100) +(buffer_rx_local[22] * 0x1000) +(buffer_rx_local[23] * 0x10000);
					
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
			aut_semilla = (((int)buffer_rx_local[4]) * 0x100) + (int)buffer_rx_local[5];
			modifyCharacteristic(&buffer_rx_local[6], 6, TIME_DATE_CONNECTION_DATE_TIME_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[12], 6, TIME_DATE_DISCONNECTION_DATE_TIME_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[18], 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[24], 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);

			#ifdef CONNECTED
				int TimeStamp = (Convert_To_Epoch(&buffer_rx_local[6]));
				if(TimeStamp!=Status.Time.connect_date_time){
					Status.Time.connect_date_time = TimeStamp;
					ConfigFirebase.WriteTime = true;
				}
				TimeStamp = Convert_To_Epoch(&buffer_rx_local[12]);
				if(TimeStamp!=Status.Time.disconnect_date_time){
					Status.Time.disconnect_date_time = TimeStamp;
					ConfigFirebase.WriteTime = true;
				}
				TimeStamp = Convert_To_Epoch(&buffer_rx_local[18]);
				if(TimeStamp!=Status.Time.charge_start_time){
					Status.Time.charge_start_time = TimeStamp;
					ConfigFirebase.WriteTime = true;
				}
				TimeStamp = Convert_To_Epoch(&buffer_rx_local[24]);
				if(TimeStamp!=Status.Time.charge_stop_time){
					Status.Time.charge_stop_time = TimeStamp;
					ConfigFirebase.WriteTime = true;
				}
				TimeStamp = Convert_To_Epoch(buffer_rx_local);
				if(TimeStamp!=Status.Time.actual_time){
					Status.Time.actual_time = TimeStamp;
				}
				
			#endif
		}
		break;

		case MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);
			#ifdef DEBUG
			Serial.println("Instalation current limit Changed to" );
			Serial.print(buffer_rx_local[0]);
			#endif
			#ifdef CONNECTED
				Params.inst_current_limit = buffer_rx_local[0];
			#endif
		}	
		break;
	
		case MEASURES_CURRENT_COMMAND_CHAR_HANDLE:{
			Comands.desired_current = buffer_rx_local[0];
			Comands.Newdata = false;
			#ifdef DEBUG
			Serial.println("Current command received");
			Serial.println(Comands.desired_current);
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
			Comands.start=0;
			#ifdef DEBUG
			Serial.println("Start recibido");
			#endif
			modifyCharacteristic(buffer_rx_local, 1, CHARGING_BLE_MANUAL_START_CHAR_HANDLE);
		}
		break;

		case CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE:{
			Comands.stop=0;
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
		} 
		break;
		
		case VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE:{
		
			modifyCharacteristic(buffer_rx_local, 1, VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE);
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
			ESP.restart();
			//modifyCharacteristic(&buffer_rx_local[0], 1, RESET_RESET_CHAR_HANDLE);
		} 
		break;
		
		case ENERGY_RECORD_RECORD_CHAR_HANDLE:{
			#ifdef DEBUG
			Serial.println("Total record received");
			#endif
			memcpy(record_buffer,buffer_rx_local,20);

			int j = 20;
			bool end = false;
			for(int i =20;i<512;i+=4){		
				if(j<252){
					if(buffer_rx_local[j]==255 && buffer_rx_local[j+1]==255){
						end = true;
					}
					int PotLeida=buffer_rx_local[j]*0x100+buffer_rx_local[j+1];
					if(PotLeida >0 && PotLeida < 5500 && !end){
						record_buffer[i]   = buffer_rx_local[j];
						record_buffer[i+1] = buffer_rx_local[j+1];
						record_buffer[i+2] = 0;
						record_buffer[i+3] = 0;
					}
					else{
						record_buffer[i]   = 0;
						record_buffer[i+1] = 0;
						record_buffer[i+2] = 0;
						record_buffer[i+3] = 0;
					}

					j+=2;
				}
				else{
					record_buffer[i]   = 0;
					record_buffer[i+1] = 0;
					record_buffer[i+2] = 0;
					record_buffer[i+3] = 0;
				}				
			}

#ifdef CONNECTED
			//Si no estamos conectados por ble
			
			if(ConfigFirebase.InternetConection){
				if(Coms.ETH.ON || Coms.Wifi.ON){
					if(!serverbleGetConnected()){
						WriteFirebaseHistoric((char*)buffer_rx_local);
					}	
				}
			}

#endif
			modifyCharacteristic(record_buffer, 512, ENERGY_RECORD_RECORD_CHAR_HANDLE);
		} 
		break;
		
		case RECORDING_REC_LAST_CHAR_HANDLE:{
			#ifdef DEBUG
			printf("Me ha llegado una nueva recarga! %i\n", (buffer_rx_local[0] + buffer_rx_local[1]*0x100));
			#endif
			LastRecord = (buffer_rx_local[0] + buffer_rx_local[1]*0x100);
			modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_LAST_CHAR_HANDLE);
		} 
		break;
		
		case RECORDING_REC_INDEX_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_INDEX_CHAR_HANDLE);
		} 
		break;
		
		case RECORDING_REC_LAPS_CHAR_HANDLE:{
			uint8_t buffer[2];
			#ifdef DEBUG
			printf("Me ha llegado una nueva vuelta! %i\n", (buffer_rx_local[0] + buffer_rx_local[1]*0x100));
			#endif

			buffer[0] = (uint8)(LastRecord & 0x00FF);
			buffer[1] = (uint8)((LastRecord >> 8) & 0x00FF);

			modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_LAPS_CHAR_HANDLE);
			delay(10);
			SendToPSOC5((char*)buffer, 2, RECORDING_REC_INDEX_CHAR_HANDLE);
		} 
		break;
		
		case CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 2, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
			#ifdef DEBUG
			Serial.printf("Nueva autenticacion recibida! %c %c \n", buffer_rx_local[0],buffer_rx_local[1]);
			#endif
			#ifdef CONNECTED
				memcpy(Params.autentication_mode,buffer_rx_local,2);
			#endif
		} 
		break;
		
		case DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE);
			#ifdef DEBUG
			Serial.println("Potencia contratada cambiada a: ");
			Serial.print(buffer_rx_local[0]+buffer_rx_local[1]*100);
			#endif

			#ifdef CONNECTED
				Params.potencia_contratada=buffer_rx_local[0]+buffer_rx_local[1]*100;
			#endif
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
			xTaskCreate(UpdateTask,"TASK UPDATE",4096,NULL,1,NULL);
			updateTaskrunning=1;
		} 
		break;
		
		case DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE:{
			modifyCharacteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
			Params.CDP				  = buffer_rx_local[0];
			#ifdef CONNECTED			
				if((buffer_rx_local[0] >> 1) && 0x01){
					if(!Params.Tipo_Sensor){
						Params.Tipo_Sensor    = (buffer_rx_local[0]  >> 4);
						//Bloquear la carga hasta que encontremos el medidor
						if(Params.Tipo_Sensor){
							Coms.ETH.ON = true;
							Bloqueo_de_carga = 1;
							SendToPSOC5(Bloqueo_de_carga,BLOQUEO_CARGA);
							SendToPSOC5(1,COMS_CONFIGURATION_ETH_ON);
						}
						
					}
					else{
						Params.Tipo_Sensor    = (buffer_rx_local[0]  >> 4);
					}
				}
				else{
					Params.Tipo_Sensor = 0;
				}

				if(!Params.Tipo_Sensor){
					if(!Coms.ETH.Auto && Coms.ETH.DHCP){
						Coms.ETH.Auto = true;
						

						SendToPSOC5(0,COMS_CONFIGURATION_ETH_ON);
						SendToPSOC5(Coms.ETH.Auto,COMS_CONFIGURATION_ETH_AUTO);
					}
				}
			#endif
			#ifdef DEBUG_BLE
			Serial.printf("New CDP %i %i \n", Params.CDP, Params.Tipo_Sensor);
			#endif
		} 
		break;

#ifdef CONNECTED
		case COMS_CONFIGURATION_WIFI_ON:{
			Coms.Wifi.ON    =  buffer_rx_local[0];
			#ifdef DEBUG
			Serial.print("WIFI ON:");
			Serial.println(Coms.Wifi.ON);
			#endif
			modifyCharacteristic(buffer_rx_local , 1, COMS_CONFIGURATION_WIFI_ON);
		} 
		break;
		
		case COMS_CONFIGURATION_ETH_ON:{
			
			Coms.ETH.ON     =  Params.Tipo_Sensor ? true : buffer_rx_local[0];
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
		
		break;

		case COMS_FW_UPDATEMODE_CHAR_HANDLE:{
			memcpy(Params.Fw_Update_mode,buffer_rx_local,2);
			#ifdef DEBUG
			Serial.printf("Nuevo fw_update:%c %c \n", buffer_rx_local[0],buffer_rx_local[1]);
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

#ifdef USE_GROUPS
		case GROUPS_DEVICES_PART_1:{
            Serial.printf("Nuevo grupo recibido\n");
            char n[2];
            char ID[8];
            memcpy(n,buffer_rx_local,2);
            ChargingGroup.group_chargers.size=0;
            uint8_t limit = atoi(n) > 25? 25: atoi(n);
			
            for(uint8_t i=0; i<limit;i++){    
                for(uint8_t j =0; j< 8; j++){
                    ID[j]=(char)buffer_rx_local[2+i*9+j];
                }
                add_to_group(ID, get_IP(ID), &ChargingGroup.group_chargers);
                ChargingGroup.group_chargers.charger_table[i].Fase = buffer_rx_local[10+i*9]-'0';
                if(!memcmp(ID,ConfigFirebase.Device_Id,8)){
                    ChargingGroup.group_chargers.charger_table[i].Conected = true;
                    Params.Fase =buffer_rx_local[10+i*9]-'0';
                }
            }

            //Si tenemos mas de 25, las comprobaciones del grupo las realizamos en la segunda parte
            if(atoi(n)>25){
                break;
            }

            //si llega un grupo en el que no estoy, significa que me han sacado de el
            //cierro el coap y borro el grupo
            if(check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) == 255){
                if(ChargingGroup.Conected){
                    ChargingGroup.DeleteOrder = true;
                }
            }

            //Ponerme el primero en el grupo para indicar que soy el maestro
            if(ChargingGroup.Params.GroupMaster){
                if(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                    if(ChargingGroup.group_chargers.size > 0 && check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) != 255){
                        while(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                            carac_charger OldMaster=ChargingGroup.group_chargers.charger_table[0];
                            remove_from_group(OldMaster.name, &ChargingGroup.group_chargers);
                            add_to_group(OldMaster.name, OldMaster.IP, &ChargingGroup.group_chargers);
                            ChargingGroup.group_chargers.charger_table[ChargingGroup.group_chargers.size-1].Fase=OldMaster.Fase;
                        }
                        ChargingGroup.SendNewGroup = true;
                    }
                }
                //si soy el maestro, avisar a los nuevos de que son parte de mi grupo
                broadcast_a_grupo("Satrt client", 12);
            }
            print_table(ChargingGroup.group_chargers, "Grupo desde PSOC");
        }
        break;

        //Segunda parte del grupo de cargadores
        case GROUPS_DEVICES_PART_2:{
            Serial.printf("Segunda parte del grupo recibida\n");
            char n[2];
            char ID[8];
            memcpy(n,buffer_rx_local,2);
            
            for(uint8_t i=0; i<atoi(n);i++){    
                for(uint8_t j =0; j< 8; j++){
                    ID[j]=(char)buffer_rx_local[2+i*9+j];
                }
                add_to_group(ID, get_IP(ID), &ChargingGroup.group_chargers);
                ChargingGroup.group_chargers.charger_table[ChargingGroup.group_chargers.size-1].Fase = buffer_rx_local[10+i*9]-'0';

                if(!memcmp(ID,ConfigFirebase.Device_Id,8)){
                    Params.Fase =buffer_rx_local[10+i*9]-'0';
                }

            }

            //si llega un grupo en el que no estoy, significa que me han sacado de el
            //cierro el coap y borro el grupo
            if(check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) == 255){
                if(ChargingGroup.Conected){
					printf("!No estoy en el grupo!!!\n");
                    ChargingGroup.DeleteOrder = true;
                }
            }

            //Ponerme el primero en el grupo para indicar que soy el maestro
            if(ChargingGroup.Params.GroupMaster){
                if(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                    if(ChargingGroup.group_chargers.size > 0 && check_in_group(ConfigFirebase.Device_Id,&ChargingGroup.group_chargers ) != 255){
                        while(memcmp(ChargingGroup.group_chargers.charger_table[0].name,ConfigFirebase.Device_Id, 8)){
                            carac_charger OldMaster=ChargingGroup.group_chargers.charger_table[0];
                            remove_from_group(OldMaster.name, &ChargingGroup.group_chargers);
                            add_to_group(OldMaster.name, OldMaster.IP, &ChargingGroup.group_chargers);
                            ChargingGroup.group_chargers.charger_table[ChargingGroup.group_chargers.size-1].Fase=OldMaster.Fase;
                        }
                        ChargingGroup.SendNewGroup = true;
                    }
                }
                //si soy el maestro, avisar a los nuevos de que son parte de mi grupo
                broadcast_a_grupo("Satrt client", 12);
            }
            print_table(ChargingGroup.group_chargers, "Grupo desde PSOC");
			break;
        }


		case GROUPS_PARAMS:{
			memcpy(&ChargingGroup.Params,buffer_rx_local, 7);

			#ifdef DEBUG_GROUPS
			Serial.printf("Group active %i \n", ChargingGroup.Params.GroupActive);
			Serial.printf("Group master %i \n", ChargingGroup.Params.GroupMaster);
			Serial.printf("Group potencia_max %i \n", ChargingGroup.Params.potencia_max);
			Serial.printf("Group contract power %i \n", ChargingGroup.Params.ContractPower);
			Serial.printf("Group inst_max %i \n", ChargingGroup.Params.inst_max);
			Serial.printf("Group CDP %i \n", ChargingGroup.Params.CDP);	
			#endif	
			break;
		}
#endif

		case BLOQUEO_CARGA:{

			if(dispositivo_inicializado != 2){
				Bloqueo_de_carga = true;
			}

#ifdef USE_GROUPS

			//si somos parte de un grupo, debemos pedir permiso al maestro
			else if(ChargingGroup.Params.GroupActive || ChargingGroup.Finding ){
				if(ChargingGroup.Conected && ChargingGroup.ChargPerm){
					Bloqueo_de_carga = false;
					ChargingGroup.AskPerm = false;
				}
				//Pedir permiso
				else if(!ChargingGroup.AskPerm){
					ChargingGroup.AskPerm = true;
				}
			}
#endif
			//Si tenemos un medidor conectado, asta que no nos conectemos a el no permitimos la carga
			else if(Params.Tipo_Sensor){
				if(ContadorExt.ContadorConectado){
					Bloqueo_de_carga = false;
				}
			}

			//si no se cumple nada de lo anterior, permitimos la carga
			else{
				Bloqueo_de_carga = false;
			}
			SendToPSOC5(Bloqueo_de_carga, BLOQUEO_CARGA);
			break;
		}
#endif

		default:
		break;
	}
}
 
uint8_t sendBinaryBlock ( uint8_t *data, int len )
{
	if(mainFwUpdateActive)
	{
		int ret=0;
		ret = serialLocal.write(data, len);
		cnt_timeout_tx = 254;
		return ret;
	}

	return 1;
}

int controlSendToSerialLocal ( uint8_t * data, int len ){

	if(!mainFwUpdateActive){
	    int ret=0;
		ret = serialLocal.write(data, len);
		cnt_timeout_tx = TIMEOUT_TX_BLOQUE2;
		return ret;
	}
	return 0;
}

void deviceConnectInd ( void ){
	int random = 0;
	const void* outputvec1;
	// This event is received when device is connected over GATT level 

	srand(aut_semilla);
	random = rand();
	authChallengeQuery[7] = (uint8)(random >> 8);
	authChallengeQuery[6] = (uint8)random;
	random = rand();
	authChallengeQuery[5] = (uint8)(random >> 8);
	authChallengeQuery[4] = (uint8)random;
	random = rand();
	authChallengeQuery[3] = (uint8)(random >> 8);
	authChallengeQuery[2] = (uint8)random;
	random = rand();
	authChallengeQuery[1] = (uint8)(random >> 8);
	authChallengeQuery[0] = (uint8)random;

	outputvec1 = dev_auth_challenge(authChallengeQuery);

	memcpy(authChallengeReply, outputvec1, 8);

	//Delay para dar tiempo a conectar
	//vTaskDelay(pdMS_TO_TICKS(250));
	modifyCharacteristic(authChallengeQuery, 8, AUTENTICACION_MATRIX_CHAR_HANDLE);

	#ifdef DEBUG
	Serial.println("Sending authentication");
	#endif
	vTaskDelay(pdMS_TO_TICKS(500));
	AuthTimer = xTaskGetTickCount();
}

void deviceDisconnectInd ( void )
{
	authSuccess = 0;
	memset(authChallengeReply, 0x00, 8);
	//memset(authChallengeQuery, 0x00, 8);
	modifyCharacteristic(authChallengeQuery, 10, AUTENTICACION_MATRIX_CHAR_HANDLE);
	vTaskDelay(pdMS_TO_TICKS(500));
}

uint8_t isMainFwUpdateActive (void)
{
	return mainFwUpdateActive;
}

uint8_t setMainFwUpdateActive (uint8_t val )
{
	mainFwUpdateActive = val;
	return mainFwUpdateActive;
}

uint8_t setAuthToken ( uint8_t *data, int len ){
	if(!authSuccess){
		if(!memcmp(authChallengeReply, data, 8) || !memcmp(deviceSerNum, initialSerNum, 10))
		{
			AuthTimer=0;
			#ifdef DEBUG
			printf("authSuccess\r\n");
			#endif
			
			serverbleSetConnected(true);
			buffer_tx_local[0] = HEADER_TX;
			buffer_tx_local[1] = (uint8)(BLOQUE_STATUS >> 8);
			buffer_tx_local[2] = (uint8)(BLOQUE_STATUS);
			buffer_tx_local[3] = 2;
			buffer_tx_local[4] = serverbleGetConnected();
			buffer_tx_local[5] = ESTADO_NORMAL;
			controlSendToSerialLocal(buffer_tx_local, 6);
			authSuccess = 1;
		}
	}

	return 1;
}

uint8_t authorizedOK ( void ){
	return (authSuccess || mainFwUpdateActive == 1 );
}

void updateCharacteristic(uint8_t* data, uint16_t len, uint16_t attrHandle){
	serverbleSetCharacteristic ( data,len,attrHandle);
	return;
}

void modifyCharacteristic(uint8* data, uint16 len, uint16 attrHandle){
	serverbleSetCharacteristic ( data,len,attrHandle);
	return;
}

/************************************************
		Convertir fecha a timestamp epoch
************************************************/
int Convert_To_Epoch(uint8* data){
	struct tm t = {0};  // Initalize to all 0's
	t.tm_year = (data[2]!=0)?data[2]+100:0;  // This is year-1900, so 112 = 2012
	t.tm_mon  = (data[1]!=0)?data[1]-1:0;
	t.tm_mday = data[0];
	t.tm_hour = data[3];
	t.tm_min  = data[4];
	t.tm_sec  = data[5];
	return mktime(&t);
}

/************************************************
 * Update Tasks
 * **********************************************/
void UpdateTask(void *arg){
	UpdateStatus.InstalandoArchivo = true;
	#ifdef DEBUG
	Serial.println("\nComenzando actualizacion del PSOC5!!");
	#endif
	
	int Nlinea =0;
	uint8_t err = 0;
	String Buffer;

	unsigned long  siliconID;
	unsigned char siliconRev;
	unsigned char packetChkSumType;
	unsigned char arrayId; 
	unsigned short rowNum;
	unsigned short rowSize; 
	unsigned char checksum ;
	unsigned long blVer=0;
	unsigned char rowData[512];
	SPIFFS.begin();
	File file;
	file = SPIFFS.open("/FreeRTOS_V6.cyacd"); 	
	if(!file || file.size() == 0){ 
		file.close();
		SPIFFS.end();
		Serial.println("El spiffs parece cerrado, intentandolo otra vez"); 		
		SPIFFS.begin(0,"/spiffs",1,"PSOC5");
		file = SPIFFS.open("/FreeRTOS_V6.cyacd");
		if(!file || file.size() == 0){
			Serial.println("Imposible abrir");
		}
	}
	Serial.println("Tamaño del archivo!");
	Serial.println(file.size());
	if(file){
		//Leer la primera linea y obtener los datos del chip
		Buffer=file.readStringUntil('\n'); 
		int longitud = Buffer.length()-1;  
		unsigned char* b = (unsigned char*) Buffer.c_str(); 
		err=CyBtldr_ParseHeader((unsigned int)longitud,  b, &siliconID , &siliconRev ,&packetChkSumType);  
		err = CyBtldr_ParseRowData((unsigned int)longitud,b, &arrayId, &rowNum, rowData, &rowSize, &checksum);

		//Reiniciar la lectura del archivo
		file.seek(PRIMERA_FILA,SeekSet);

		err = CyBtldr_StartBootloadOperation(&serialLocal ,siliconID, siliconRev ,&blVer);
		
		while(1){
			delay(1);
			if (file.available() && err == 0) {   
				Buffer=file.readStringUntil('\n'); 
				longitud = Buffer.length()-1; 
				b = (unsigned char*) Buffer.c_str();     
				err = CyBtldr_ParseRowData((unsigned int)longitud,b, &arrayId, &rowNum, rowData, &rowSize, &checksum);
				if(err!=0){
					Serial.printf("Error 1%i \n", err);
				}
				if (err==0){
					err = CyBtldr_ProgramRow(arrayId, rowNum, rowData, rowSize);
				}	
						
				Nlinea++;
				Serial.printf("Lineas leidas: %u \n",Nlinea);
			}
			else{
				//End Bootloader Operation 
				CyBtldr_EndBootloadOperation(&serialLocal);		
				Serial.println("Actualizacion terminada!!");
				Serial.printf("Lineas leidas: %u \n",Nlinea);
				Serial.printf("Error %i \n", err);
				file.close();
				updateTaskrunning=0;
				setMainFwUpdateActive(0);
				UpdateStatus.InstalandoArchivo=0;
				if(!UpdateStatus.ESP_UpdateAvailable){
					Serial.println("Reiniciando en 4 segundos!"); 
					vTaskDelay(pdMS_TO_TICKS(4000));
					MAIN_RESET_Write(0);						
					ESP.restart();
				}
				else{
					UpdateStatus.PSOC5_UpdateAvailable = false;
					UpdateStatus.DobleUpdate 		   = true;
				}

				vTaskDelete(NULL);		
			}
			
		}
	}
}
/*********** Pruebas tar.gz **************/
#ifdef UPDATE_COMPRESSED
void UpdateCompressedTask(void *arg){

	Serial.println("Source filesystem Mount Successful :)");

	GzUnpacker *GZUnpacker = new GzUnpacker();

	GZUnpacker->haltOnError( true ); // stop on fail (manual restart/reset required)
	GZUnpacker->setupFSCallbacks( targzTotalBytesFn, targzFreeBytesFn2 ); // prevent the partition from exploding, recommended
	GZUnpacker->setGzProgressCallback( BaseUnpacker::defaultProgressCallback ); // targzNullProgressCallback or defaultProgressCallback
	GZUnpacker->setLoggerCallback( BaseUnpacker::targzPrintLoggerCallback  );    // gz log verbosity

	if( ! GZUnpacker->gzUpdater(tarGzFS, tarGzFile, false ) ) {
	Serial.printf("gzUpdater failed with return code #%d\n", GZUnpacker->tarGzGetError() );
	}
	tarGzFS.end();
}
#endif

void controlInit(void){

	//Freertos estatico
	xTaskCreateStatic(LedControl_Task,"TASK LEDS",4096*2,NULL,PRIORIDAD_LEDS,xLEDStack,&xLEDBuffer); 
	xTaskCreateStatic(controlTask,"TASK CONTROL",4096*6,NULL,PRIORIDAD_CONTROL,xControlStack,&xControlBuffer); 
	#ifdef CONNECTED
		//xTaskCreateStatic(ComsTask,"TASK COMS", 4096*4,NULL,PRIORIDAD_COMS,xComsStack, &xComsBuffer);
		xTaskCreate(ComsTask,"Task Coms",4096*2,NULL,PRIORIDAD_COMS,NULL);
		xTaskCreateStatic(Firebase_Conn_Task,"TASK FIREBASE", 4096*6,NULL,PRIORIDAD_FIREBASE,xFirebaseStack , &xFirebaseBuffer );
	#endif
}

uint32 GetStateTime(TickType_t xStart){
	return (pdTICKS_TO_MS(xTaskGetTickCount() - xStart));
}

/***************************************************
 *         Enviar nuevos valores al PSOC5
***************************************************/
void SendToPSOC5(uint8 data, uint16 attrHandle){

  cnt_timeout_tx = TIMEOUT_TX_BLOQUE2;
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(attrHandle >> 8);
  buffer_tx_local[2] = (uint8)(attrHandle);
  buffer_tx_local[3] = 1; //size
  buffer_tx_local[4] = data;
  controlSendToSerialLocal(buffer_tx_local, 5);
}

void SendToPSOC5(char *data, uint16 len, uint16 attrHandle){

  cnt_timeout_tx = TIMEOUT_TX_BLOQUE2;
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(attrHandle >> 8);
  buffer_tx_local[2] = (uint8)(attrHandle);
  buffer_tx_local[3] = len; //size
  memcpy(&buffer_tx_local[4],data,len);
  controlSendToSerialLocal(buffer_tx_local, len+4);
}


