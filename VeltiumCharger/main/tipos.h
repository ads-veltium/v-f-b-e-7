/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
 */


#ifndef TIPOS_H
#define TIPOS_H

#include "lwip/ip_addr.h"

#define EXT_FLASH_SIZE								8*1024*1024
#define EXT_FLASH_PAGE_SIZE							4096
#define EXT_FLASH_PARAM_PAGE						(EXT_FLASH_SIZE - EXT_FLASH_PAGE_SIZE) / EXT_FLASH_PAGE_SIZE
#define EXT_FLASH_HPT_ESTADO_PAGE					EXT_FLASH_PARAM_PAGE - 3
#define EXT_FLASH_PUNT_RECORD_PAGE					EXT_FLASH_PARAM_PAGE - 4
#define EXT_FLASH_TOT_ACTIVE_ENERGY_PAGE		    EXT_FLASH_PARAM_PAGE - 5
#define EXT_FLASH_TOT_REACTIVE_ENERGY_PAGE		    EXT_FLASH_PARAM_PAGE - 6
#define EXT_FLASH_CHARGING_START_TIME_PAGE		    EXT_FLASH_PARAM_PAGE - 7
#define EXT_FLASH_CHARGING_STOP_TIME_PAGE		    EXT_FLASH_PARAM_PAGE - 8
#define EXT_FLASH_RECORD_BUFFER_PAGE				0

// ESTADOS GENERALES
#define ESTADO_GENERAL_OK		0x00
#define ESTADO_GENERAL_WARNING	0x01
#define ESTADO_GENERAL_ERROR	0xFF

// CODIGOS DE ERROR
#define ESTADO_ERROR_NO_ERROR					0x00
#define ESTADO_ERROR_RCD_DISPARO_DIFERENCIAL	0x10
#define ESTADO_ERROR_MCB_SOBRECORRIENTE			0x20
#define ESTADO_ERROR_HPT_FALLO_FUENTE_ALIM		0x30
#define ESTADO_ERROR_HPT_CORTOCIRCUITO			0x31
#define ESTADO_ERROR_HPT_SIN_TIERRA				0x32
#define ESTADO_ERROR_HPT_SIN_DIODO				0x33
#define ESTADO_ERROR_SIN_CONEXION_TIERRA		0x40
#define ESTADO_ERROR_FASE_NEUTRO_INVERTIDOS		0x50
#define ESTADO_ERROR_ICP						0x60
#define ESTADO_ERROR_RCD_TEST_INICIAL			0x70

// ESTADO AUTOMATA HPT - Definición movida de HPT.h a tipos.h
#define HPT_ESTADO_0V		0
#define HPT_ESTADO_A1		1
#define HPT_ESTADO_A2		2
#define HPT_ESTADO_B1		3
#define HPT_ESTADO_B2		4
#define HPT_ESTADO_C1		5
#define HPT_ESTADO_C2		6
#define HPT_ESTADO_E		7
#define HPT_ESTADO_F		8

// TIPOS DE BLOQUE DE INTERCAMBIO CON BLE
#define BLOQUE_INICIALIZACION	0xFFFF
#define BLOQUE_STATUS			0xFFFE
#define BLOQUE_DATE_TIME		0xFFFD
#define BLOQUE_COMS				0xFFFC
#define BLOQUE_CALENDAR         0xFFFB
#define BLOQUE_APN              0xFFFA

#define LED_MAXIMO_PWM      1200     // Sobre 1200 de periodo
#define pdTICKS_TO_MS( xTicks )   ( ( uint32_t ) ( xTicks ) * 1000 / configTICK_RATE_HZ )

typedef	uint8_t			uint8;
typedef	uint16_t		uint16;
typedef	uint32_t		uint32;

typedef struct{
	uint8  max_current_cable;
	uint8  power_factor;
	uint16 consumo_domestico;
	uint16 instant_current;
	uint16 instant_voltage;
	uint16 active_power;
	uint16 reactive_power;
	uint32 active_energy;
	uint32 reactive_energy;
	uint32 apparent_power;
} caract_measures;

typedef struct{
	long long actual_time;
	long long connect_date_time;
	long long disconnect_date_time;
	long long charge_start_time;
	long long charge_stop_time;
} caract_date_time;

typedef struct{
	uint8 inst_delayed;
	uint8 start_stop_mode;
} caract_charge_config;

typedef struct{
	char charger_device_id[6];
	uint8 users_number;
	uint8 user_id;
	uint8 user_index;
} caract_name_users;


typedef struct{
	uint8 luminosity;
	uint8 R_led_level;
	uint8 G_led_level;
	uint8 B_led_level;

} caract_led_ctl;


/************* Estructuras para Firebase ************/
typedef struct{
	bool   start = false;
	bool   stop  = false;
	bool   reset = false;
	bool   fw_update = false;
	bool   conn_lock = false;

	uint8  desired_current;

	bool Newdata = false;
	long long last_ts_app_req= 0;

} carac_Comands;

typedef struct{
	bool ICP_status;
	bool DC_Leack_status;
	bool Con_Lock;
	bool Trifasico = false;
	uint8 error_code;
	uint8 current_limit;
	char HPT_status[2];
	
	caract_measures Measures;
	caract_measures MeasuresB;
	caract_measures MeasuresC;
	caract_date_time Time;

	long long last_ts_app_req= 0;
} carac_Status;

typedef struct{
	bool   Tipo_Sensor;
	bool   CDP_On;
	bool   Sensor_Conectado;
	bool   NewData;
	char   Fw_Update_mode[2];
	char   autentication_mode[2];
	uint8  Fase = 1;
	uint8  CDP;
	uint8  inst_current_limit;
	uint16 potencia_contratada1;
	uint16 potencia_contratada2;

	long long last_ts_app_req= 0;
} carac_Params;

typedef struct{
	bool ON;
	bool Internet = false;
	uint8_t AP[34]={'\0'};
	uint8_t Pass[64]={'\0'};
	ip4_addr_t IP;
	bool restart = false;
	bool SetFromInternal = false;
}carac_WIFI;

