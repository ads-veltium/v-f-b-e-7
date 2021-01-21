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


// TIPOS DE BLOQUE DE INTERCAMBIO CON BLE
#define BLOQUE_INICIALIZACION	0xFFFF
#define BLOQUE_STATUS			0xFFFE
#define BLOQUE_DATE_TIME		0xFFFD

#define LED_MAXIMO_PWM      1200     // Sobre 1200 de periodo

StaticTask_t xControlBuffer ;
static StackType_t xControlStack[4096*4] EXT_RAM_ATTR;
StaticTask_t xLEDBuffer ;
static StackType_t xLEDStack[4096*2] EXT_RAM_ATTR;

//Variables Firebase
carac_Update_Status UpdateStatus EXT_RAM_ATTR;
carac_Firebase_Configuration ConfigFirebase EXT_RAM_ATTR;
carac_Comands Comands      EXT_RAM_ATTR;
carac_Status  Status       EXT_RAM_ATTR;
carac_Params  Params       EXT_RAM_ATTR;
carac_Coms    Coms         EXT_RAM_ATTR;

/* VARIABLES BLE */
uint8 device_ID[16] = {"VCD17010001"};
uint8 deviceSerNum[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};     //{0x00, 0x00, 0x00, 0x00, 0x0B, 0xCD, 0x17, 0x01, 0x00, 0x05};
uint8 authChallengeReply[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// DACO changed size by crash
//uint8 authChallengeQuery[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};      //{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
uint8 authChallengeQuery[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};      //{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

uint8 contador_cent_segundos = 0, contador_cent_segundos_ant = 0;
uint32 cont_seg = 0, cont_seg_ant = 0, cont_min_ant = 0, cont_min=0, cont_hour=0, cont_hour_ant=0;
uint8 luminosidad, rojo, verde, azul, togle_led = 0, Led_color=VERDE ;
uint8 estado_inicial = 1;
uint8 estado_actual = ESTADO_ARRANQUE;
uint8 authSuccess = 0;
int aut_semilla = 0x0000;
uint8 NextLine=0;

uint8 cnt_fin_bloque = 0;
int estado_recepcion = 0;
uint8 buffer_tx_local[550];
uint8 buffer_rx_local[550];
uint16 puntero_rx_local = 0;
uint8 updateTaskrunning=0;
uint8 cnt_timeout_tx = 0;

// initial serial number
uint8 initialSerNum[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8 mainFwUpdateActive = 0;

uint8 dispositivo_inicializado = 0;
uint8 cnt_timeout_inicio = TIMEOUT_INICIO;
uint16 cnt_repeticiones_inicio = 100;	//1000;

uint8 status_hpt_anterior[2] = {'0','V' };
uint16 inst_current_anterior = 0x0000;
uint16 cnt_diferencia = 30;

uint8 version_firmware[11] = {"VBLE2_0500"};	
uint8 PSOC5_version_firmware[11] = {"VELT2_0400"};		

uint8 systemStarted = 0;

void startSystem(void);

void StackEventHandler( uint32 eventCode, void *eventParam );
void modifyCharacteristic(uint8* data, uint16 len, uint16 attrHandle);
void proceso_recepcion(void);

void Disable_VELT1_CHARGER_services(void);

/*******************************************************************************
 * Rutina de atencion a inerrupcion de timer (10mS)
 ********************************************************************************/

hw_timer_t * timer10ms = NULL;
hw_timer_t * timer1s = NULL;
HardwareSerialMOD serialLocal(2);

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
						if(!dispositivo_inicializado){
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
									cnt_repeticiones_inicio = 100;		//1000;								
									//startSystem();
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
						//startSystem();
						cnt_timeout_tx = TIMEOUT_TX_BLOQUE;
						estado_actual = ESTADO_NORMAL;
						break;
					case ESTADO_NORMAL:
						proceso_recepcion();	
						if(cnt_timeout_tx == 0)
						{
							cnt_timeout_tx = TIMEOUT_TX_BLOQUE;
							buffer_tx_local[0] = HEADER_TX;
							buffer_tx_local[1] = (uint8)(BLOQUE_STATUS >> 8);
							buffer_tx_local[2] = (uint8)BLOQUE_STATUS;
							buffer_tx_local[3] = 1;
							buffer_tx_local[4] = serverbleGetConnected();
							buffer_tx_local[5] = ESTADO_NORMAL;
							serialLocal.write(buffer_tx_local, 6);
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
					vTaskDelay(500 / portTICK_PERIOD_MS);
					xTaskCreate(UpdateTask,"TASK UPDATE",4096,NULL,1,NULL);
					updateTaskrunning=1;
				}				
			}

		}

		
		// Eventos 1 Seg
		if(cont_seg != cont_seg_ant){
			cont_seg_ant = cont_seg;

			if (Comands.start){
				cnt_timeout_tx = TIMEOUT_TX_BLOQUE;
                buffer_tx_local[0] = HEADER_TX;
                buffer_tx_local[1] = (uint8)(CHARGING_BLE_MANUAL_START_CHAR_HANDLE >> 8);
                buffer_tx_local[2] = (uint8)(CHARGING_BLE_MANUAL_START_CHAR_HANDLE);
                buffer_tx_local[3] = 5; //size
				buffer_tx_local[4] = 1; //size
                controlSendToSerialLocal(buffer_tx_local, 5);              
				}
				
			else if (Comands.stop)
			{
				cnt_timeout_tx = TIMEOUT_TX_BLOQUE;
				buffer_tx_local[0] = HEADER_TX;
				buffer_tx_local[1] = (uint8)(CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE >> 8);
				buffer_tx_local[2] = (uint8)(CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE);
				buffer_tx_local[3] = 5; //size
				buffer_tx_local[4] = 1; //size
				controlSendToSerialLocal(buffer_tx_local, 5);
			}
		
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

		vTaskDelay(pdMS_TO_TICKS(15));	
	}
}

