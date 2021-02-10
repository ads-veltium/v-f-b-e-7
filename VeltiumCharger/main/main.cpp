#include "control.h"

void setup() 
{
	Serial.begin(115200);
	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreePsram());

	DRACO_GPIO_Init();
	
#ifdef USE_DRACO_BLE

	unsigned char dummySerial[10] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	dev_auth_init(dummySerial);
	
	Serial.println("FREE HEAP MEMORY [after DRACO_GPIO_Init] **************************");
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