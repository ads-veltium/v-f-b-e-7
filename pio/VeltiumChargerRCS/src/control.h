#ifndef __CONTROL_MAIN
#define __CONTROL_MAIN

// ESTADO GENERAL
#define	ESTADO_ARRANQUE			0
#define	ESTADO_INICIALIZACION	1
#define	ESTADO_NORMAL			2
#define ESTADO_CONECTADO		3

// ESTADO AUTOMATA UART
#define ESPERANDO_HEADER	0
#define ESPERANDO_TIPO		1
#define ESPERANDO_LONGITUD	2
#define RECIBIENDO_BLOQUE	3

// TIMEOUTS
#define TIMEOUT_TX_BLOQUE	10
#define TIMEOUT_RX_BLOQUE	10
#define TIMEOUT_INICIO		10
#define TIME_PARPADEO		500 		// 500 mS (2seg)

#define HEADER_RX			':'
#define HEADER_TX			':'


void Update_VELT1_CHARGER_characteristic(uint8_t* Data, uint16_t len, uint16_t attrHandle);
int controlSendToSerialLocal( uint8_t * dades, int len );
void LED_Control(uint8_t luminosity, uint8_t r_level, uint8_t g_level, uint8_t b_level);
int controlMain(void);
void deviceConnectInd(void);
void deviceDisconnectInd(void);
uint8_t getModeTelecarga (void);
uint8_t setModeTelecarga (uint8_t val);
uint8_t sendBinaryBlock ( uint8_t *data, int len );
uint8_t setAutentificationToken ( uint8_t *data, int len );
uint8_t hasAutentification ( void );
void MAIN_RESET_Write( uint8_t val );
void controlInit(void);

#endif // __CONTROL_MAIN
