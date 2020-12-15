#include <Arduino.h>
#include "DRACO_IO.h"
#include "WS2812.h"
#include "controlLed.h"

//NEOPIXELS 
#define  NUM_PIXELS  7  
uint8_t MAX_COLOR_VAL = 255;

//rgbVal *pixels;
rgbVal myColor;
rgbVal pixels[NUM_PIXELS];
rgbVal pixelsTmp[NUM_PIXELS];


const uint8_t gamma8[] = 
{
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
	2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
	5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
	10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
	17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
	25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
	37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
	51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
	69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
	90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
	115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
	144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
	177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
	215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 
};


int correctGammaColor ( void  )
{
	int i = 0 ;

	for ( i = 0 ; i< NUM_PIXELS; i++ )
	{
		pixelsTmp[i].r = gamma8[pixels[i].r];
		pixelsTmp[i].g = gamma8[pixels[i].g];
		pixelsTmp[i].b = gamma8[pixels[i].b];
	}
	return 1;
}

void displayOne( uint8_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t pixeln)
{
	for (int j = 0; j < NUM_PIXELS; j++)
	{
		pixels[j] = makeIRGBVal(0, 0, 0, 0);
	}
	pixels[pixeln] = makeIRGBVal(i, r, g, b);

	ws2812_setColors(NUM_PIXELS, pixels);
}


void displayAll( uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
	for (int j = 0; j < NUM_PIXELS; j++)
	{
		pixels[j] = makeIRGBVal(i, r, g, b);
	}
	ws2812_setColors(NUM_PIXELS, pixels);

}


void displayOff()
{
	for (int i = 0; i < NUM_PIXELS; i++)
	{
		pixels[i] = makeRGBVal(0, 0, 0);
	}
	ws2812_setColors(NUM_PIXELS, pixels);
}

void initLeds ( void )
{
	digitalWrite(EN_NEOPIXEL,HIGH);
	pinMode( EN_NEOPIXEL, OUTPUT);
	digitalWrite(EN_NEOPIXEL,HIGH);

	if(ws2812_init(DO_NEOPIXEL, LED_WS2812B))
	{
		while (1)
		{
			vTaskDelay(pdMS_TO_TICKS(100));
		}
	}
}

void Kit (void){
	
	for (int j = 0; j < 7; j++){
		displayOne(100,100,0,0,j);
		vTaskDelay(pdMS_TO_TICKS(550));
	}
	for (int j = 7; j >= 0; j--){
		displayOne(100,100,0,0,j);
		vTaskDelay(pdMS_TO_TICKS(550));
	}
}