typedef struct{
	bool ON;
	bool Auto = 1;
	bool DHCP = 0;
	bool Internet = false;
	bool Wifi_Perm = false;
	bool restart = false;

	ip4_addr_t IP;
	ip4_addr_t Gateway;
	ip4_addr_t Mask;

	uint8_t   State;
	uint8_t   Puerto;
	bool 	conectado = false;
}carac_ETH;

typedef struct{
	bool ON;
	String Apn;
	String Pass;
	String User;
	uint8_t Pin [4];
	bool Internet = false;
	
	uint32 ts_app_req;
	uint32 ts_dev_ack;
}carac_MODEM;

typedef struct{
	bool StartConnection   = false;
	bool StartProvisioning = false;
	bool StartSmartconfig  = false;
	bool RemoveCredentials = false;
	bool RestartConection  = false;
	bool Provisioning 	   = false;

	carac_WIFI   Wifi;
	carac_ETH     ETH;
	carac_MODEM   GSM;

	long long last_ts_app_req= 0;
} carac_Coms;

typedef struct{
	
	bool      ContadorConectado = false;

	char      ContadorIp[15] ={"0"};
	uint16    DomesticPower;
	uint16    DomesticCurrentA;
	uint16    DomesticCurrentB;
	uint16    DomesticCurrentC;

	long long last_ts_app_req= 0;
} carac_Contador;

typedef struct{
	//Machine state orders
	bool WriteParams  = false;
	bool WriteStatus  = false;
	bool WriteComs    = false;
	bool WriteControl = false;
	bool WriteTime    = false;

	bool ReadControl  = false;
	bool ReadStatus   = false;
	bool ReadComs     = false;
	bool ReadParams   = false;

	//Auth configuration
	char  Device_Ser_num[30] = {'0'};
	char  Device_Id[12]      = {'0'};
	char  Device_Db_ID[30]   = {'0'};
	uint8 Email[30]          = {'0'};
	uint8 Password[30]		 = {'0'};
	uint8 User_Db_ID[30		]= {'0'};

	//Firebase conection Status
	bool FirebaseConnected = false;
	bool InternetConection = false;

	uint8 ConectionTimeout=0;
	bool ClientConnected = false;

}carac_Firebase_Configuration;

typedef struct{
	char     name[9];
	IPAddress     IP;
	char      HPT[3];
	uint8_t     Fase  = 1;
	uint8_t  Circuito = 1;
	uint8_t  order 	  = 0;
	uint16_t Consigna = 0;
	uint16_t Current  = 0;
	uint16_t CurrentB = 0;
	uint16_t CurrentC = 0;
	uint16_t   Delta  = 0;


	uint16_t Delta_timer = 0;
	TickType_t Period = 0;
	bool Conected = false;
	bool Baimena = false;

}carac_charger;

typedef struct{
	uint8_t GroupMaster = 0;
    uint8_t GroupActive = 0;
	uint8_t inst_max    = 0;
	uint8_t CDP         = 0;
	uint16_t ContractPower = 0;
	uint8_t UserID      = 0;
	uint16_t potencia_max = 0;	

}carac_group_params;

typedef struct{
    uint8_t numero = 0;
	int corriente_total = 0;
    int conex = 0;
    int consigna_total = 0;
    int corriente_sobrante = 0;
    float corriente_disponible = 0;

}carac_fase;


typedef struct{
    uint8_t numero = 0;
    uint8_t limite_corriente = 32;
	carac_fase Fases[3];
	int corriente_total = 0;
    int conex = 0;
    int consigna_total = 0;
	int corriente_sobrante = 0;
    float corriente_disponible = 0;
}carac_circuito;

typedef struct{
	bool AskPerm   = false;
	bool NewData   = false;
	bool SendNewParams = false;
	bool SendNewGroup  = false;
	bool SendNewCircuits  = false;
	bool Conected	   = false;
	bool StopOrder     = false;
	bool DeleteOrder   = false;
	bool StartClient   = false;
	bool ChargPerm     = false;
	bool Finding       = false;
	uint8_t Circuit_number = 0;
	uint8_t Charger_number = 0;

	IPAddress MasterIP;

	carac_group_params Params;

	char GroupId[10] = {'0'};
	long long last_ts_app_req= 0;

}carac_group;

typedef struct{
	//configuracion
	bool BetaPermission = false;
	uint16 PSOC5_Act_Ver;
	uint16 ESP_Act_Ver;

	bool PSOC5_UpdateAvailable = false;
	bool ESP_UpdateAvailable   = false;
	bool DescargandoArchivo    = false;
	bool InstalandoArchivo     = false;
	bool DobleUpdate  		   = false;
	
	String ESP_url;
	String PSOC_url;

} carac_Update_Status;
#define RCD_NO_ACTIVO
#undef RCD_ACTIVO
// Variable para definir si el Medidor doméstico mide la corriente total o sólo la vivienda  A.D.S.
#undef MEDIDA_CONSUMO_TOTAL
#define MEDIDA_CONSUMO_VIVIENDA

/***************************************
* Conditional Compilation Parameters
***************************************/

/* Maximum supported Custom Services */
#define CUSTOMS_SERVICE_COUNT                  (0x13u)
#define CUSTOMC_SERVICE_COUNT                  (0x00u)
#define CUSTOM_SERVICE_CHAR_COUNT              (0x0Bu)
#define CUSTOM_SERVICE_CHAR_DESCRIPTORS_COUNT  (0x01u)

/* Below are the indexes and handles of the defined Custom Services and their characteristics */
#define STATUS_SERVICE_INDEX   (0x00u) /* Index of Status service in the cyBle_customs array */
#define STATUS_HPT_STATUS_CHAR_INDEX   (0x00u) /* Index of HPT_Status characteristic */
#define STATUS_HPT_STATUS_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_INDEX   (0x00u) /* Index of Client Characteristic Configuration descriptor */
#define STATUS_ICP_STATUS_CHAR_INDEX   (0x01u) /* Index of ICP_Status characteristic */
#define STATUS_MCB_STATUS_CHAR_INDEX   (0x02u) /* Index of MCB_Status characteristic */
#define STATUS_RCD_STATUS_CHAR_INDEX   (0x03u) /* Index of RCD_Status characteristic */
#define STATUS_CONN_LOCK_STATUS_CHAR_INDEX   (0x04u) /* Index of Conn_Lock_Status characteristic */

