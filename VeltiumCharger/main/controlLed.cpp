#include "control.h"
#include "controlLed.h"
#include "FastLED.h"

#define NUM_PIXELS 7

//Extern configuration variables
extern uint8 luminosidad, rojo, verde, azul;
extern carac_Auto_Update AutoUpdate;
extern carac_Status  Status;

//Adafruit_NeoPixel strip EXT_RAM_ATTR;
CRGB leds[NUM_PIXELS] EXT_RAM_ATTR;

//Encender un solo led (Y apagar el resto)
void displayOne( uint8_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t pixeln)
{
	i=map(i,0,100,0,255);
	r=map(r,0,100,0,255);
	g=map(g,0,100,0,255);
	b=map(b,0,100,0,255);

	for (int j = 0; j < NUM_PIXELS; j++)
	{
		leds[j].r = r; 
    	leds[j].g = g; 
    	leds[j].b = b;
	}
	FastLED.setBrightness(i);
	leds[pixeln].r = r; 
    leds[pixeln].g = g; 
    leds[pixeln].b = b;
	FastLED.show();
}

//Cambiar un solo led (El resto permanecen igual)
void changeOne( uint8_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t pixeln)
{
	i=map(i,0,100,0,255);
	r=map(r,0,100,0,255);
	g=map(g,0,100,0,255);
	b=map(b,0,100,0,255);

	FastLED.setBrightness(i);
	leds[pixeln].r = r; 
    leds[pixeln].g = g; 
    leds[pixeln].b = b;
	FastLED.show();
}

//Cambiar un solo led (El resto permanecen igual)
void displayAll( uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
	i=map(i,0,100,0,255);
	r=map(r,0,100,0,255);
	g=map(g,0,100,0,255);
	b=map(b,0,100,0,255);

	FastLED.setBrightness(i );
	for(int dot = 0; dot < NUM_PIXELS; dot++) { 
		leds[dot].r = r; 
    	leds[dot].g = g; 
    	leds[dot].b = b;
		FastLED.show();
	}

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
		displayOne(100,100,0,0,j);
		vTaskDelay(550/portTICK_PERIOD_MS);
	}
	for (int j = 7; j >= 0; j--){
		displayOne(100,100,0,0,j);
		vTaskDelay(550/portTICK_PERIOD_MS);
	}
}


void LedControl_Task(void *arg){
	uint8 subiendo=1, luminosidad_carga= 0, LedPointer =0,luminosidad_Actual=50, togle_led=0;
	uint16 cnt_parpadeo=0, Delay= 20;
	uint8 Efectos=0;

	initLeds();

	while(1){
		if(AutoUpdate.DescargandoArchivo){
			Efectos=1;
		}

		//Descarga:
		if(Efectos){
			Delay=150;
			luminosidad=100;
			LedPointer++;
			if(LedPointer>=7){
				if(luminosidad==100){
					rojo=0;
					verde=0;
					azul=0;
					luminosidad=0;
				}
				else{
					rojo=50;
					verde=80;
					azul=100;
					luminosidad=100;
				}
				LedPointer=0;
			} 
			changeOne(100,rojo,verde,azul,LedPointer);
		}
		else{
			
			//Funcionamiento normal
			if (luminosidad & 0x80 && rojo == 0 && verde == 0){ //Cargando				
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
			    Delay=20;
				displayAll(luminosidad_carga,rojo,verde,azul);
			}
			else if(luminosidad & 0x80){ //Parpadeo
				if(++cnt_parpadeo >= TIME_PARPADEO)
				{
					cnt_parpadeo = 0;
					if(togle_led == 0)
					{
						togle_led = 1;
						displayAll(0, rojo, verde, azul);
					}
					else
					{
						togle_led = 0;
						displayAll((luminosidad & 0x7F), rojo, verde, azul);
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
				Delay=500;
				displayAll(luminosidad_Actual,rojo,verde,azul);	
			}

		}

		
		vTaskDelay(pdMS_TO_TICKS(Delay));
	}
}