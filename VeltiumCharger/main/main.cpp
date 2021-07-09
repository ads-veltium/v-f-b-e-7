#include "control.h"



void setup() 
{
	Serial.begin(115200);

	#ifdef DEBUG
	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreePsram());
	Serial.println(ESP.getFreeHeap());
	#endif

	DRACO_GPIO_Init();
	unsigned char dummySerial[10] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	dev_auth_init(dummySerial);
	
	#ifdef DEBUG
	Serial.println("FREE HEAP MEMORY [after DRACO_GPIO_Init] **************************");
	Serial.println(ESP.getFreeHeap());
	#endif

	serverbleInit();
	
	#ifdef DEBUG
	Serial.println("FREE HEAP MEMORY [after serverbleInit] **************************");
	Serial.println(ESP.getFreeHeap());
	#endif

	controlInit();

	#ifdef DEBUG
	Serial.println("FREE HEAP MEMORY [after controlInit write] **************************");
	Serial.println(ESP.getFreeHeap());
	#endif
	
}