#define MEASURES_SERVICE_INDEX   (0x01u) /* Index of Measures service in the cyBle_customs array */
#define MEASURES_MAX_CURRENT_CABLE_CHAR_INDEX   (0x00u) /* Index of Max_Current_Cable characteristic */
#define MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_INDEX   (0x01u) /* Index of Instalation_Current_Limit characteristic */
#define MEASURES_CURRENT_COMMAND_CHAR_INDEX   (0x02u) /* Index of Current_Command characteristic */
#define MEASURES_INST_CURRENT_CHAR_INDEX   (0x03u) /* Index of Inst_Current characteristic */
#define MEASURES_INST_CURRENT_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_INDEX   (0x00u) /* Index of Client Characteristic Configuration descriptor */
#define MEASURES_INST_VOLTAGE_CHAR_INDEX   (0x04u) /* Index of Inst_Voltage characteristic */
#define MEASURES_ACTIVE_POWER_CHAR_INDEX   (0x05u) /* Index of Active_Power characteristic */
#define MEASURES_REACTIVE_POWER_CHAR_INDEX   (0x06u) /* Index of Reactive_Power characteristic */
#define MEASURES_ACTIVE_ENERGY_CHAR_INDEX   (0x07u) /* Index of Active_Energy characteristic */
#define MEASURES_REACTIVE_ENERGY_CHAR_INDEX   (0x08u) /* Index of Reactive_Energy characteristic */
#define MEASURES_APPARENT_POWER_CHAR_INDEX   (0x09u) /* Index of Apparent_Power characteristic */
#define MEASURES_POWER_FACTOR_CHAR_INDEX   (0x0Au) /* Index of Power_Factor characteristic */

#define ENERGY_RECORD_SERVICE_INDEX   (0x02u) /* Index of Energy_Record service in the cyBle_customs array */
#define ENERGY_RECORD_RECORD_CHAR_INDEX   (0x00u) /* Index of Record characteristic */
#define ENERGY_RECORD_USER_CHAR_INDEX   (0x01u) /* Index of User characteristic */
#define ENERGY_RECORD_SESSION_NUMBER_CHAR_INDEX   (0x02u) /* Index of Session_Number characteristic */

#define TIME_DATE_SERVICE_INDEX   (0x03u) /* Index of Time_Date service in the cyBle_customs array */
#define TIME_DATE_DATE_TIME_CHAR_INDEX   (0x00u) /* Index of Date_Time characteristic */
#define TIME_DATE_CONNECTION_DATE_TIME_CHAR_INDEX   (0x01u) /* Index of Connection_Date_Time characteristic */
#define TIME_DATE_DISCONNECTION_DATE_TIME_CHAR_INDEX   (0x02u) /* Index of Disconnection_Date_Time characteristic */
#define TIME_DATE_DELTA_DELAY_FOR_CHARGING_CHAR_INDEX   (0x03u) /* Index of Delta_Delay_for_Charging characteristic */
#define TIME_DATE_CHARGING_START_TIME_CHAR_INDEX   (0x04u) /* Index of Charging_Start_Time characteristic */
#define TIME_DATE_CHARGING_STOP_TIME_CHAR_INDEX   (0x05u) /* Index of Charging_Stop_Time characteristic */

#define CHARGING_SERVICE_INDEX   (0x04u) /* Index of Charging service in the cyBle_customs array */
#define CHARGING_INSTANT_DELAYED_CHAR_INDEX   (0x00u) /* Index of Instant_Delayed characteristic */
#define CHARGING_START_STOP_START_MODE_CHAR_INDEX   (0x01u) /* Index of Start_Stop_Start_Mode characteristic */
#define CHARGING_BLE_MANUAL_START_CHAR_INDEX   (0x02u) /* Index of BLE_Manual_start characteristic */
#define CHARGING_BLE_MANUAL_STOP_CHAR_INDEX   (0x03u) /* Index of BLE_Manual_Stop characteristic */

#define SCHED_CHARGING_SERVICE_INDEX   (0x05u) /* Index of Sched_Charging service in the cyBle_customs array */
#define SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_INDEX   (0x00u) /* Index of Schedule_Matrix characteristic */

#define VCD_NAME_USERS_SERVICE_INDEX   (0x06u) /* Index of VCD_Name_Users service in the cyBle_customs array */
#define VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_INDEX   (0x00u) /* Index of Charger_Device_ID characteristic */
#define VCD_NAME_USERS_USERS_NUMBER_CHAR_INDEX   (0x01u) /* Index of Users_Number characteristic */
#define VCD_NAME_USERS_USER_TYPE_CHAR_INDEX   (0x02u) /* Index of User type characteristic */
#define VCD_NAME_USERS_USER_INDEX_CHAR_INDEX   (0x03u) /* Index of User_Index characteristic */

#define TEST_SERVICE_INDEX   (0x07u) /* Index of Test service in the cyBle_customs array */
#define TEST_LAUNCH_RCD_PE_TEST_CHAR_INDEX   (0x00u) /* Index of Launch_RCD_PE_Test characteristic */
#define TEST_RCD_PE_TEST_RESULT_CHAR_INDEX   (0x01u) /* Index of RCD_PE_Test_Result characteristic */
#define TEST_LAUNCH_MCB_TEST_CHAR_INDEX   (0x02u) /* Index of Launch_MCB_Test characteristic */
#define TEST_MCB_TEST_RESULT_CHAR_INDEX   (0x03u) /* Index of MCB_Test_Result characteristic */

#define MAN_LOCK_UNLOCK_SERVICE_INDEX   (0x08u) /* Index of Man_Lock_Unlock service in the cyBle_customs array */
#define MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_CHAR_INDEX   (0x00u) /* Index of Locking_Mechanism_ON_OFF characteristic */

