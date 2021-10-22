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
extern carac_Coms Coms;

#ifdef USE_COMS
	extern carac_Contador   ContadorExt;
	#ifdef CONNECTED
	extern carac_group    ChargingGroup;
	#endif
#endif



//Adafruit_NeoPixel strip EXT_RAM_ATTR;
CRGB leds[NUM_PIXELS] EXT_RAM_ATTR;

uint8 Actualcolor;
uint8 luminosidad = 90;
uint8 LedPointer = 0;
uint8 toggle_led  = 0;
uint16 cnt_parpadeo=TIME_PARPADEO;
bool subiendo    = 1;
uint8 Fadecolor = 0;
uint8 _LED_COLOR=VERDE;
uint8 luminosidad_Actual=85;


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
	displayAll(0,0);
}


/***************************
 *   Efectos de los leds
 * ************************/
//
void OutWave( uint8_t color){
	static uint8_t pointer = 0;
	static bool down = true;

	if(!down){
		luminosidad+=3;
		if(luminosidad >= 100){
			pointer ++;
			luminosidad = 0;
			if (pointer >= 4){
				luminosidad = 100;
				pointer = 0;
				down = true;
			}
		}
		changeOne(luminosidad,color,3-pointer);
		changeOne(luminosidad,color,3+pointer);
	}
	else{
		luminosidad --;
		displayAll(luminosidad,color);

		if(luminosidad <= 2){
			luminosidad = 0;
			for (int j = 0; j < NUM_PIXELS; j++)
			{
				leds[j].setHSV(0,0,0);
			}
			FastLED.show();
			down = false;
		}
	}
	

}

void InWave( uint8_t color){
	static uint8_t pointer = 4;
	static bool down = true;

	if(!down){
		luminosidad+=2;
		if(luminosidad >= 100){
			pointer --;
			luminosidad = 0;
			if (pointer <= 0){
				luminosidad = 100;
				pointer = 4;
				down = true;
			}
		}
		changeOne(luminosidad,color,3-(pointer-1));
		changeOne(luminosidad,color,3+(pointer-1));
	}
	else{
		luminosidad -=4;
		displayAll(luminosidad,color);

		if(luminosidad <= 2){
			luminosidad = 0;
			for (int j = 0; j < NUM_PIXELS; j++)
			{
				leds[j].setHSV(0,0,0);
			}
			FastLED.show();
			down = false;
		}
	}
}

void Kit (uint8_t color){
	if(subiendo){
		LedPointer++;
		if(LedPointer==6){
			subiendo = false;
		}
	}
	else{
		LedPointer--;
		if(LedPointer==0){
			subiendo = true;
		}
	}

	uint8_t s = color == BLANCO ? 0:255;

	if(LedPointer-4 >= 0)
		leds[LedPointer-4].setHSV(color, s,25);
	if(LedPointer-3 >= 0)
		leds[LedPointer-3].setHSV(color, s,65);
	if(LedPointer-3 >= 0)
		leds[LedPointer-2].setHSV(color, s,125);
	if(LedPointer-1 >= 0)
		leds[LedPointer-1].setHSV(color, s,195);
	leds[LedPointer].setHSV(color, s,255);
	if(LedPointer+1 <= 6)
		leds[LedPointer+1].setHSV(color, s,195);
	if(LedPointer+2 <= 6)
		leds[LedPointer+2].setHSV(color, s,125);
	if(LedPointer+3 <= 6)
		leds[LedPointer+3].setHSV(color, s,65);
	if(LedPointer+4 <= 6)
		leds[LedPointer+4].setHSV(color, s,25);
	FastLED.show();
	luminosidad_Actual=85;
}

//Efecto de ola
void OLA(uint8 color){
	if(subiendo){
		luminosidad +=4;
		if(luminosidad >= 90){
			LedPointer++;
			luminosidad = 0;
			if(LedPointer>=7){
				LedPointer = 0;
				luminosidad = 90;
				subiendo = false;
			}
		}
	}
	else{
		luminosidad -=4;
		if(luminosidad <= 5){
			LedPointer++;
			luminosidad = 90;
			if(LedPointer>=7){
				LedPointer = 0;
				luminosidad = 0;
				subiendo = true;
			}
		}
	}

	changeOne(luminosidad,color,LedPointer);
	luminosidad_Actual=85;
}

//
void Fade(uint8 color){
					
	if(subiendo){
		luminosidad++;
		if(luminosidad>=90){
			subiendo=0;
		}
	}
	else{
		luminosidad--;
		if(luminosidad<=30){
			subiendo=1;
		}
	}
	displayAll(luminosidad,color);
	luminosidad_Actual=85;
}

