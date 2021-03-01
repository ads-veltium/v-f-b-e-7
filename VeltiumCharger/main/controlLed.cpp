#include "control.h"
#include "controlLed.h"
#include "FastLED.h"

#define NUM_PIXELS 7

//Extern configuration variables
extern uint8 luminosidad;
extern carac_Update_Status UpdateStatus;
extern carac_Status  Status;

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
		displayOne(100,HUE_RED,j);
		vTaskDelay(pdMS_TO_TICKS(50));
	}
	for (int j = 7; j >= 0; j--){
		displayOne(100,HUE_RED,j);
		vTaskDelay(pdMS_TO_TICKS(50));
	
	}
}


void LedControl_Task(void *arg){
	uint8 subiendo=1, luminosidad_carga= 0, LedPointer =0,luminosidad_Actual=50, togle_led=0;
	uint8 _LED_COLOR=0;
	uint16 cnt_parpadeo=TIME_PARPADEO, Delay= 20;
	uint8 Efectos=0;
	bool LastBle=0;
	initLeds();
	vTaskDelay(pdMS_TO_TICKS(100));
	displayAll(50,VERDE);	
	vTaskDelay(pdMS_TO_TICKS(350));
	displayAll(50,ROJO);
	vTaskDelay(pdMS_TO_TICKS(350));
	displayAll(50,AZUL_OSCURO);
	vTaskDelay(pdMS_TO_TICKS(350));
	displayAll(50,VERDE);

	while(1){
		//conexion/Desconexion ble
		if(LastBle!=serverbleGetConnected()){		
			displayAll(50,BLANCO);	
			vTaskDelay(pdMS_TO_TICKS(500));
			LastBle=serverbleGetConnected();
		}
		if(UpdateStatus.InstalandoArchivo && !Efectos){
			Efectos=1;
			LedPointer=0;
			Delay=150;
			luminosidad=100;
			_LED_COLOR=HUE_AQUA;
		}
		else if(UpdateStatus.DescargandoArchivo){
			Delay=75;
			luminosidad |= 0x80;
			_LED_COLOR=HUE_PURPLE;
		}

		Efectos=UpdateStatus.InstalandoArchivo;

		//Descarga:
		if(Efectos){			
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
						luminosidad_carga = luminosidad;
						luminosidad_Actual=luminosidad;
						if(Actualcolor!=_LED_COLOR){
							displayAll(luminosidad_Actual,_LED_COLOR);
							delay(10);
							displayAll(luminosidad_Actual,_LED_COLOR);
							delay(10);
							displayAll(luminosidad_Actual,_LED_COLOR);
							delay(10);
						}	
					}
					
					
				}
			}
			else{//Si hay algun error
				//Error transitorio	
				Serial.println("Error!");			
				if(Status.error_code <= (uint8)0x33 && Status.error_code != (uint8)0x30){
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