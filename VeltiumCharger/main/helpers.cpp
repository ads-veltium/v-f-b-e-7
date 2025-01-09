#include "control.h"
#include "helpers.h"
#include "lwip/dns.h"

extern HardwareSerialMOD serialLocal;
extern uint8_t mainFwUpdateActive;
extern uint8_t updateTaskrunning;
extern uint8 deviceSerNumFlash[10];
extern carac_group  ChargingGroup;

const static char *TAG = "helpers";

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
    Serial.printf("================================================= %s =================================================\n", table_name);
    Serial.printf("\tID\tFase\tHPT\tI\tConsig\tDelta\tDeltaT\tConnect\tPeriod\tCircuit\tCharReq\tOrder\tIP Add\n");
    for(int i=0; i< size;i++){     //comprobar si el cargador ya está almacenado
        Serial.printf("  %s\t%i\t%s\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%s\n",table[i].name,table[i].Fase,table[i].HPT,table[i].Current, table[i].Consigna, table[i].Delta,  table[i].Delta_timer, table[i].Conected, table[i].Period, table[i].Circuito, table[i].ChargeReq, table[i].order, table[i].IP.toString().c_str());
    }
    Serial.printf("Memoria interna disponible: %i\n", esp_get_free_internal_heap_size());
    Serial.printf("Memoria total disponible:   %i\n", esp_get_free_heap_size());
    Serial.printf("==================================================================================================\n");
}

void print_net_table(carac_charger *table, char* table_name = "Grupo de cargadores", uint8_t size = 0){
    Serial.printf("================ %s ================\n", table_name);
    Serial.printf("\tID\tIP Add\n");
    for(int i=0; i< size;i++){     //comprobar si el cargador ya está almacenado
        Serial.printf("  %s\t%s\n",table[i].name,table[i].IP.toString().c_str());
    }
    Serial.printf("================================================\n");
}

void print_group_param(carac_group* group){
    Serial.printf("================================================= Parámetros =================================================\n");
    Serial.printf("Perm\tNewData\tConect\tStopOrd\tDelOrd\tStartCl\tChargP\tFind\tCreando\tGrAct\tGrMast\n");
    Serial.printf("  %i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\n",group->AskPerm,group->NewData,group->Conected,group->StopOrder,group->DeleteOrder,group->StartClient,group->ChargPerm,group->Finding,group->Creando,group->Params.GroupActive,group->Params.GroupMaster);
    Serial.printf("==============================================================================================================\n");
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
void SendToPSOC5(uint8_t data, uint16_t attrHandle){
  uint8_t buffer_tx_local[5];
  int err;
  //cnt_timeout_tx = TIMEOUT_TX_BLOQUE2;
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8_t)(attrHandle >> 8);
  buffer_tx_local[2] = (uint8_t)(attrHandle);
  buffer_tx_local[3] = 1; //size
  buffer_tx_local[4] = data;
  err = controlSendToSerialLocal(buffer_tx_local, 5);
  // err=serialLocal.write(buffer_tx_local, 5);
#ifdef DEBUG_TX_UART
  ESP_LOGI(TAG,"SenToPSOC5 - %i bytes sent to PSoC. attrHandle=0x%X",err,attrHandle);
#endif   
}

void SendToPSOC5(uint8 *data, uint16 len, uint16 attrHandle){
  uint8 buffer_tx_local[len +4];
  int err;
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(attrHandle >> 8);
  buffer_tx_local[2] = (uint8)(attrHandle);
  buffer_tx_local[3] = len; //size
  memcpy(&buffer_tx_local[4],data,len);
  err = controlSendToSerialLocal(buffer_tx_local, len+4);
#ifdef DEBUG_TX_UART
  ESP_LOGI(TAG,"SenToPSOC5(int) - %i bytes sent to PSoC. attrHandle=0x%X",err,attrHandle);
#endif 
}

void SendToPSOC5(char *data, uint16 len, uint16 attrHandle){
  uint8 buffer_tx_local[len +4];
  int err;
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(attrHandle >> 8);
  buffer_tx_local[2] = (uint8)(attrHandle);
  buffer_tx_local[3] = len; //size
  memcpy(&buffer_tx_local[4],data,len);
  err = controlSendToSerialLocal(buffer_tx_local, len+4);
#ifdef DEBUG_TX_UART
  ESP_LOGI(TAG,"SenToPSOC5(char) - %i bytes sent to PSoC. attrHandle=0x%X. data=[%s]",err,attrHandle,data);
#endif 
}

void SendStatusToPSOC5(uint8_t connected, uint8_t inicializado, uint8_t comm_type){
  uint8 buffer_tx_local[6];
  int err;
  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(BLOQUE_STATUS >> 8);
  buffer_tx_local[2] = (uint8)(BLOQUE_STATUS);
  buffer_tx_local[3] = 3; //size
  buffer_tx_local[4] = connected;
  buffer_tx_local[5] = inicializado;
  buffer_tx_local[6] = comm_type;
  err=controlSendToSerialLocal(buffer_tx_local, 7);
#ifdef DEBUG_TX_UART
  ESP_LOGI(TAG,"SendStatusToPSOC5 %i bytes sent to PSoC.\n",err);
#endif 
}