#define BOOT_LOADER_SERVICE_INDEX   (0x09u) /* Index of Boot_Loader service in the cyBle_customs array */
#define BOOT_LOADER_PROTECTION_CODE_CHAR_INDEX   (0x00u) /* Index of Protection_Code characteristic */
#define BOOT_LOADER_LOAD_SW_APP_CHAR_INDEX   (0x01u) /* Index of Load_SW_App characteristic */
#define BOOT_LOADER_BINARY_BLOCK_CHAR_INDEX   (0x02u) /* Index of Binary_Block characteristic */
#define BOOT_LOADER_BINARY_BLOCK_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_INDEX   (0x00u) /* Index of Client Characteristic Configuration descriptor */

#define LED_LUMIN_COLOR_SERVICE_INDEX   (0x0Au) /* Index of Led_Lumin_Color service in the cyBle_customs array */
#define LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_INDEX   (0x00u) /* Index of Luminosity_Level characteristic */
#define LED_LUMIN_COLOR_R_LED_COLOUR_CHAR_INDEX   (0x01u) /* Index of R_Led_Colour characteristic */
#define LED_LUMIN_COLOR_G_LED_COLOUR_CHAR_INDEX   (0x02u) /* Index of G_Led_Colour characteristic */
#define LED_LUMIN_COLOR_B_LED_COLOUR_CHAR_INDEX   (0x03u) /* Index of B_Led_Colour characteristic */

#define RESET_SERVICE_INDEX   (0x0Bu) /* Index of Reset service in the cyBle_customs array */
#define RESET_RESET_CHAR_INDEX   (0x00u) /* Index of Reset characteristic */

#define CLAVE_SERVICE_INDEX   (0x0Cu) /* Index of Clave service in the cyBle_customs array */
#define CLAVE_WRITE_PASS_CHAR_INDEX   (0x00u) /* Index of Write_Pass characteristic */

#define RECORDING_SERVICE_INDEX   (0x0Du) /* Index of Recording service in the cyBle_customs array */
#define RECORDING_REC_CAPACITY_CHAR_INDEX   (0x00u) /* Index of Rec_Capacity characteristic */
#define RECORDING_REC_LAST_CHAR_INDEX   (0x01u) /* Index of Rec_Last characteristic */
#define RECORDING_REC_INDEX_CHAR_INDEX   (0x02u) /* Index of Rec_Index characteristic */
#define RECORDING_REC_LAPS_CHAR_INDEX   (0x03u) /* Index of Rec_Laps characteristic */

#define VERSIONES_SERVICE_INDEX   (0x0Eu) /* Index of Versiones service in the cyBle_customs array */
#define VERSIONES_VERSION_FIRMWARE_CHAR_INDEX   (0x00u) /* Index of Version_Firmware characteristic */
#define VERSIONES_VERSION_FIRM_BLE_CHAR_INDEX   (0x01u) /* Index of Version_Firm_BLE characteristic */

#define CONFIGURACION_SERVICE_INDEX   (0x0Fu) /* Index of Configuracion service in the cyBle_customs array */
#define CONFIGURACION_AUTENTICATION_MODES_CHAR_INDEX   (0x00u) /* Index of Autentication_Modes characteristic */

#define AUTENTICACION_SERVICE_INDEX   (0x10u) /* Index of Autenticacion service in the cyBle_customs array */
#define AUTENTICACION_SERIAL_NUMBER_CHAR_INDEX   (0x00u) /* Index of Serial_Number characteristic */
#define AUTENTICACION_MATRIX_CHAR_INDEX   (0x01u) /* Index of Matrix characteristic */
#define AUTENTICACION_TOKEN_CHAR_INDEX   (0x02u) /* Index of Token characteristic */

#define DOMESTIC_CONSUMPTION_SERVICE_INDEX   (0x11u) /* Index of Domestic_Consumption service in the cyBle_customs array */
#define DOMESTIC_CONSUMPTION_REAL_CURRENT_LIMIT_CHAR_INDEX   (0x00u) /* Index of Real_Current_Limit characteristic */
#define DOMESTIC_CONSUMPTION_DOMESTIC_CURRENT_CHAR_INDEX   (0x01u) /* Index of Domestic_Current characteristic */
#define DOMESTIC_CONSUMPTION_KS_CHAR_INDEX   (0x02u) /* Index of KS characteristic */
#define DOMESTIC_CONSUMPTION_FCT_CHAR_INDEX   (0x03u) /* Index of FCT characteristic */
#define DOMESTIC_CONSUMPTION_FS_CHAR_INDEX   (0x04u) /* Index of FS characteristic */
#define DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_CHAR_INDEX   (0x05u) /* Index of Potencia_Contratada characteristic */

#define ERROR_STATUS_SERVICE_INDEX   (0x12u) /* Index of Error_Status service in the cyBle_customs array */
#define ERROR_STATUS_ERROR_CODE_CHAR_INDEX   (0x00u) /* Index of Error_Code characteristic */


#define STATUS_SERVICE_HANDLE   (0x000Cu) /* Handle of Status service */
#define STATUS_HPT_STATUS_DECL_HANDLE   (0x000Du) /* Handle of HPT_Status characteristic declaration */
#define STATUS_HPT_STATUS_CHAR_HANDLE   (0x000Eu) /* Handle of HPT_Status characteristic */
#define STATUS_HPT_STATUS_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_HANDLE   (0x000Fu) /* Handle of Client Characteristic Configuration descriptor */
#define STATUS_ICP_STATUS_DECL_HANDLE   (0x0010u) /* Handle of ICP_Status characteristic declaration */
#define STATUS_ICP_STATUS_CHAR_HANDLE   (0x0011u) /* Handle of ICP_Status characteristic */
#define STATUS_MCB_STATUS_DECL_HANDLE   (0x0012u) /* Handle of MCB_Status characteristic declaration */
#define STATUS_MCB_STATUS_CHAR_HANDLE   (0x0013u) /* Handle of MCB_Status characteristic */
#define STATUS_RCD_STATUS_DECL_HANDLE   (0x0014u) /* Handle of RCD_Status characteristic declaration */
#define STATUS_RCD_STATUS_CHAR_HANDLE   (0x0015u) /* Handle of RCD_Status characteristic */
#define STATUS_CONN_LOCK_STATUS_DECL_HANDLE   (0x0016u) /* Handle of Conn_Lock_Status characteristic declaration */
#define STATUS_CONN_LOCK_STATUS_CHAR_HANDLE   (0x0017u) /* Handle of Conn_Lock_Status characteristic */

