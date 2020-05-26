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
#include <stdio.h>
#include <stdlib.h>
#include <HardwareSerial.h>

#include <math.h>

#include "control.h"
#include "serverble.h"
#include "controlLed.h"
#include "DRACO_IO.h"


#include "tipos.h"
#include "dev_auth.h"

// TIPOS DE BLOQUE DE INTERCAMBIO CON BLE
#define BLOQUE_INICIALIZACION	0xFFFF
#define BLOQUE_STATUS			0xFFFE
#define BLOQUE_DATE_TIME		0xFFFD

#define LED_MAXIMO_PWM      1200     // Sobre 1200 de periodo

/* VARIABLES BLE */
uint8 device_ID[16] = {"VCD17010001"};
uint8 device_SN[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};     //{0x00, 0x00, 0x00, 0x00, 0x0B, 0xCD, 0x17, 0x01, 0x00, 0x05};
uint8 autent_token[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// DACO changed size by crash
//uint8 autent_matrix[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};      //{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
uint8 autent_matrix[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};      //{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

uint8 contador_cent_segundos = 0, contador_cent_segundos_ant = 0;
uint32 cont_seg = 0, cont_seg_ant = 0;
uint8 luminosidad, rojo, verde, azul, togle_led = 0 ; // cnt_parpadeo = 0;
int cnt_parpadeo = 0;
uint8 estado_inicial = 1;
uint8 estado_actual = ESTADO_ARRANQUE;
uint8 autentificacion_recibida = 1;
int aut_semilla = 0x0000;

uint8 cnt_fin_bloque = 0;
int estado_recepcion = 0;
uint8 buffer_tx_local[550];
uint8 buffer_rx_local[550];
uint16 puntero_rx_local = 0;
uint8 cnt_timeout_tx = 0;

uint8 dummy_array[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


uint8 modo_telecarga_micro_principal = 0;

uint8 dispositivo_inicializado = 0;
uint8 cnt_timeout_inicio = TIMEOUT_INICIO;
uint16 cnt_repeticiones_inicio = 100;	//1000;

caract_measures measures;
caract_charge_config charge_config;
caract_name_users name_users;

uint8 status_hpt_anterior[2] = {'0','V' };
uint16 inst_current_anterior = 0x0000;
uint16 inst_current_actual = 0x0000;
uint16 cnt_diferencia = 30;

uint8 version_firmware[11] = {"VBLE1_0504"};		// 17122019 Bootloader estable

void inciar_sistema(void);
void StackEventHandler( uint32 eventCode, void *eventParam );
void Modify_VELT1_CHARGER_characteristic(uint8* Data, uint16 len, uint16 attrHandle);
void procesar_bloque(uint16 tipo_bloque);
void proceso_recepcion(void);

void Disable_VELT1_CHARGER_services(void);

uint32 InterruptHpn;


uint8 busyStatus;

double flick1, flick2;
int flick3,lum;
int TIME_PARPADEO_VARIABLE = 600;

/*******************************************************************************
 * Rutina de atencion a inerrupcion de timer (10mS)
 ********************************************************************************/

hw_timer_t * timer10ms = NULL;
hw_timer_t * timer1s = NULL;

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
	cont_seg++;
}

void configTimers ( void )
{
	//1 tick take 1/(80MHZ/80) = 1us so we set divider 80 and count up
	timer10ms = timerBegin(0, 80, true);
	timer1s = timerBegin(1, 80, true);
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
	return;
}



HardwareSerial serialLocal(2);

void controlTask(void *arg) 
{	
	// Inicia el timer de 10mS i 1 segundo
	configTimers();

	serialLocal.begin(115200, SERIAL_8N1, 16, 17); // pins: rx, tx

	// INICIALIZO ELEMENTOS PARA AUTENTICACION
	srand(aut_semilla);


	Modify_VELT1_CHARGER_characteristic(autent_matrix, 8, AUTENTICACION_MATRIX_CHAR_HANDLE);


	MAIN_RESET_Write(1);        // Permito el arranque del micro principal
	
	while(1)
	{
		// Eventos 10 mS
		if(contador_cent_segundos != contador_cent_segundos_ant)
		{
			contador_cent_segundos_ant = contador_cent_segundos;

			if ((luminosidad & 0x80) && (rojo == 0) && (verde == 0)) 
			{                
				lum = (luminosidad & 0x7F);
				flick1 = (++cnt_parpadeo)* 2 * M_PI / TIME_PARPADEO_VARIABLE;
				flick2 = (1 + sin (flick1))/2;
				flick2 = flick2*flick2; /* A.D.S. Comprobar efecto óptico */
				flick3 = lum * flick2;
				LED_Control (1+(uint8)flick3,rojo,verde,azul);
				if (cnt_parpadeo == TIME_PARPADEO_VARIABLE)
				{
					cnt_parpadeo = 0;
					TIME_PARPADEO_VARIABLE = 1180 - inst_current_actual*3/10; // A.D.S. 32A=2,2seg 6A=10seg
				}
			}
			else if (luminosidad & 0x80)
			{
				if(++cnt_parpadeo >= TIME_PARPADEO)
				{
					cnt_parpadeo = 0;
					if(togle_led == 0)
					{
						togle_led = 1;
						LED_Control(0, rojo, verde, azul);
					}
					else
					{
						togle_led = 0;
						LED_Control((luminosidad & 0x7F), rojo, verde, azul);
					}
				}
			}

			if(modo_telecarga_micro_principal == 0)
			{
				switch (estado_actual)
				{
					case ESTADO_ARRANQUE:
						if(!dispositivo_inicializado)
						{
							proceso_recepcion();
							if(--cnt_timeout_inicio == 0)
							{
								cnt_timeout_inicio = TIMEOUT_INICIO;
								buffer_tx_local[0] = HEADER_TX;
								buffer_tx_local[1] = (uint8)(BLOQUE_INICIALIZACION >> 8);
								buffer_tx_local[2] = (uint8)BLOQUE_INICIALIZACION;
								buffer_tx_local[3] = 1;
								buffer_tx_local[4] = 0;
								serialLocal.write(buffer_tx_local, 5);
								if(--cnt_repeticiones_inicio == 0)
								{
									cnt_repeticiones_inicio = 100;		//1000;
									inciar_sistema();
									modo_telecarga_micro_principal = 1;
									LED_Control(100, 10, 10, 10);
								}
							}
						}
						else
						{
							serialLocal.flush();
							estado_actual = ESTADO_INICIALIZACION;
						}
						break;
					case ESTADO_INICIALIZACION:
						inciar_sistema();
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
			else
			{
				proceso_recepcion();
			}
		}

		// Eventos 1 Seg
		if(cont_seg != cont_seg_ant)
		{
			cont_seg_ant = cont_seg;
		}

		vTaskDelay(5 / portTICK_PERIOD_MS);	
	}
}

void inciar_sistema(void)
{
	dev_auth_init((void const*)&device_SN);
}

void LED_Control(uint8_t luminosity, uint8_t r_level, uint8_t g_level, uint8_t b_level)
{
	if(modo_telecarga_micro_principal)
		return;
	displayAll(luminosity,r_level,g_level,b_level);
	return;
}

void proceso_recepcion(void)
{
	static uint8 byte, inicio = 0, dummy_8;
	static uint16 longitud_bloque = 0, len, i;
	static uint16 tipo_bloque = 0x0000;

	if(modo_telecarga_micro_principal)
	{
		LED_Control(10, 10, 10, 10);

		puntero_rx_local = 0;
		len = serialLocal.available();
		if(len != 0)
		{
			delay(10); // A.D.S. Disminuido delay de 100 a 10
			len = serialLocal.available();
			LED_Control(10, 0, 0, 10);
			for(i = 0; i < len; i++)
			{
				//dummy_8 = (uint8)SERIAL_LOCAL_UartGetByte();
				serialLocal.read(&dummy_8, 1);
				if(dummy_8 == 0x01 && inicio == 0)
				{
					puntero_rx_local = 0;
					inicio = 1;
				}
				buffer_rx_local[puntero_rx_local] = dummy_8;
				if(++puntero_rx_local == 4)
				{
					longitud_bloque = buffer_rx_local[2] + (0x100 * buffer_rx_local[3]);
				}
				if(dummy_8 == 0x17 && inicio != 0 && puntero_rx_local >= (longitud_bloque + 7))
				{
					inicio = 0;
					serverbleNotCharacteristic(buffer_rx_local, puntero_rx_local, BOOT_LOADER_BINARY_BLOCK_CHAR_HANDLE); 
					puntero_rx_local = 0;
				}

			}
		}	
	}
	else
	{
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
			case ESPERANDO_LONGITUD:
				serialLocal.read(&ui8Tmp,1);
				longitud_bloque = ui8Tmp;
				if(longitud_bloque == 0)
					longitud_bloque = 512;
				puntero_rx_local = 0;
				estado_recepcion = RECIBIENDO_BLOQUE;
				if(--len == 0)
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

void procesar_bloque(uint16 tipo_bloque)
{
	if(BLOQUE_INICIALIZACION == tipo_bloque)
	{
		memcpy(device_ID, buffer_rx_local, 11);
		esp_ble_gap_set_device_name((const char *)device_ID);
		changeAdvName(device_ID);
		printf("Change name set device name to %s\r\n",device_ID);
		Modify_VELT1_CHARGER_characteristic(device_ID, 11, VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[11], 1, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);

		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[11], 1, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);		// Actualizo "current_command" con el valor mas alto

		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[12], 2, TIME_DATE_DELTA_DELAY_FOR_CHARGING_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[14], 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[20], 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[26], 1, CHARGING_INSTANT_DELAYED_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[27], 1, CHARGING_START_STOP_START_MODE_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[28], 168, SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[196], 1, VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[197], 1, VCD_NAME_USERS_UI_X_USER_ID_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[198], 1, VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[199], 10, VERSIONES_VERSION_FIRMWARE_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(version_firmware, 10, VERSIONES_VERSION_FIRM_BLE_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[209], 2, RECORDING_REC_CAPACITY_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[211], 2, RECORDING_REC_LAST_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[213], 1, RECORDING_REC_LAPS_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[214], 2, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[216], 1, DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[217], 1, DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[218], 1, DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE);
		memcpy(device_SN, &buffer_rx_local[219], 10);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[229], 1, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE);

		cnt_timeout_inicio = TIMEOUT_INICIO;
		dispositivo_inicializado = 1;
	}
	else if(BLOQUE_STATUS == tipo_bloque)
	{		
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[1], 1, LED_LUMIN_COLOR_R_LED_COLOUR_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[2], 1, LED_LUMIN_COLOR_G_LED_COLOUR_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[3], 1, LED_LUMIN_COLOR_B_LED_COLOUR_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[4], 2, STATUS_HPT_STATUS_CHAR_HANDLE);
		if((memcmp(&buffer_rx_local[4], status_hpt_anterior, 2) != 0) && serverbleGetConnected())
		{
			memcpy(status_hpt_anterior, &buffer_rx_local[4], 2);
			serverbleNotCharacteristic(&buffer_rx_local[4], 2, STATUS_HPT_STATUS_CHAR_HANDLE); 
		}
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[6], 2, STATUS_ICP_STATUS_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[8], 2, STATUS_MCB_STATUS_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[10], 2, STATUS_RCD_STATUS_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[12], 2, STATUS_CONN_LOCK_STATUS_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[14], 1, MEASURES_MAX_CURRENT_CABLE_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[16], 2, MEASURES_INST_CURRENT_CHAR_HANDLE);
		inst_current_actual = buffer_rx_local[16] + (buffer_rx_local[17] * 0x100);

		if(((inst_current_actual / 10) != (inst_current_anterior / 10)) && serverbleGetConnected() && (--cnt_diferencia == 0))
		{
			cnt_diferencia = 2; // A.D.S. Cambiado de 30 a 5
			inst_current_anterior = inst_current_actual;
			serverbleNotCharacteristic(&buffer_rx_local[16], 2, STATUS_HPT_STATUS_CHAR_HANDLE); 
		}

		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[18], 2, MEASURES_INST_VOLTAGE_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[20], 2, MEASURES_ACTIVE_POWER_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[22], 2, MEASURES_REACTIVE_POWER_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[24], 4, MEASURES_ACTIVE_ENERGY_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[28], 4, MEASURES_REACTIVE_ENERGY_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[32], 4, MEASURES_APPARENT_POWER_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[36], 1, MEASURES_POWER_FACTOR_CHAR_HANDLE);

		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[38], 2, DOMESTIC_CONSUMPTION_DOMESTIC_CURRENT_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[40], 1, DOMESTIC_CONSUMPTION_REAL_CURRENT_LIMIT_CHAR_HANDLE);

		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[41], 1, ERROR_STATUS_ERROR_CODE_CHAR_HANDLE);


		luminosidad = buffer_rx_local[0];
		rojo = buffer_rx_local[1];
		verde = buffer_rx_local[2];
		azul = buffer_rx_local[3];
		if(luminosidad < 0x80)
			LED_Control(buffer_rx_local[0], buffer_rx_local[1], buffer_rx_local[2], buffer_rx_local[3]);
	}
	else if(BLOQUE_DATE_TIME == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 6, TIME_DATE_DATE_TIME_CHAR_HANDLE);
		aut_semilla = (((int)buffer_rx_local[4]) * 0x100) + (int)buffer_rx_local[5];
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[6], 6, TIME_DATE_CONNECTION_DATE_TIME_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[12], 6, TIME_DATE_DISCONNECTION_DATE_TIME_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[18], 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[24], 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);
	}
	else if(MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE);
	}
	else if(MEASURES_CURRENT_COMMAND_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, MEASURES_CURRENT_COMMAND_CHAR_HANDLE);
	}
	else if(TIME_DATE_DELTA_DELAY_FOR_CHARGING_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 2, TIME_DATE_DELTA_DELAY_FOR_CHARGING_CHAR_HANDLE);
	}
	else if(TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 6, TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE);
	}
	else if(TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 6, TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE);
	}
	else if(CHARGING_INSTANT_DELAYED_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, CHARGING_INSTANT_DELAYED_CHAR_HANDLE);
	}
	else if(CHARGING_START_STOP_START_MODE_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, CHARGING_START_STOP_START_MODE_CHAR_HANDLE);
	}
	else if(CHARGING_BLE_MANUAL_START_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, CHARGING_BLE_MANUAL_START_CHAR_HANDLE);
	}
	else if(CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE);
	}
	else if(SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 168, SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);
	}
	else if(VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_HANDLE == tipo_bloque)
	{
		memcpy(device_ID, buffer_rx_local, 11);
		esp_ble_gap_set_device_name((const char *)device_ID);
		changeAdvName(device_ID);
		printf("CHANge name set device name to %s\r\n",device_ID);

		Update_VELT1_CHARGER_characteristic(device_ID, 11, VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_HANDLE);
	}
	else if(VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE);
	}
	else if(VCD_NAME_USERS_UI_X_USER_ID_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, VCD_NAME_USERS_UI_X_USER_ID_CHAR_HANDLE);
	}
	else if(VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE);
	}
	else if(TEST_LAUNCH_RCD_PE_TEST_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, TEST_LAUNCH_RCD_PE_TEST_CHAR_HANDLE);
	}
	else if(TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE);
	}
	else if(TEST_LAUNCH_MCB_TEST_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, TEST_LAUNCH_MCB_TEST_CHAR_HANDLE);
	}
	else if(TEST_MCB_TEST_RESULT_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, TEST_MCB_TEST_RESULT_CHAR_HANDLE);
	}
	else if(MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_CHAR_HANDLE);
	}
	else if(LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE == tipo_bloque)
	{
		luminosidad = buffer_rx_local[0];
		LED_Control(luminosidad, rojo, verde, azul);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[0], 1, LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE);
	}
	else if(LED_LUMIN_COLOR_R_LED_COLOUR_CHAR_HANDLE == tipo_bloque)
	{
		rojo = buffer_rx_local[0];
		LED_Control(luminosidad, rojo, verde, azul);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[0], 1, LED_LUMIN_COLOR_R_LED_COLOUR_CHAR_HANDLE);
	}
	else if(LED_LUMIN_COLOR_G_LED_COLOUR_CHAR_HANDLE == tipo_bloque)
	{
		verde = buffer_rx_local[0];
		LED_Control(luminosidad, rojo, verde, azul);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[0], 1, LED_LUMIN_COLOR_G_LED_COLOUR_CHAR_HANDLE);
	}
	else if(LED_LUMIN_COLOR_B_LED_COLOUR_CHAR_HANDLE == tipo_bloque)
	{
		azul = buffer_rx_local[0];
		LED_Control(luminosidad, rojo, verde, azul);
		Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[0], 1, LED_LUMIN_COLOR_B_LED_COLOUR_CHAR_HANDLE);
	}
	else if(RESET_RESET_CHAR_HANDLE == tipo_bloque)
	{	
		//Modify_VELT1_CHARGER_characteristic(&buffer_rx_local[0], 1, RESET_RESET_CHAR_HANDLE);
	}
	else if(ENERGY_RECORD_RECORD_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 512, ENERGY_RECORD_RECORD_CHAR_HANDLE);
	}
	else if(RECORDING_REC_CAPACITY_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 2, RECORDING_REC_CAPACITY_CHAR_HANDLE);
	}
	else if(RECORDING_REC_LAST_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 2, RECORDING_REC_LAST_CHAR_HANDLE);
	}
	else if(RECORDING_REC_INDEX_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 2, RECORDING_REC_INDEX_CHAR_HANDLE);
	}
	else if(RECORDING_REC_LAPS_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 2, RECORDING_REC_LAPS_CHAR_HANDLE);
	}
	else if(CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 2, CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE);
	}
	else if(DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE);
	}
	else if(DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE);
	}
	else if(DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE);
	}
	else if(DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_HANDLE);
	}
	else if(ERROR_STATUS_ERROR_CODE_CHAR_HANDLE == tipo_bloque)
	{
		Modify_VELT1_CHARGER_characteristic(buffer_rx_local, 1, ERROR_STATUS_ERROR_CODE_CHAR_HANDLE);
	}

}
 
