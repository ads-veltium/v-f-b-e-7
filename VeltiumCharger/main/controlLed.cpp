#include "control.h"
#include "controlLed.h"
#include "FastLED.h"

#define NUM_PIXELS 7

//Extern configuration variables
extern uint8 luminosidad;
extern carac_Update_Status UpdateStatus;
extern carac_Status  Status;
extern carac_Params	  Params;
extern uint8 dispositivo_inicializado;

#ifdef USE_COMS
	extern carac_Contador   ContadorExt;
	extern carac_group    ChargingGroup;
#endif



//Adafruit_NeoPixel strip EXT_RAM_ATTR;
CRGB leds[NUM_PIXELS] EXT_RAM_ATTR;

uint8 Actualcolor;
//Encender un solo led (Y apagar el resto)
void displayOne( uint8_t i, uint8_t color, uint8_t pixeln){
	i=map(i,0,100,0,255);

	for (int j = 0; j < NUM_PIXELS; j++)
	{
		leds[j].setHSV(0,0,0);
	}
	uint8_t s = 255;
	if(color==BLANCO){
		s=0;
	}
	leds[pixeln].setHSV(color, s,i);
	FastLED.show();
}

//Cambiar un solo led (El resto permanecen igual)
void changeOne( uint8_t i, uint8_t color, uint8_t pixeln)
{
	i=map(i,0,100,0,255);
	uint8_t s = 255;
	if(color==BLANCO){
		s=0;
	}
	leds[pixeln].setHSV(color, s,i);
	FastLED.show();
	Actualcolor=color;
}

void displayAll( uint8_t i, uint8_t color)
{
	i=map(i,0,100,0,255);
	uint8_t s = 255;
	if(color==BLANCO){
		s=0;
	}
	FastLED.showColor(CHSV(color, s, i));
	FastLED.showColor(CHSV(color, s, i));
	Actualcolor=color;
}

void initLeds ( void )
{
	digitalWrite(EN_NEOPIXEL,HIGH);
	pinMode( EN_NEOPIXEL, OUTPUT);
	digitalWrite(EN_NEOPIXEL,HIGH);

	FastLED.addLeds<NEOPIXEL, DO_NEOPIXEL>(leds, NUM_PIXELS);

}


/***************************
 *   Efectos de los leds
 * ************************/
void Kit (void){
	
	for (int j = 0; j < 7; j++){
		displayOne(100,ROJO,j);
		vTaskDelay(pdMS_TO_TICKS(50));
	}
	for (int j = 6; j >= 0; j--){
		displayOne(100,ROJO,j);
		vTaskDelay(pdMS_TO_TICKS(50));
	
	}
}