#define MEASURES_SERVICE_HANDLE   (0x0018u) /* Handle of Measures service */
#define MEASURES_MAX_CURRENT_CABLE_DECL_HANDLE   (0x0019u) /* Handle of Max_Current_Cable characteristic declaration */
#define MEASURES_MAX_CURRENT_CABLE_CHAR_HANDLE   (0x001Au) /* Handle of Max_Current_Cable characteristic */
#define MEASURES_INSTALATION_CURRENT_LIMIT_DECL_HANDLE   (0x001Bu) /* Handle of Instalation_Current_Limit characteristic declaration */
#define MEASURES_INSTALATION_CURRENT_LIMIT_CHAR_HANDLE   (0x001Cu) /* Handle of Instalation_Current_Limit characteristic */
#define MEASURES_CURRENT_COMMAND_DECL_HANDLE   (0x001Du) /* Handle of Current_Command characteristic declaration */
#define MEASURES_CURRENT_COMMAND_CHAR_HANDLE   (0x001Eu) /* Handle of Current_Command characteristic */
#define MEASURES_INST_CURRENT_DECL_HANDLE   (0x001Fu) /* Handle of Inst_Current characteristic declaration */
#define MEASURES_INST_CURRENT_CHAR_HANDLE   (0x0020u) /* Handle of Inst_Current characteristic */
#define MEASURES_INST_CURRENT_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_HANDLE   (0x0021u) /* Handle of Client Characteristic Configuration descriptor */
#define MEASURES_INST_VOLTAGE_DECL_HANDLE   (0x0022u) /* Handle of Inst_Voltage characteristic declaration */
#define MEASURES_INST_VOLTAGE_CHAR_HANDLE   (0x0023u) /* Handle of Inst_Voltage characteristic */
#define MEASURES_ACTIVE_POWER_DECL_HANDLE   (0x0024u) /* Handle of Active_Power characteristic declaration */
#define MEASURES_ACTIVE_POWER_CHAR_HANDLE   (0x0025u) /* Handle of Active_Power characteristic */
#define MEASURES_REACTIVE_POWER_DECL_HANDLE   (0x0026u) /* Handle of Reactive_Power characteristic declaration */
#define MEASURES_REACTIVE_POWER_CHAR_HANDLE   (0x0027u) /* Handle of Reactive_Power characteristic */
#define MEASURES_ACTIVE_ENERGY_DECL_HANDLE   (0x0028u) /* Handle of Active_Energy characteristic declaration */
#define MEASURES_ACTIVE_ENERGY_CHAR_HANDLE   (0x0029u) /* Handle of Active_Energy characteristic */
#define MEASURES_REACTIVE_ENERGY_DECL_HANDLE   (0x002Au) /* Handle of Reactive_Energy characteristic declaration */
#define MEASURES_REACTIVE_ENERGY_CHAR_HANDLE   (0x002Bu) /* Handle of Reactive_Energy characteristic */
#define MEASURES_APPARENT_POWER_DECL_HANDLE   (0x002Cu) /* Handle of Apparent_Power characteristic declaration */
#define MEASURES_APPARENT_POWER_CHAR_HANDLE   (0x002Du) /* Handle of Apparent_Power characteristic */
#define MEASURES_POWER_FACTOR_DECL_HANDLE   (0x002Eu) /* Handle of Power_Factor characteristic declaration */
#define MEASURES_POWER_FACTOR_CHAR_HANDLE   (0x002Fu) /* Handle of Power_Factor characteristic */

#define ENERGY_RECORD_SERVICE_HANDLE   (0x0030u) /* Handle of Energy_Record service */
#define ENERGY_RECORD_RECORD_DECL_HANDLE   (0x0031u) /* Handle of Record characteristic declaration */
#define ENERGY_RECORD_RECORD_CHAR_HANDLE   (0x0032u) /* Handle of Record characteristic */
#define ENERGY_RECORD_USER_DECL_HANDLE   (0x0033u) /* Handle of User characteristic declaration */
#define ENERGY_RECORD_USER_CHAR_HANDLE   (0x0034u) /* Handle of User characteristic */
#define ENERGY_RECORD_SESSION_NUMBER_DECL_HANDLE   (0x0035u) /* Handle of Session_Number characteristic declaration */
#define ENERGY_RECORD_SESSION_NUMBER_CHAR_HANDLE   (0x0036u) /* Handle of Session_Number characteristic */