void startSystem(void){
	
	Serial.printf("startSystem(): device SN = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",
		deviceSerNum[0], deviceSerNum[1], deviceSerNum[2], deviceSerNum[3], deviceSerNum[4],
		deviceSerNum[5], deviceSerNum[6], deviceSerNum[7], deviceSerNum[8], deviceSerNum[9]);
	dev_auth_init((void const*)&deviceSerNum);

	//serverbleStartAdvertising();
	#ifdef USE_WIFI
		//Get Device FirebaseDB ID
		sprintf(ConfigFirebase.Device_Db_ID,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",deviceSerNum[0], deviceSerNum[1], deviceSerNum[2], deviceSerNum[3], deviceSerNum[4],
			deviceSerNum[5], deviceSerNum[6], deviceSerNum[7], deviceSerNum[8], deviceSerNum[9]);
		memcpy(&ConfigFirebase.Device_Db_ID[20],&device_ID[3],8);

		//Init firebase values
		UpdateStatus.ESP_Act_Ver = ParseFirmwareVersion((char *)(version_firmware));
		UpdateStatus.PSOC5_Act_Ver = ParseFirmwareVersion((char *)(PSOC5_version_firmware));
	#endif

}

void proceso_recepcion(void)
{
	static uint8 byte;
	static uint16 longitud_bloque = 0, len;
	static uint16 tipo_bloque = 0x0000;

	if(isMainFwUpdateActive()){
		
	}
	else{
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
					longitud_bloque = 512;
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
	if(BLOQUE_INICIALIZACION == tipo_bloque){	
		if (!systemStarted) {
			memcpy(device_ID, buffer_rx_local, 11);
			esp_ble_gap_set_device_name((const char *)device_ID);
			changeAdvName(device_ID);
			printf("Change name set device name to %s\r\n",device_ID);
			modifyCharacteristic(device_ID, 11, VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[11], 1, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[12], 2, TIME_DATE_DELTA_DELAY_FOR_CHARGING_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[14], 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[20], 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[26], 1, CHARGING_INSTANT_DELAYED_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[27], 1, CHARGING_START_STOP_START_MODE_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[28], 168, SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[196], 1, VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[197], 1, VCD_NAME_USERS_UI_X_USER_ID_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[198], 1, VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[199], 10, VERSIONES_VERSION_FIRMWARE_CHAR_HANDLE);
			memcpy(PSOC5_version_firmware, &buffer_rx_local[199],10);
			modifyCharacteristic(version_firmware, 10, VERSIONES_VERSION_FIRM_BLE_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[209], 2, RECORDING_REC_CAPACITY_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[211], 2, RECORDING_REC_LAST_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[213], 1, RECORDING_REC_LAPS_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[214], 2, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[216], 1, DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[217], 1, DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[218], 1, DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE);
			memcpy(deviceSerNum, &buffer_rx_local[219], 10);			
			modifyCharacteristic(&buffer_rx_local[229], 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[231], 1, LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE);
			luminosidad=buffer_rx_local[231];
			modifyCharacteristic(&buffer_rx_local[232], 1, DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
			Serial.println(buffer_rx_local[232]);
			modifyCharacteristic(&buffer_rx_local[233], 1, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);

			Serial.println(buffer_rx_local[233]);
						
			startSystem();
			systemStarted = 1;

			cnt_timeout_inicio = TIMEOUT_INICIO;
			dispositivo_inicializado = 1;
		}	
	}
	else if(BLOQUE_STATUS == tipo_bloque)
	{		
		if(buffer_rx_local[89]==0x36){
			//Leds
			modifyCharacteristic(buffer_rx_local, 1, LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[1], 1, LED_LUMIN_COLOR_R_LED_COLOUR_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[2], 1, LED_LUMIN_COLOR_G_LED_COLOUR_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[3], 1, LED_LUMIN_COLOR_B_LED_COLOUR_CHAR_HANDLE);

			luminosidad = buffer_rx_local[0];
			rojo = buffer_rx_local[1];
			verde = buffer_rx_local[2];
			azul = buffer_rx_local[3];

			if(rojo ==100){
				Led_color=ROJO;
			}
			else if (azul==100){
				Led_color=AZUL_OSCURO;
			}
			else if (verde==100){
				Led_color=VERDE;
			}

			//Hilo piloto
			modifyCharacteristic(&buffer_rx_local[4], 2, STATUS_HPT_STATUS_CHAR_HANDLE);

			if((memcmp(&buffer_rx_local[4], status_hpt_anterior, 2) != 0) && (serverbleGetConnected()))
			{
				memcpy(status_hpt_anterior, &buffer_rx_local[4], 2);
				serverbleNotCharacteristic(&buffer_rx_local[4], 2, STATUS_HPT_STATUS_CHAR_HANDLE); 
			}

			//Medidas
			modifyCharacteristic(&buffer_rx_local[6], 2, STATUS_ICP_STATUS_CHAR_HANDLE);						
			modifyCharacteristic(&buffer_rx_local[10], 2, STATUS_RCD_STATUS_CHAR_HANDLE);			
			modifyCharacteristic(&buffer_rx_local[12], 2, STATUS_CONN_LOCK_STATUS_CHAR_HANDLE);			
			modifyCharacteristic(&buffer_rx_local[14], 1, MEASURES_MAX_CURRENT_CABLE_CHAR_HANDLE);	
			modifyCharacteristic(&buffer_rx_local[16], 2, MEASURES_INST_CURRENT_CHAR_HANDLE);
			if(((Status.Measures.instant_current) != (inst_current_anterior)) && (serverbleGetConnected())&& (--cnt_diferencia == 0))
			{
				cnt_diferencia = 2; // A.D.S. Cambiado de 30 a 5
				inst_current_anterior = Status.Measures.instant_current;
				serverbleNotCharacteristic(&buffer_rx_local[16], 2, MEASURES_INST_CURRENT_CHAR_HANDLE); 
			}

			modifyCharacteristic(&buffer_rx_local[18], 2, MEASURES_INST_VOLTAGE_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[20], 2, MEASURES_ACTIVE_POWER_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[22], 2, MEASURES_REACTIVE_POWER_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[24], 4, MEASURES_ACTIVE_ENERGY_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[28], 4, MEASURES_REACTIVE_ENERGY_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[32], 4, MEASURES_APPARENT_POWER_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[36], 1, MEASURES_POWER_FACTOR_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[38], 2, DOMESTIC_CONSUMPTION_DOMESTIC_CURRENT_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[40], 1, DOMESTIC_CONSUMPTION_REAL_CURRENT_LIMIT_CHAR_HANDLE);
			modifyCharacteristic(&buffer_rx_local[41], 1, ERROR_STATUS_ERROR_CODE_CHAR_HANDLE);		
			

			//Pasar datos a Status
			memcpy(Status.HPT_status, &buffer_rx_local[4], 2);
			memcpy(Status.ICP_status, &buffer_rx_local[6], 2);
			memcpy(Status.DC_Leack_status, &buffer_rx_local[10], 2);
			memcpy(Status.Con_Lock, &buffer_rx_local[12], 2);
			Status.Measures.max_current_cable=buffer_rx_local[14];
			Status.Measures.instant_current = buffer_rx_local[16] + (buffer_rx_local[17] * 0x100);
			Status.Measures.instant_voltage = buffer_rx_local[18] + (buffer_rx_local[19] * 0x100);
			Status.Measures.active_power = buffer_rx_local[20] + (buffer_rx_local[21] * 0x100);
			Status.Measures.reactive_power = buffer_rx_local[22] + (buffer_rx_local[23] * 0x100);
			Status.Measures.active_energy = buffer_rx_local[24] + (buffer_rx_local[25] * 0x100) +(buffer_rx_local[26] * 0x1000) +(buffer_rx_local[27] * 0x10000);
			Status.Measures.active_energy = buffer_rx_local[28] + (buffer_rx_local[29] * 0x100) +(buffer_rx_local[30] * 0x1000) +(buffer_rx_local[32] * 0x10000);			
			Status.Measures.active_energy = buffer_rx_local[32] + (buffer_rx_local[33] * 0x100) +(buffer_rx_local[34] * 0x1000) +(buffer_rx_local[35] * 0x10000);		
			Status.Measures.power_factor = buffer_rx_local[36];
			Status.Measures.active_power = buffer_rx_local[38] + (buffer_rx_local[39] * 0x100);
			Status.error_code = buffer_rx_local[41];


			#ifdef USE_WIFI
				if(ConfigFirebase.FirebaseConnected && UpdatefireBaseData){
					UpdateFirebaseStatus();
				}
			#endif
		}
	}
	else if(BLOQUE_DATE_TIME == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 6, TIME_DATE_DATE_TIME_CHAR_HANDLE);
		aut_semilla = (((int)buffer_rx_local[4]) * 0x100) + (int)buffer_rx_local[5];
		modifyCharacteristic(&buffer_rx_local[6], 6, TIME_DATE_CONNECTION_DATE_TIME_CHAR_HANDLE);
		modifyCharacteristic(&buffer_rx_local[12], 6, TIME_DATE_DISCONNECTION_DATE_TIME_CHAR_HANDLE);
		modifyCharacteristic(&buffer_rx_local[18], 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
		modifyCharacteristic(&buffer_rx_local[24], 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);
	}
	else if(MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);
	}
	else if(MEASURES_CURRENT_COMMAND_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
	}
	else if(TIME_DATE_DELTA_DELAY_FOR_CHARGING_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 2, TIME_DATE_DELTA_DELAY_FOR_CHARGING_CHAR_HANDLE);
	}
	else if(TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
	}
	else if(TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);
	}
	else if(CHARGING_INSTANT_DELAYED_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, CHARGING_INSTANT_DELAYED_CHAR_HANDLE);
	}
	else if(CHARGING_START_STOP_START_MODE_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, CHARGING_START_STOP_START_MODE_CHAR_HANDLE);
	}
	else if(CHARGING_BLE_MANUAL_START_CHAR_HANDLE == tipo_bloque)
	{
		Comands.start=0;
		modifyCharacteristic(buffer_rx_local, 1, CHARGING_BLE_MANUAL_START_CHAR_HANDLE);
	}
	else if(CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE == tipo_bloque)
	{
		Comands.stop=0;
		modifyCharacteristic(buffer_rx_local, 1, CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE);
	}
	else if(SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 168, SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);
	}
	else if(VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_HANDLE == tipo_bloque)
	{
		memcpy(device_ID, buffer_rx_local, 11);
		esp_ble_gap_set_device_name((const char *)device_ID);
		changeAdvName(device_ID);

		updateCharacteristic(device_ID, 11, VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_HANDLE);
	}
	else if(VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE);
	}
	else if(VCD_NAME_USERS_UI_X_USER_ID_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, VCD_NAME_USERS_UI_X_USER_ID_CHAR_HANDLE);
	}
	else if(VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE);
	}
	else if(TEST_LAUNCH_RCD_PE_TEST_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, TEST_LAUNCH_RCD_PE_TEST_CHAR_HANDLE);
	}
	else if(TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE);
	}
	else if(TEST_LAUNCH_MCB_TEST_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, TEST_LAUNCH_MCB_TEST_CHAR_HANDLE);
	}
	else if(TEST_MCB_TEST_RESULT_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, TEST_MCB_TEST_RESULT_CHAR_HANDLE);
	}
	else if(MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_CHAR_HANDLE);
	}
	else if(LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE == tipo_bloque)
	{
		luminosidad = buffer_rx_local[0];
		modifyCharacteristic(&buffer_rx_local[0], 1, LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE);
	}
	else if(LED_LUMIN_COLOR_R_LED_COLOUR_CHAR_HANDLE == tipo_bloque)
	{
		rojo = buffer_rx_local[0];
		modifyCharacteristic(&buffer_rx_local[0], 1, LED_LUMIN_COLOR_R_LED_COLOUR_CHAR_HANDLE);
	}
	else if(LED_LUMIN_COLOR_G_LED_COLOUR_CHAR_HANDLE == tipo_bloque)
	{
		verde = buffer_rx_local[0];
		modifyCharacteristic(&buffer_rx_local[0], 1, LED_LUMIN_COLOR_G_LED_COLOUR_CHAR_HANDLE);
	}
	else if(LED_LUMIN_COLOR_B_LED_COLOUR_CHAR_HANDLE == tipo_bloque)
	{
		azul = buffer_rx_local[0];
		modifyCharacteristic(&buffer_rx_local[0], 1, LED_LUMIN_COLOR_B_LED_COLOUR_CHAR_HANDLE);
	}
	else if(RESET_RESET_CHAR_HANDLE == tipo_bloque)
	{	
		//modifyCharacteristic(&buffer_rx_local[0], 1, RESET_RESET_CHAR_HANDLE);
	}
	else if(ENERGY_RECORD_RECORD_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 512, ENERGY_RECORD_RECORD_CHAR_HANDLE);
	}
	else if(RECORDING_REC_CAPACITY_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_CAPACITY_CHAR_HANDLE);
		
	}
	else if(RECORDING_REC_LAST_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_LAST_CHAR_HANDLE);
	}
	else if(RECORDING_REC_INDEX_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_INDEX_CHAR_HANDLE);
	}
	else if(RECORDING_REC_LAPS_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 2, RECORDING_REC_LAPS_CHAR_HANDLE);
	}
	else if(CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 2, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
	}
	else if(DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE);
	}
	else if(DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE);
	}
	else if(DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE);
	}
	else if(DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 2, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE);
		Serial.println(buffer_rx_local[0]+buffer_rx_local[1]*100);
	}
	else if(ERROR_STATUS_ERROR_CODE_CHAR_HANDLE == tipo_bloque)
	{
		modifyCharacteristic(buffer_rx_local, 1, ERROR_STATUS_ERROR_CODE_CHAR_HANDLE);
	}
	else if(BOOT_LOADER_LOAD_SW_APP_CHAR_HANDLE== tipo_bloque){
		xTaskCreate(UpdateTask,"TASK UPDATE",4096,NULL,1,NULL);
		updateTaskrunning=1;
	}
	else if (DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE== tipo_bloque){
		modifyCharacteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
	}
	else if (DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE== tipo_bloque){
		modifyCharacteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE);
	}		
}
 
