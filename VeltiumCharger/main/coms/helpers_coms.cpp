#include "helper_coms.h"
#ifdef CONNECTED
static const char* TAG = "helpers_coms";

void Update_Status_Coms(uint16_t Code, uint8_t block){
	static uint16 Status_Coms = 0;
	static uint16 Last_Status = 0;

    ESP_LOGI(TAG,"Update_Status_Coms. Code=%i block=%i",Code,block);

	if(Code > 0){
		//Resetear los bits que estuvieran activos por cada bloque
		if (Code <= ETH_CONNECTED){
			Status_Coms &= 0b1111111111111100;
		}
		else if(Code <= WIFI_BAD_CREDENTIALS){
			Status_Coms &= 0b1111111111100011;
		}
		else if(Code <= MED_CONECTION_RESTAURED){
			Status_Coms &= 0b1111111100011111;
		}
		else if(Code <= MODEM_CONNECTED){
			Status_Coms &= 0b0000001111111111;
		}	
		Status_Coms |= Code;
	}
	else{
		switch (block){
			case ETH_BLOCK:
				Status_Coms &= 0b1111111111111100;
				break;
			case WIFI_BLOCK:
				Status_Coms &= 0b1111111111100011;
				break;
			case MED_BLOCK:
				Status_Coms &= 0b1111110000011111;
				break;
			case MODEM_BLOCK:
				Status_Coms &= 0b0000001111111111;
				break;
		}
	}

	if(Last_Status != Status_Coms){
		#ifdef DEBUG
		printf("Nuevo status coms: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",BYTE_TO_BINARY( Status_Coms >> 8), BYTE_TO_BINARY( Status_Coms ));
		#endif
		Last_Status = Status_Coms;
		uint8 data[2];
		data[0] = (uint8)(Status_Coms & 0x00FF);
		data[1] = (uint8)((Status_Coms >> 8) & 0x00FF);
		serverbleNotCharacteristic((uint8_t*)data, 2, STATUS_COMS);
	}
}


uint32 GetStateTime(TickType_t xStart){
	return (pdTICKS_TO_MS(xTaskGetTickCount() - xStart));
}

#endif