#define TIME_DATE_SERVICE_HANDLE   (0x0037u) /* Handle of Time_Date service */
#define TIME_DATE_DATE_TIME_DECL_HANDLE   (0x0038u) /* Handle of Date_Time characteristic declaration */
#define TIME_DATE_DATE_TIME_CHAR_HANDLE   (0x0039u) /* Handle of Date_Time characteristic */
#define TIME_DATE_CONNECTION_DATE_TIME_DECL_HANDLE   (0x003Au) /* Handle of Connection_Date_Time characteristic declaration */
#define TIME_DATE_CONNECTION_DATE_TIME_CHAR_HANDLE   (0x003Bu) /* Handle of Connection_Date_Time characteristic */
#define TIME_DATE_DISCONNECTION_DATE_TIME_DECL_HANDLE   (0x003Cu) /* Handle of Disconnection_Date_Time characteristic declaration */
#define TIME_DATE_DISCONNECTION_DATE_TIME_CHAR_HANDLE   (0x003Du) /* Handle of Disconnection_Date_Time characteristic */
#define TIME_DATE_DELTA_DELAY_FOR_CHARGING_DECL_HANDLE   (0x003Eu) /* Handle of Delta_Delay_for_Charging characteristic declaration */
#define TIME_DATE_DELTA_DELAY_FOR_CHARGING_CHAR_HANDLE   (0x003Fu) /* Handle of Delta_Delay_for_Charging characteristic */
#define TIME_DATE_CHARGING_START_TIME_DECL_HANDLE   (0x0040u) /* Handle of Charging_Start_Time characteristic declaration */
#define TIME_DATE_CHARGING_START_TIME_CHAR_HANDLE   (0x0041u) /* Handle of Charging_Start_Time characteristic */
#define TIME_DATE_CHARGING_STOP_TIME_DECL_HANDLE   (0x0042u) /* Handle of Charging_Stop_Time characteristic declaration */
#define TIME_DATE_CHARGING_STOP_TIME_CHAR_HANDLE   (0x0043u) /* Handle of Charging_Stop_Time characteristic */

#define CHARGING_SERVICE_HANDLE   (0x0044u) /* Handle of Charging service */
#define CHARGING_INSTANT_DELAYED_DECL_HANDLE   (0x0045u) /* Handle of Instant_Delayed characteristic declaration */
#define CHARGING_INSTANT_DELAYED_CHAR_HANDLE   (0x0046u) /* Handle of Instant_Delayed characteristic */
#define CHARGING_START_STOP_START_MODE_DECL_HANDLE   (0x0047u) /* Handle of Start_Stop_Start_Mode characteristic declaration */
#define CHARGING_START_STOP_START_MODE_CHAR_HANDLE   (0x0048u) /* Handle of Start_Stop_Start_Mode characteristic */
#define CHARGING_BLE_MANUAL_START_DECL_HANDLE   (0x0049u) /* Handle of BLE_Manual_start characteristic declaration */
#define CHARGING_BLE_MANUAL_START_CHAR_HANDLE   (0x004Au) /* Handle of BLE_Manual_start characteristic */
#define CHARGING_BLE_MANUAL_STOP_DECL_HANDLE   (0x004Bu) /* Handle of BLE_Manual_Stop characteristic declaration */
#define CHARGING_BLE_MANUAL_STOP_CHAR_HANDLE   (0x004Cu) /* Handle of BLE_Manual_Stop characteristic */

#define SCHED_CHARGING_SERVICE_HANDLE   (0x004Du) /* Handle of Sched_Charging service */
#define SCHED_CHARGING_SCHEDULE_MATRIX_DECL_HANDLE   (0x004Eu) /* Handle of Schedule_Matrix characteristic declaration */
#define SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE   (0x004Fu) /* Handle of Schedule_Matrix characteristic */

#define VCD_NAME_USERS_SERVICE_HANDLE   (0x0050u) /* Handle of VCD_Name_Users service */
#define VCD_NAME_USERS_CHARGER_DEVICE_ID_DECL_HANDLE   (0x0051u) /* Handle of Charger_Device_ID characteristic declaration */
#define VCD_NAME_USERS_CHARGER_DEVICE_ID_CHAR_HANDLE   (0x0052u) /* Handle of Charger_Device_ID characteristic */
#define VCD_NAME_USERS_USERS_NUMBER_DECL_HANDLE   (0x0053u) /* Handle of Users_Number characteristic declaration */
#define VCD_NAME_USERS_USERS_NUMBER_CHAR_HANDLE   (0x0054u) /* Handle of Users_Number characteristic */
#define VCD_NAME_USERS_USER_TYPE_DECL_HANDLE   (0x0055u) /* Handle of user_type characteristic declaration */
#define VCD_NAME_USERS_USER_TYPE_CHAR_HANDLE   (0x0056u) /* Handle of user_type characteristic */
#define VCD_NAME_USERS_USER_INDEX_DECL_HANDLE   (0x0057u) /* Handle of User_Index characteristic declaration */
#define VCD_NAME_USERS_USER_INDEX_CHAR_HANDLE   (0x0058u) /* Handle of User_Index characteristic */

#define TEST_SERVICE_HANDLE   (0x0059u) /* Handle of Test service */
#define TEST_LAUNCH_RCD_PE_TEST_DECL_HANDLE   (0x005Au) /* Handle of Launch_RCD_PE_Test characteristic declaration */
#define TEST_LAUNCH_RCD_PE_TEST_CHAR_HANDLE   (0x005Bu) /* Handle of Launch_RCD_PE_Test characteristic */
#define TEST_RCD_PE_TEST_RESULT_DECL_HANDLE   (0x005Cu) /* Handle of RCD_PE_Test_Result characteristic declaration */
#define TEST_RCD_PE_TEST_RESULT_CHAR_HANDLE   (0x005Du) /* Handle of RCD_PE_Test_Result characteristic */
#define TEST_LAUNCH_MCB_TEST_DECL_HANDLE   (0x005Eu) /* Handle of Launch_MCB_Test characteristic declaration */
#define TEST_LAUNCH_MCB_TEST_CHAR_HANDLE   (0x005Fu) /* Handle of Launch_MCB_Test characteristic */
#define TEST_MCB_TEST_RESULT_DECL_HANDLE   (0x0060u) /* Handle of MCB_Test_Result characteristic declaration */
#define TEST_MCB_TEST_RESULT_CHAR_HANDLE   (0x0061u) /* Handle of MCB_Test_Result characteristic */

#define MAN_LOCK_UNLOCK_SERVICE_HANDLE   (0x0062u) /* Handle of Man_Lock_Unlock service */
#define MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_DECL_HANDLE   (0x0063u) /* Handle of Locking_Mechanism_ON_OFF characteristic declaration */
#define MAN_LOCK_UNLOCK_LOCKING_MECHANISM_ON_OFF_CHAR_HANDLE   (0x0064u) /* Handle of Locking_Mechanism_ON_OFF characteristic */

