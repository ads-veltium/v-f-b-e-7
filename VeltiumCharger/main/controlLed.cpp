#include "control.h"
#include "controlLed.h"
#include "FastLED.h"

#define NUM_PIXELS 7

//Extern configuration variables
extern uint8 luminosidad, Led_color;
extern carac_Update_Status UpdateStatus;
extern carac_Status  Status;

//Adafruit_NeoPixel strip EXT_RAM_ATTR;
CRGB leds[NUM_PIXELS] EXT_RAM_ATTR;

//Encender un solo led (Y apagar el resto)
void displayOne( uint8_t i, uint8_t color, uint8_t pixeln){
	i=map(i,0,100,0,255);

	for (int j = 0; j < NUM_PIXELS; j++)
	{
		leds[j].setHSV(0,0,0);
	}
	leds[pixeln].setHSV(color, 255,i);
	FastLED.show();
}

//Cambiar un solo led (El resto permanecen igual)
void changeOne( uint8_t i, uint8_t color, uint8_t pixeln)
{
	i=map(i,0,100,0,255);

	leds[pixeln].setHSV(color, 255,i);
	FastLED.show();
}

void displayAll( uint8_t i, uint8_t color)
{
	i=map(i,0,100,0,255);
	FastLED.showColor(CHSV(color, 255, i));
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
		vTaskDelay(50/portTICK_PERIOD_MS);
	}
	for (int j = 7; j >= 0; j--){
		displayOne(100,HUE_RED,j);
		vTaskDelay(50/portTICK_PERIOD_MS);
	
	}
}


void LedControl_Task(void *arg){
	uint8 subiendo=1, luminosidad_carga= 0, LedPointer =0,luminosidad_Actual=50, togle_led=0;
	uint8 _LED_COLOR=0;
	uint16 cnt_parpadeo=0, Delay= 20;
	uint8 Efectos=0;

	initLeds();

	while(1){
		
		if(UpdateStatus.InstalandoArchivo && !Efectos){
			Efectos=1;
			LedPointer=0;
			Delay=150;
			luminosidad=100;
			Led_color=HUE_AQUA;
		}
		else if(UpdateStatus.DescargandoArchivo){
			Delay=75;
			luminosidad |= 0x80;
			Led_color=HUE_PURPLE;
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
			if (luminosidad & 0x80 && (Led_color==HUE_BLUE || Led_color==HUE_PURPLE)) { //Cargando o actualizando		
				uint8 lumin_limit=50;

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
					if(luminosidad_carga<=0){
						subiendo=1;
					}
				}
				displayAll(luminosidad_carga,Led_color);
			}
			else if(luminosidad & 0x80){ //Parpadeo
				if(++cnt_parpadeo >= TIME_PARPADEO)
				{
					cnt_parpadeo = 0;
					if(togle_led == 0)
					{
						togle_led = 1;
						displayAll(0, 0);
					}
					else
					{
						togle_led = 0;
						displayAll((luminosidad & 0x7F), Led_color);
					}
				}
			}
			else if(luminosidad < 0x80){

				//control de saltos entre luminosidad
				if(luminosidad-luminosidad_Actual>=3){
					luminosidad_Actual++;
				}
				else if (luminosidad_Actual-luminosidad>=3){
					luminosidad_Actual--;
				}
				else{
					luminosidad_Actual=luminosidad;
				}
				Delay=100;
				displayAll(luminosidad_Actual,Led_color);	
			}

		}

		
		vTaskDelay(pdMS_TO_TICKS(Delay));
	}
}