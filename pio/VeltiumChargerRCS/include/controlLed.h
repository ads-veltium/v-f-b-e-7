#ifndef __CONTROLLED_H
#define __CONTROLLED_H


void initLeds ( void );
void displayAll( uint8_t i, uint8_t r, uint8_t g, uint8_t b);
void displayOne( uint8_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t pixeln) ;
void changeOne( uint8_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t pixeln);
void Kit (void);

#endif // __CONTROLLED_H