void Parpadeo(uint8 color){
					
	if(++cnt_parpadeo >= TIME_PARPADEO)	{
		cnt_parpadeo = 0;
		if(toggle_led == 0)
		{
			toggle_led = 1;
			displayAll(100, HUE_YELLOW);
		}
		else
		{
			toggle_led = 0;
			displayAll(100, ROJO);
		}
	}
	luminosidad_Actual=85;
}

void FadeDoble(uint8 color1, uint8 color2){
	
	if(Fadecolor ==0){
		Fadecolor = color1;
	}
					
	if(subiendo){
		luminosidad++;
		if(luminosidad>=90){
			subiendo=0;
			Fadecolor = color1;
		}
	}
	else{
		luminosidad--;
		if(luminosidad<=30){
			subiendo=1;
			Fadecolor = color2;
		}
	}
	displayAll(luminosidad,Fadecolor);
	luminosidad_Actual=85;

}

void TransicionAverde(){

	while(1){
		if(subiendo){
			luminosidad +=4;
			if(luminosidad >= 90){
				LedPointer++;
				luminosidad = 0;
				if(LedPointer>=7){
					changeOne(luminosidad,VERDE,LedPointer);
					break;
				}
			}
		}
		else{
			luminosidad -=4;
			if(luminosidad <= 5){
				LedPointer++;
				luminosidad = 90;
				if(LedPointer>=7){
					LedPointer = 0;
					luminosidad = 0;
					subiendo = true;
				}
			}
		}
		delay(5);
		changeOne(luminosidad,VERDE,LedPointer);
	}
	
}
void Reset_Values(){
	
	luminosidad = 90;
	LedPointer  = 8;
	toggle_led  = 0;
	subiendo    = 0;
	_LED_COLOR=VERDE;
	cnt_parpadeo=TIME_PARPADEO;
	Fadecolor =0;
}


void LedControl_Task(void *arg){
	
	uint16 Delay= 20;
	bool LastBle=0;
	initLeds();
	Reset_Values();
	//Arrancando
	while(dispositivo_inicializado != 2){
		OLA(VERDE);
		delay(5);
	}
	TransicionAverde();
	Reset_Values();
	

	while(1){


		if(LastBle!=serverbleGetConnected()){	
			displayAll(90,BLANCO);	
			delay(500);
			LastBle=serverbleGetConnected();
		}

		//Instalando archivo
		else if(UpdateStatus.InstalandoArchivo){			
			OLA(HUE_AQUA);
			Delay = 7;
		}

		//Descargando archivo
		else if(UpdateStatus.DescargandoArchivo){
			Delay=25;
			Fade(HUE_PURPLE);
		}

#ifdef USE_COMS
		else if((Params.Tipo_Sensor || (ChargingGroup.Params.GroupMaster && ChargingGroup.Params.CDP >> 4)) && ContadorExt.ConexionPerdida){
			Kit(NARANJA_OSCURO);
			Delay= 85;
		}
		//Buscando Medidor
		else if((Params.Tipo_Sensor || (ChargingGroup.Params.GroupMaster &&  ChargingGroup.Params.CDP >> 4))  && !ContadorExt.MeidorConectado && !Coms.Provisioning){
			//Buscando Gateway
			if(!ContadorExt.GatewayConectado){
				if(!Coms.ETH.DHCP){
					InWave(VERDE);
				}
				else{
					OutWave(VERDE);
				}
				Delay=7;
			}	
			//Gateway encontrado pero medidor no
			else if(ContadorExt.GatewayConectado ){
				Kit(VERDE);
				Delay=85;
			}
		}


#endif
		else{
			
			//Funcionamiento normal
			if(Status.error_code == 0){
				
				if(!memcmp(Status.HPT_status, "C", 1 )) { //Cargando		
					Delay=50;
					if(Status.Measures.instant_current>600){
						Delay = map(Status.Measures.instant_current, 600, 3500, 40, 5);
					}
					Fade(HUE_BLUE);
				}
				else {
					Reset_Values();
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
						luminosidad_Actual=luminosidad;	
					}					
				}
			}
			else{//Si hay algun error
				//Error transitorio			 
				if((Status.error_code <= (uint8)0x33 && Status.error_code != (uint8)0x30) || Status.error_code == (uint8)0x80){
					Delay=40;
					Fade(ROJO);
				}
				//Error grave
				else if(Status.error_code == (uint8)0x60 || Status.error_code == (uint8)0x70 || Status.error_code == (uint8)0x30){
					displayAll(100,ROJO);
					Delay=1000;
				}
				//Error instalacion
				else if(Status.error_code == (uint8)0x40 || Status.error_code == (uint8)0x50){
					Delay=10;
					FadeDoble(NARANJA_OSCURO, ROJO);
				}
			}

		}	
		vTaskDelay(pdMS_TO_TICKS(Delay));
	}
}