/*
 */

#ifndef HardwareSerialMOD_h
#define HardwareSerialMOD_h

#include "HardwareSerial.h"

class HardwareSerialMOD: public HardwareSerial
{
public:
    HardwareSerialMOD(int uart_nr);

    size_t read(uint8_t *buffer, size_t size);
    inline size_t read(char * buffer, size_t size)
    {
        return read((uint8_t*) buffer, size);
    }
};

#endif // HardwareSerialMOD_h