#define BOOT_LOADER_SERVICE_HANDLE   (0x0065u) /* Handle of Boot_Loader service */
#define BOOT_LOADER_PROTECTION_CODE_DECL_HANDLE   (0x0066u) /* Handle of Protection_Code characteristic declaration */
#define BOOT_LOADER_PROTECTION_CODE_CHAR_HANDLE   (0x0067u) /* Handle of Protection_Code characteristic */
#define BOOT_LOADER_LOAD_SW_APP_DECL_HANDLE   (0x0068u) /* Handle of Load_SW_App characteristic declaration */
#define BOOT_LOADER_LOAD_SW_APP_CHAR_HANDLE   (0x0069u) /* Handle of Load_SW_App characteristic */
#define BOOT_LOADER_BINARY_BLOCK_DECL_HANDLE   (0x006Au) /* Handle of Binary_Block characteristic declaration */
#define BOOT_LOADER_BINARY_BLOCK_CHAR_HANDLE   (0x006Bu) /* Handle of Binary_Block characteristic */
#define BOOT_LOADER_BINARY_BLOCK_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_HANDLE   (0x006Cu) /* Handle of Client Characteristic Configuration descriptor */

#define LED_LUMIN_COLOR_SERVICE_HANDLE   (0x006Du) /* Handle of Led_Lumin_Color service */
#define LED_LUMIN_COLOR_LUMINOSITY_LEVEL_DECL_HANDLE   (0x006Eu) /* Handle of Luminosity_Level characteristic declaration */
#define LED_LUMIN_COLOR_LUMINOSITY_LEVEL_CHAR_HANDLE   (0x006Fu) /* Handle of Luminosity_Level characteristic */
#define LED_LUMIN_COLOR_R_LED_COLOUR_DECL_HANDLE   (0x0070u) /* Handle of R_Led_Colour characteristic declaration */
#define LED_LUMIN_COLOR_R_LED_COLOUR_CHAR_HANDLE   (0x0071u) /* Handle of R_Led_Colour characteristic */
#define LED_LUMIN_COLOR_G_LED_COLOUR_DECL_HANDLE   (0x0072u) /* Handle of G_Led_Colour characteristic declaration */
#define LED_LUMIN_COLOR_G_LED_COLOUR_CHAR_HANDLE   (0x0073u) /* Handle of G_Led_Colour characteristic */
#define LED_LUMIN_COLOR_B_LED_COLOUR_DECL_HANDLE   (0x0074u) /* Handle of B_Led_Colour characteristic declaration */
#define LED_LUMIN_COLOR_B_LED_COLOUR_CHAR_HANDLE   (0x0075u) /* Handle of B_Led_Colour characteristic */

#define RESET_SERVICE_HANDLE   (0x0076u) /* Handle of Reset service */
#define RESET_RESET_DECL_HANDLE   (0x0077u) /* Handle of Reset characteristic declaration */
#define RESET_RESET_CHAR_HANDLE   (0x0078u) /* Handle of Reset characteristic */

#define CLAVE_SERVICE_HANDLE   (0x0079u) /* Handle of Clave service */
#define CLAVE_WRITE_PASS_DECL_HANDLE   (0x007Au) /* Handle of Write_Pass characteristic declaration */
#define CLAVE_WRITE_PASS_CHAR_HANDLE   (0x007Bu) /* Handle of Write_Pass characteristic */

#define RECORDING_SERVICE_HANDLE   (0x007Cu) /* Handle of Recording service */
#define RECORDING_REC_CAPACITY_DECL_HANDLE   (0x007Du) /* Handle of Rec_Capacity characteristic declaration */
#define RECORDING_REC_CAPACITY_CHAR_HANDLE   (0x007Eu) /* Handle of Rec_Capacity characteristic */
#define RECORDING_REC_LAST_DECL_HANDLE   (0x007Fu) /* Handle of Rec_Last characteristic declaration */
#define RECORDING_REC_LAST_CHAR_HANDLE   (0x0080u) /* Handle of Rec_Last characteristic */
#define RECORDING_REC_INDEX_DECL_HANDLE   (0x0081u) /* Handle of Rec_Index characteristic declaration */
#define RECORDING_REC_INDEX_CHAR_HANDLE   (0x0082u) /* Handle of Rec_Index characteristic */
#define RECORDING_REC_LAPS_DECL_HANDLE   (0x0083u) /* Handle of Rec_Laps characteristic declaration */
#define RECORDING_REC_LAPS_CHAR_HANDLE   (0x0084u) /* Handle of Rec_Laps characteristic */

#define VERSIONES_SERVICE_HANDLE   (0x0085u) /* Handle of Versiones service */
#define VERSIONES_VERSION_FIRMWARE_DECL_HANDLE   (0x0086u) /* Handle of Version_Firmware characteristic declaration */
#define VERSIONES_VERSION_FIRMWARE_CHAR_HANDLE   (0x0087u) /* Handle of Version_Firmware characteristic */
#define VERSIONES_VERSION_FIRM_BLE_DECL_HANDLE   (0x0088u) /* Handle of Version_Firm_BLE characteristic declaration */
#define VERSIONES_VERSION_FIRM_BLE_CHAR_HANDLE   (0x0089u) /* Handle of Version_Firm_BLE characteristic */

#define CONFIGURACION_SERVICE_HANDLE   (0x008Au) /* Handle of Configuracion service */
#define CONFIGURACION_AUTENTICATION_MODES_DECL_HANDLE   (0x008Bu) /* Handle of Autentication_Modes characteristic declaration */
#define CONFIGURACION_AUTENTICATION_MODES_CHAR_HANDLE   (0x008Cu) /* Handle of Autentication_Modes characteristic */