void SendScheduleMatrixToPSOC5(uint8_t *data) {
  const uint8_t size = 24; // 24 horas
  uint8_t buffer_tx_local[size + 1 + 4]; //24 horas + día + cabecera

  buffer_tx_local[0] = HEADER_TX;
  buffer_tx_local[1] = (uint8)(SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE >> 8);
  buffer_tx_local[2] = (uint8)(SCHED_CHARGING_SCHEDULE_MATRIX_CHAR_HANDLE);
  buffer_tx_local[3] = size + 1; 

  for (uint8_t day = 0; day < 7; ++day) {
    buffer_tx_local[4] = day; 
    memcpy(&buffer_tx_local[5], &data[day * size], size);
    int err = controlSendToSerialLocal(buffer_tx_local, size + 4);
#ifdef DEBUG_TX_UART
    Serial.printf("SendMatrixToPSOC5 (day %u): %i bytes sent.\n", day, err);
#endif
    delay(10); 
  }
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
uint16 ParseFirmwareVersion(String Texto){ //SACA EL NÚMERO DE LA VERSIÓN
  String sub = Texto.substring(6, 10); 
  return(sub.toInt());
}


String ParseFirmwareModel(String Texto){ //SACA VELTx o VBLEx
  String sub = Texto.substring(0, 5); 
  return(sub);
}
//********************Funciones privadas no accessibles desde fuera***********************************/
int controlSendToSerialLocal ( uint8_t * data, int len ){

	if(!updateTaskrunning){
#ifdef DEBUG_TX_UART
    for (int i = 0; i < len; i++) {
      ESP_LOGI(TAG, "controlSendToSerialLocal - Byte %d: 0x%02X", i, data[i]);
    }
    ESP_LOGI(TAG,"controlSendToSerialLocal - len=[%i]",len);
#endif
	  int ret=0;
		ret = serialLocal.write(data, len+1);
    // NOTA: Se envía 1 byte adicional por compatibilidad en la recepcion el PSoC. NO ELIMINAR 
		return ret;
	}
	return 0;
}

int Convert_To_Epoch(uint8* data){
	struct tm t = {0};  // Initalize to all 0's
	t.tm_year = (2000+data[2])-1900;  // This is year-1900, so 112 = 2012
	t.tm_mon  = (data[1]!=0)?data[1]-1:0;
	t.tm_mday = data[0];
	t.tm_hour = data[3];
	t.tm_min  = data[4];
	t.tm_sec  = data[5];
	return mktime(&t);
	}

void convertSN(){
  char temp[2]={0,0};
	int i=0;
	int j=0;
	while (i<20){
		memcpy(temp,&Configuracion.data.deviceSerNum[i],2);
		i=i+2;
		deviceSerNumFlash[j]= (int)strtol(temp,NULL,16);
		j++;
	}
}

bool ContadorConfigurado(){
  return ((ChargingGroup.Params.CDP >> 4) & 0x01);
}

#ifdef IS_UNO_KUBO

void SetDNS(){
    ESP_LOGD(TAG,"SetDNS() - Dirección del DNS Principal: %s", ipaddr_ntoa(dns_getserver(ESP_NETIF_DNS_MAIN)));
    ESP_LOGD(TAG,"SetDNS() - Dirección del DNS Backup: %s", ipaddr_ntoa(dns_getserver(ESP_NETIF_DNS_BACKUP)));
    ip_addr_t dns_main_server_info;
    ip_addr_t dns_backup_server_info;

    IP4_ADDR(ip_2_ip4(&dns_main_server_info), DNS_MAIN_SERVER_IP[0], DNS_MAIN_SERVER_IP[1], DNS_MAIN_SERVER_IP[2], DNS_MAIN_SERVER_IP[3]);
    IP4_ADDR(ip_2_ip4(&dns_backup_server_info), DNS_BACKUP_SERVER_IP[0], DNS_BACKUP_SERVER_IP[1], DNS_BACKUP_SERVER_IP[2], DNS_BACKUP_SERVER_IP[3]);

    dns_setserver(ESP_NETIF_DNS_MAIN, &dns_main_server_info);
    dns_setserver(ESP_NETIF_DNS_BACKUP, &dns_backup_server_info);

    ESP_LOGD(TAG,"SetDNS() - Nueva dirección del DNS Principal: %s", ipaddr_ntoa(dns_getserver(ESP_NETIF_DNS_MAIN)));
    ESP_LOGD(TAG,"SetDNS() - Nueva dirección del DNS Backup: %s", ipaddr_ntoa(dns_getserver(ESP_NETIF_DNS_BACKUP)));
    dns_init();
}

int obtener_direccion_IP(const String &host_name, String &ip_address) {
  // MODIFICAR PARA USAR EL CALLBACK. VER dns_found_callback
    ip_addr_t addr;
    err_t err = dns_gethostbyname(host_name.c_str(), &addr, NULL, NULL);
    if (err == ERR_OK) {
        char ip_str[16]; // Suficiente para almacenar una dirección IPv4 en formato de texto
        ip4_addr_t *ip4 = (ip4_addr_t *)&addr.u_addr.ip4;
        sprintf(ip_str, IPSTR, IP2STR(ip4));
        ip_address = String(ip_str); // Guarda la dirección IP en el argumento de salida
        ESP_LOGE(TAG,"obtener_direccion_IP - HOST: %s - IP: %s", host_name.c_str(), ip_str);
        return 0; // Devuelve 0 para indicar éxito
    } else {
        ip_address = ""; // Asigna una cadena vacía en caso de error
        ESP_LOGE(TAG,"obtener_direccion_IP - No se puede resolver la direccion del HOST: %s", host_name.c_str());
        return -1; // Devuelve -1 para indicar error
    }
}
#endif
