#include "control.h"

/**********************************************
 * 			       PROTOTIPOS
 * *******************************************/
void perform_ps_malloc_tests(uint8_t pot_first, uint8_t pot_last);
void perform_malloc_tests(uint8_t pot_first, uint8_t pot_last);

void setup() 
{
	Serial.begin(115200);
	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreePsram());

	DRACO_GPIO_Init();
	
#ifdef USE_DRACO_BLE
	unsigned char dummySerial[10] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	dev_auth_init(dummySerial);
#endif

#ifdef USE_ETH
	ETH_begin();
#endif // USE_ETH

#ifdef USE_WIFI
	Station_Begin();
	Serial.println("FREE HEAP MEMORY [after WIFI_INIT] **************************");
	Serial.println(ESP.getFreeHeap());

#endif

#ifdef USE_DRACO_BLE
	
	Serial.println("FREE HEAP MEMORY [after DRACO_GPIO_Init] **************************");
	Serial.println(ESP.getFreeHeap());

	Serial.println("FREE HEAP MEMORY [after initLeds] **************************");
	Serial.println(ESP.getFreeHeap());

	serverbleInit();

	Serial.println("FREE HEAP MEMORY [after serverbleInit] **************************");
	Serial.println(ESP.getFreeHeap());

	controlInit();

	Serial.println("FREE HEAP MEMORY [after controlInit write] **************************");
	Serial.println(ESP.getFreeHeap());
#endif

	Serial.println("FREE HEAP MEMORY [after draco init] **************************");
	Serial.println(ESP.getFreeHeap());

}
/**********************************************
 * 			       FUNCIONES
 * *******************************************/

void perform_ps_malloc_tests(uint8_t pot_first, uint8_t pot_last)
{
	for (uint8_t pot = pot_first; pot <= pot_last; pot++) {
		unsigned bytesToAllocate = 1 << pot;
		Serial.printf("[before] free heap memory: %7u bytes\n", ESP.getFreeHeap());
		Serial.printf("ps_mallocating %7u bytes... ", bytesToAllocate);
		void *buf = ps_malloc(bytesToAllocate);
		if (buf != NULL) {
			Serial.println("OK.");
			Serial.printf("[after ] free heap memory: %7u bytes\n", ESP.getFreeHeap());
			free(buf);
			Serial.println("buffer released.");
		}
		else {
			Serial.println("FAILURE!!!");
			break;
		}
	}
}

void perform_malloc_tests(uint8_t pot_first, uint8_t pot_last)
{
	for (uint8_t pot = pot_first; pot <= pot_last; pot++) {
		unsigned bytesToAllocate = 1 << pot;
		Serial.printf("[before] free heap memory: %7u bytes\n", ESP.getFreeHeap());
		Serial.printf("mallocating %7u bytes... ", bytesToAllocate);
		void *buf = malloc(bytesToAllocate);
		if (buf != NULL) {
			Serial.println("OK.");
			Serial.printf("[after ] free heap memory: %7u bytes\n", ESP.getFreeHeap());
			free(buf);
			Serial.println("buffer released.");
		}
		else {
			Serial.println("FAILURE!!!");
			break;
		}
	}
}