uint8_t sendBinaryBlock ( uint8_t *data, int len )
{
	if(mainFwUpdateActive)
	{
		int ret=0;
		ret = serialLocal.write(data, len);
		cnt_timeout_tx = TIMEOUT_TX_BLOQUE;
		return ret;
	}

	return 1;
}

int controlSendToSerialLocal ( uint8_t * data, int len ){
	if(!isMainFwUpdateActive()){
	    int ret=0;
		ret = serialLocal.write(data, len);
		cnt_timeout_tx = TIMEOUT_TX_BLOQUE;
		return ret;
	}
	return 0;
}

void deviceConnectInd ( void ){
	int random = 0;
	const void* outputvec1;
	// This event is received when device is connected over GATT level 

	luminosidad = 50;
	Led_color=BLANCO;
	

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
	vTaskDelay(pdMS_TO_TICKS(250));
	modifyCharacteristic(authChallengeQuery, 8, AUTENTICACION_MATRIX_CHAR_HANDLE);
	Serial.print("Sending authentication");
	vTaskDelay(pdMS_TO_TICKS(250));

}

void deviceDisconnectInd ( void )
{
	luminosidad = 50;
	Led_color=BLANCO;
	authSuccess = 0;
	memset(authChallengeReply, 0x00, 8);
	//memset(authChallengeQuery, 0x00, 8);
	modifyCharacteristic(authChallengeQuery, 10, AUTENTICACION_MATRIX_CHAR_HANDLE);
	vTaskDelay(500/portTICK_PERIOD_MS);
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

uint8_t setAuthToken ( uint8_t *data, int len )
{
	if(!authSuccess)
	{
		printf("%s %s \r\n",deviceSerNum, initialSerNum);
		if(!memcmp(authChallengeReply, data, 8) || !memcmp(deviceSerNum, initialSerNum, 10))
		{
			printf("authSuccess\r\n");
			authSuccess = 1;
		}
	}

	return 1;
}

uint8_t authorizedOK ( void )
{
	return (authSuccess || mainFwUpdateActive == 1 );
}

void updateCharacteristic(uint8_t* data, uint16_t len, uint16_t attrHandle) 
{
	serverbleSetCharacteristic ( data,len,attrHandle);
	return;
}

void modifyCharacteristic(uint8* data, uint16 len, uint16 attrHandle)
{
	serverbleSetCharacteristic ( data,len,attrHandle);
	return;
}

void Disable_VELT1_CHARGER_services(void)
{

}


/************************************************
 * Update Tasks
 * **********************************************/
void UpdateTask(void *arg){
	Serial.println("\nComenzando actualizacion del PSOC5!!");
	
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

	SPIFFS.begin(1,"/spiffs",1,"PSOC5");
	File file = SPIFFS.open("/FreeRTOS_V6.cyacd"); 
	if(!file || file.size() == 0){ 
		Serial.println("Failed to open file for reading , intentandolo otra vez"); 
		
		file = SPIFFS.open("/FreeRTOS_V6.cyacd");
		if(!file || file.size() == 0){
			Serial.println("Imposible abrir");
		}
	}
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
		Serial.println(err);
		
		while(1){
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
				Serial.println("Reiniciando en 4 segundos!"); 
				vTaskDelay(pdMS_TO_TICKS(4000));
				MAIN_RESET_Write(0);						
				ESP.restart();
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
	//xTaskCreate(controlTask,"TASK CONTROL",4096*2,NULL,1,NULL);
	//Freertos estatico
	xTaskCreateStatic(LedControl_Task,"TASK LEDS",4096,NULL,PRIORIDAD_LEDS,xLEDStack,&xLEDBuffer); 
	xTaskCreateStatic(controlTask,"TASK CONTROL",4096*4,NULL,PRIORIDAD_CONTROL,xControlStack,&xControlBuffer); 
}


