#ifndef __HELPERS_H
#define __HELPERS_H

//Semaforo para el puerto serie
#define SERIALSEMAPHORE_TIMEOUT 1000 / portTICK_RATE_MS

//Convertir str a uint16
bool str_to_uint16(const char *str, uint16_t *res);
//Imprimir la tabla de cargadores
void print_table(carac_charger *table, char* table_name, uint8_t size);
//Limpiar el scroll
void cls();
//Printar un caracter hexadecimal
void printHex(const uint8_t num) ;
//Printar todo un buffer hexadecimal
void printHexBuffer(const char* num);

//Printar por consola el directorio del spiffs
void listFilesInDir(File dir, int numTabs = 1);

//Enviar datos al PSOC5
void SendToPSOC5(uint8 data, uint16 attrHandle);

//Enviar datos al PSOC5
void SendToPSOC5(char *data, uint16 len, uint16 attrHandle);

//Enviar datos al PSOC5
void SendToPSOC5(uint8 *data, uint16 len, uint16 attrHandle);

//Caso especial para enviar el status al psoc5
void SendStatusToPSOC5(uint8_t connected, uint8_t inicializado, uint8_t comm_type);

//Caso especial para enviar firmware al psoc5
uint8_t sendBinaryBlock ( uint8_t *data, int len );

uint8_t setMainFwUpdateActive (uint8_t val );
uint8_t getMainFwUpdateActive ();

//Extraer el numero de version de un string
uint16 ParseFirmwareVersion(String Texto);
String ParseFirmwareModel(String Texto);

/***********************************************************
		    Convertir fecha a timestamp epoch
***********************************************************/
int Convert_To_Epoch(uint8* data);

void convertSN();

#endif