int controlSendToSerialLocal ( uint8_t * dades, int len )
{
	int ret=0;
	ret = serialLocal.write(dades, len);
	cnt_timeout_tx = TIMEOUT_TX_BLOQUE;
	return ret;
}

void deviceConnectInd ( void )
{
	int random = 0;
	const void* outputvec1;
	// This event is received when device is connected over GATT level 

	LED_Control(50, 10, 10, 0);

	srand(aut_semilla);
	random = rand();
	autent_matrix[7] = (uint8)(random >> 8);
	autent_matrix[6] = (uint8)random;
	random = rand();
	autent_matrix[5] = (uint8)(random >> 8);
	autent_matrix[4] = (uint8)random;
	random = rand();
	autent_matrix[3] = (uint8)(random >> 8);
	autent_matrix[2] = (uint8)random;
	random = rand();
	autent_matrix[1] = (uint8)(random >> 8);
	autent_matrix[0] = (uint8)random;

	outputvec1 = dev_auth_challenge(autent_matrix);

	memcpy(autent_token, outputvec1, 8);

	Modify_VELT1_CHARGER_characteristic(autent_matrix, 8, AUTENTICACION_MATRIX_CHAR_HANDLE);

}

void deviceDisconnectInd ( void )
{
	LED_Control(50, 10, 10, 10);
	//autentificacion_recibida = 0;
	memset(autent_token, 0x00, 8);
	//memset(autent_matrix, 0x00, 8);
	Modify_VELT1_CHARGER_characteristic(autent_matrix, 10, AUTENTICACION_MATRIX_CHAR_HANDLE);
}