void LedControl_Task(void *arg){
	uint8 subiendo=1, luminosidad_carga= 0, LedPointer =8,luminosidad_Actual=50, togle_led=0;
	uint8 _LED_COLOR=VERDE;
	uint16 cnt_parpadeo=TIME_PARPADEO, Delay= 20;
	bool LastBle=0;
	initLeds();
	/*vTaskDelay(pdMS_TO_TICKS(100));
	displayAll(50,VERDE);	
	vTaskDelay(pdMS_TO_TICKS(350));
	displayAll(50,ROJO);
	vTaskDelay(pdMS_TO_TICKS(350));
	displayAll(50,AZUL_OSCURO);
	vTaskDelay(pdMS_TO_TICKS(350));
	displayAll(50,VERDE);*/

	//Arrancando
	while(dispositivo_inicializado != 2){
		LedPointer++;			
		if(LedPointer>=7){
			if(luminosidad==90){
				_LED_COLOR=0;
				luminosidad=0;
			}
			else{
				_LED_COLOR = VERDE;
				luminosidad=90;
			}
			LedPointer=0;
		} 
		changeOne(luminosidad,_LED_COLOR,LedPointer);
		delay(150);
	}
	_LED_COLOR = 0;
	LedPointer = 0;
	subiendo=0;
	luminosidad_carga=90;

	while(1){


		if(LastBle!=serverbleGetConnected()){	
			displayAll(50,BLANCO);	
			vTaskDelay(pdMS_TO_TICKS(500));
			LastBle=serverbleGetConnected();
		}

		//Instalando archivo
		else if(UpdateStatus.InstalandoArchivo){			
			Delay=150;
			LedPointer++;			
			if(LedPointer>=7){
				if(luminosidad==100){
					_LED_COLOR=0;
					luminosidad=0;
				}
				else{
					_LED_COLOR = HUE_AQUA;
					luminosidad=100;
				}
				LedPointer=0;
			} 
			changeOne(luminosidad,_LED_COLOR,LedPointer);
		}

		//Descargando archivo
		else if(UpdateStatus.DescargandoArchivo){
			Delay=25;
			if(subiendo){
				luminosidad_carga++;
				if(luminosidad_carga>=90){
					subiendo=0;
				}
			}
			else{
				luminosidad_carga--;
				if(luminosidad_carga<=5){
					subiendo=1;
				}
			}
			displayAll(luminosidad_carga,HUE_PURPLE);
		}

#ifdef USE_COMS
		//Buscando Medidor
		else if(Params.Tipo_Sensor && !ContadorExt.ContadorConectado){
			for (int j = 0; j < 7; j++){
				displayOne(90,VERDE,j);
				delay(100);
			}
			delay(100);
			for (int j = 6; j >= 0; j--){
				displayOne(90,VERDE,j);
				delay(100);
			}
		}
#endif
		else{
			
			//Funcionamiento normal
			if(Status.error_code == 0){
				Delay=50;
				if(!memcmp(Status.HPT_status, "C", 1 )) { //Cargando o actualizando		
					uint8 lumin_limit=100;
					
					if(Status.Measures.instant_current>900){
						lumin_limit=(32000-Status.Measures.instant_current)*100/32000;
						if(lumin_limit<10){
							lumin_limit=10;
						}
					}
					if(subiendo){
						luminosidad_carga++;
						if(luminosidad_carga>=lumin_limit){
							subiendo=0;
						}
					}
					else{
						luminosidad_carga--;
						if(luminosidad_carga<=30){
							subiendo=1;
						}
					}
					displayAll(luminosidad_carga,HUE_BLUE);
				}
				else {
					_LED_COLOR = VERDE;
					luminosidad = 90;
					if(!memcmp(Status.HPT_status, "B", 1 )){
						_LED_COLOR = HUE_BLUE;
					}
					//control de saltos entre luminosidad
					if(luminosidad-luminosidad_Actual>=3){
						luminosidad_Actual++;
						displayAll(luminosidad_Actual,_LED_COLOR);
					}
					else if (luminosidad_Actual-luminosidad>=3){
						luminosidad_Actual--;
						displayAll(luminosidad_Actual,_LED_COLOR);	
					}
					else{
						if(Actualcolor!=_LED_COLOR){
							displayAll(luminosidad_Actual,_LED_COLOR);
							delay(10);
							displayAll(luminosidad_Actual,_LED_COLOR);
							delay(10);
							displayAll(luminosidad_Actual,_LED_COLOR);
							delay(10);
						}	
						luminosidad_carga = luminosidad;
						luminosidad_Actual=luminosidad;
						
					}
					
					
				}
			}
			else{//Si hay algun error
				//Error transitorio			 
				if((Status.error_code <= (uint8)0x33 && Status.error_code != (uint8)0x30) || Status.error_code == (uint8)0x80){
					Delay=40;
					if(++cnt_parpadeo >= TIME_PARPADEO){
						cnt_parpadeo = 0;
						if(togle_led == 0)
						{
							togle_led = 1;
							displayAll(0, 0);
						}
						else
						{
							togle_led = 0;
							displayAll(100, ROJO);
						}
					}
				}
				//Error grave
				else if(Status.error_code == (uint8)0x60 || Status.error_code == (uint8)0x70 || Status.error_code == (uint8)0x30){
					displayAll(100,ROJO);
					Delay=1000;
				}
				//Error instalacion
				else if(Status.error_code == (uint8)0x40 || Status.error_code == (uint8)0x50){
					Delay=10;
					if(++cnt_parpadeo >= TIME_PARPADEO)
					{
						cnt_parpadeo = 0;
						if(togle_led == 0)
						{
							togle_led = 1;
							displayAll(100, HUE_YELLOW);
						}
						else
						{
							togle_led = 0;
							displayAll(100, ROJO);
						}
					}
				}
			}

		}	
		vTaskDelay(pdMS_TO_TICKS(Delay));
	}
}