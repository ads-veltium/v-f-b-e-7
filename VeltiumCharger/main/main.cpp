
//Descomentar el include de abajo para activar las 
//Funciones de test
//#include "test.h"

#ifndef TESTING

#include "control.h"
#endif

void setup() 
{
	Serial.begin(115200);
	#ifndef TESTING
	
	#ifdef DEBUG
	Serial.println("FREE HEAP MEMORY [initial] **************************");
	Serial.println(ESP.getFreePsram());
	Serial.println(ESP.getFreeHeap());
	#endif

	DRACO_GPIO_Init();
	
	if (checkSpiffsBug())
	{
		Serial.println("*************\n SPIFFS ERROR: Reiniciando ESP...");
		
		ESP.restart();
	}
	
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

	#else
	prueba();
	
	#endif
}