uint8_t getModeTelecarga (void)
{
	return modo_telecarga_micro_principal;
}

uint8_t setModeTelecarga (uint8_t val )
{
	modo_telecarga_micro_principal = val;
	return modo_telecarga_micro_principal;
}

uint8_t setAutentificationToken ( uint8_t *data, int len )
{
	if(!autentificacion_recibida)
	{
		printf("%s %s \r\n",device_SN, dummy_array);
		if(!memcmp(autent_token, data, 8) || !memcmp(device_SN, dummy_array, 10))
		{
			printf("autentificacion_recibida\r\n");
			autentificacion_recibida = 1;
		}
	}

	return 1;
}

uint8_t sendBinaryBlock ( uint8_t *data, int len )
{
	if(modo_telecarga_micro_principal)
	{
		controlSendToSerialLocal(data,len);
	}

	return 1;
}

uint8_t hasAutentification ( void )
{
	return (autentificacion_recibida || modo_telecarga_micro_principal == 1 );
}

void Update_VELT1_CHARGER_characteristic(uint8_t* Data, uint16_t len, uint16_t attrHandle) 
{
	serverbleSetCharacteristic ( Data,len,attrHandle);
	return;
}


void Modify_VELT1_CHARGER_characteristic(uint8* Data, uint16 len, uint16 attrHandle)
{
	serverbleSetCharacteristic ( Data,len,attrHandle);
	return;
}

void Disable_VELT1_CHARGER_services(void)
{

}


void controlInit(void) 
{

	xTaskCreate(	
			controlTask,
			"TASK CONTROL",
			4096*10,
			NULL,
			1,
			NULL	
		   );

}