#define AUTENTICACION_SERVICE_HANDLE   (0x008Du) /* Handle of Autenticacion service */
#define AUTENTICACION_SERIAL_NUMBER_DECL_HANDLE   (0x008Eu) /* Handle of Serial_Number characteristic declaration */
#define AUTENTICACION_SERIAL_NUMBER_CHAR_HANDLE   (0x008Fu) /* Handle of Serial_Number characteristic */
#define AUTENTICACION_MATRIX_DECL_HANDLE   (0x0090u) /* Handle of Matrix characteristic declaration */
#define AUTENTICACION_MATRIX_CHAR_HANDLE   (0x0091u) /* Handle of Matrix characteristic */
#define AUTENTICACION_TOKEN_DECL_HANDLE   (0x0092u) /* Handle of Token characteristic declaration */
#define AUTENTICACION_TOKEN_CHAR_HANDLE   (0x0093u) /* Handle of Token characteristic */

#define DOMESTIC_CONSUMPTION_SERVICE_HANDLE                    (0x0094u) /* Handle of Domestic_Consumption service */
#define DOMESTIC_CONSUMPTION_REAL_CURRENT_LIMIT_DECL_HANDLE    (0x0095u) /* Handle of Real_Current_Limit characteristic declaration */
#define DOMESTIC_CONSUMPTION_REAL_CURRENT_LIMIT_CHAR_HANDLE    (0x0096u) /* Handle of Real_Current_Limit characteristic */
#define DOMESTIC_CONSUMPTION_DOMESTIC_CURRENT_DECL_HANDLE      (0x0097u) /* Handle of Domestic_Current characteristic declaration */
#define DOMESTIC_CONSUMPTION_DOMESTIC_CURRENT_CHAR_HANDLE      (0x0098u) /* Handle of Domestic_Current characteristic */
#define DOMESTIC_CONSUMPTION_KS_DECL_HANDLE                    (0x0099u) /* Handle of KS characteristic declaration */
#define DOMESTIC_CONSUMPTION_KS_CHAR_HANDLE                    (0x009Au) /* Handle of KS characteristic */
#define DOMESTIC_CONSUMPTION_FCT_DECL_HANDLE                   (0x009Bu) /* Handle of FCT characteristic declaration */
#define DOMESTIC_CONSUMPTION_FCT_CHAR_HANDLE                   (0x009Cu) /* Handle of FCT characteristic */
#define DOMESTIC_CONSUMPTION_FS_DECL_HANDLE   				   (0x009Du) /* Handle of FS characteristic declaration */
#define DOMESTIC_CONSUMPTION_FS_CHAR_HANDLE   				   (0x009Eu) /* Handle of FS characteristic */
#define DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P1_DECL_HANDLE   (0x009Fu) /* Handle of Potencia_Contratada characteristic declaration */
#define DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P1_CHAR_HANDLE   (0x00A0u) /* Handle of Potencia_Contratada characteristic */
#define DOMESTIC_CONSUMPTION_DPC_MODE_DECL_HANDLE              (0x00A1u) /* Handle of Dpc_Mode characteristic declaration */
#define DOMESTIC_CONSUMPTION_DPC_MODE_CHAR_HANDLE              (0x00A2u) /* Handle of Dpc_Mode characteristic */

#define ERROR_STATUS_SERVICE_HANDLE           (0x00A3u) /* Handle of Error_Status service */
#define ERROR_STATUS_ERROR_CODE_DECL_HANDLE   (0x00A4u) /* Handle of Error_Code characteristic declaration */
#define ERROR_STATUS_ERROR_CODE_CHAR_HANDLE   (0x00A5u) /* Handle of Error_Code characteristic */

#define TIME_DATE_COUNTRY_CHAR_HANDLE		(0x00A9u)
#define TIME_DATE_CALENDAR_CHAR_HANDLE		(0x00ABu)

// pseudo characteristic handles for Bird Prolog and Epilog Firmware Update Messages
#define FWUPDATE_BIRD_PROLOG_PSEUDO_CHAR_HANDLE (0x00ADu)
#define FWUPDATE_BIRD_DATA_PSEUDO_CHAR_HANDLE   (0x00AEu)
#define FWUPDATE_BIRD_EPILOG_PSEUDO_CHAR_HANDLE (0x00AFu)


//Custom handles for comunications
#define COMS_CONFIGURATION_WIFI_ON	       (0x00B1u)
#define COMS_CONFIGURATION_WIFI_SSID_1	   (0x00B3u)
#define COMS_CONFIGURATION_WIFI_SSID_2	   (0x00B5u)
#define COMS_CONFIGURATION_WIFI_START_PROV (0x00B7u)
#define COMS_CONFIGURATION_ETH_ON	       (0x00B9u)
#define COMS_CONFIGURATION_LAN_IP	       (0x00BBu)
#define COMS_CONFIGURATION_ETH_AUTO        (0x00BCu)

//Custom handles for groups
#define GROUPS_OPERATIONS				   (0x00BDu)
#define GROUPS_PARAMS		 		 	   (0x00BFu)
#define GROUPS_CIRCUITS					   (0x00C1u)

#define DOMESTIC_CONSUMPTION_POTENCIA_CONTRATADA_P2_CHAR_HANDLE (0x00C3u)

#define MEASURES_INST_CURRENTB_CHAR_HANDLE (0x00C5u)
#define MEASURES_INST_CURRENTC_CHAR_HANDLE (0x00C7u)

//Caracteristicas para el modem
#define APN	   							   (0x00C9u)
#define APN_USER  						   (0x00CBu)
#define APN_PASSWORD	   				   (0x00CDu)
#define APN_PIN							   (0x00CFu)
#define APN_ON 							   (0x00D1u)

#define MEASURES_EXTERNAL_COUNTER		   (0x00D3u)
#define COMS_FW_UPDATEMODE_CHAR_HANDLE     (0x00D5u)
#define GROUPS_DEVICES_PART_1 	  		   (0x00D7u)
#define GROUPS_DEVICES_PART_2 	  		   (0x00D9u)

#define SEND_GROUP_DATA		 		 	   (0x00DBu)
#define BLOQUEO_CARGA                      (0x00DDu)

#define CHARGING_GROUP_BLE_NET_DEVICES	   (0x00DFu)
#define CHARGING_GROUP_BLE_CHARGING_GROUP  (0x00E1u)






#endif

/* [] END OF FILE */

