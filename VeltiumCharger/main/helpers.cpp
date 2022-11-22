#include "control.h"
#include "helpers.h"

extern HardwareSerialMOD serialLocal;
extern uint8_t mainFwUpdateActive;
extern uint8_t updateTaskrunning;

//*Declaracion de funciones privadas*/
int controlSendToSerialLocal ( uint8_t * data, int len );

//********************Funciones publicas accessibles desde fuera***********************************/
bool str_to_uint16(const char *str, uint16_t *res){
  char *end;
  intmax_t val = strtoimax(str, &end, 10);
  if ( val < 0 || val > UINT16_MAX || end == str || *end != '\0')
    return false;
  *res = (uint16_t) val;
  return true;
}

#ifdef DEBUG
void cls(){
#ifdef DEBUG
    Serial.write(27);   
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H"); 
#endif
}

void print_table(carac_charger *table, char* table_name = "Grupo de cargadores", uint8_t size = 0){
    printf("=============== %s ===================\n", table_name);
    printf("      ID     Fase   HPT   I      CONS   D    DT     CONEX C\n");
    for(int i=0; i< size;i++){     //comprobar si el cargador ya está almacenado
        printf("   %s    %i    %s  %i   %i   %i   %i    %i  %i\n", table[i].name,table[i].Fase,table[i].HPT,table[i].Current, table[i].Consigna, table[i].Delta,  table[i].Delta_timer, table[i].Conected, table[i].Circuito);
    }
    printf("Memoria interna disponible: %i\n", esp_get_free_internal_heap_size());
    printf("Memoria total disponible:   %i\n", esp_get_free_heap_size());
    printf("=======================================================\n");
}

void printHex(const uint8_t num) {
  char hexCar[3];
  sprintf(hexCar, "%02X", num);
  Serial.print(hexCar);
  Serial.print(" ");
}

void printHexBuffer(const char* num) {
  for(int i=0; i<strlen(num); i++){
    printHex((uint8_t)num[i]);
  }
  Serial.println();
}

void listFilesInDir(File dir, int numTabs) {
  while (true) {
 
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files in the folder
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      listFilesInDir(entry, numTabs + 1);
    } else {
      // display zise for file, nothing for directory
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}
#endif
//----------------------------------------------------------------------------
bool WaitForValue(uint8* variable, uint8_t objetivo, uint16_t timeout){

	while(*variable != objetivo || --timeout > 0){
		delay(10);
	}

	return *variable == objetivo;

}

bool WaitForValue(uint16_t* variable, uint16_t objetivo, uint16_t timeout){

	while(*variable != objetivo || --timeout > 0){
		delay(10);
	}

	return *variable == objetivo;

}

bool WaitForValue(float* variable, float objetivo, uint16_t timeout){

	while(*variable != objetivo || --timeout > 0){
		delay(10);
	}

	return *variable == objetivo;

}

bool WaitForValue(String* variable, String objetivo, uint16_t timeout){

	while(*variable != objetivo || --timeout > 0){
		delay(10);
	}

	return *variable == objetivo;

}

//----------------------------------------------------------------------------
void SendToPSOC5(uint8 data, uint16 attrHandle){
  
  uint8 buffer_tx_local[5];

  //cnt_timeout_tx = TIMEOUT_TX_BLOQUE2;
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(attrHandle >> 8);
  buffer_tx_local[2] = (uint8)(attrHandle);
  buffer_tx_local[3] = 1; //size
  buffer_tx_local[4] = data;
  //controlSendToSerialLocal(buffer_tx_local, 5);
  serialLocal.write(buffer_tx_local, 5);
}

void SendToPSOC5(uint8 *data, uint16 len, uint16 attrHandle){
  uint8 buffer_tx_local[len +4];
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(attrHandle >> 8);
  buffer_tx_local[2] = (uint8)(attrHandle);
  buffer_tx_local[3] = len; //size
  memcpy(&buffer_tx_local[4],data,len);
  controlSendToSerialLocal(buffer_tx_local, len+4);
}

void SendToPSOC5(char *data, uint16 len, uint16 attrHandle){
  uint8 buffer_tx_local[len +4];
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(attrHandle >> 8);
  buffer_tx_local[2] = (uint8)(attrHandle);
  buffer_tx_local[3] = len; //size
  memcpy(&buffer_tx_local[4],data,len);
  controlSendToSerialLocal(buffer_tx_local, len+4);
}

void SendStatusToPSOC5(uint8_t connected, uint8_t inicializado, uint8_t comm_type){

  uint8 buffer_tx_local[6];
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(BLOQUE_STATUS >> 8);
  buffer_tx_local[2] = (uint8)(BLOQUE_STATUS);
  buffer_tx_local[3] = 3; //size
  buffer_tx_local[4] = connected;
  buffer_tx_local[5] = inicializado;
  buffer_tx_local[6] = comm_type;
  
  controlSendToSerialLocal(buffer_tx_local, 7);
}

uint8_t sendBinaryBlock ( uint8_t *data, int len ){
	if(mainFwUpdateActive)
	{
		int ret=0;
		ret = serialLocal.write(data, len);
		return ret;
	}

	return 1;
}

//----------------------------------------------------------------------------
uint8_t setMainFwUpdateActive (uint8_t val ){
	mainFwUpdateActive = val;
	return mainFwUpdateActive;
}

uint8_t getMainFwUpdateActive (){
  return mainFwUpdateActive;
}

//----------------------------------------------------------------------------
uint16 ParseFirmwareVersion(String Texto){
  String sub = Texto.substring(6, 10); 
  return(sub.toInt());
}
//********************Funciones privadas no accessibles desde fuera***********************************/
int controlSendToSerialLocal ( uint8_t * data, int len ){

	if(!updateTaskrunning){
	  int ret=0;
		ret = serialLocal.write(data, len);

		return ret;
	}
	return 0;
}

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