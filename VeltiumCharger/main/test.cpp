#include "test.h"

#include "libb64/cdecode.h"

uint8_t CreateMatrix(uint16_t* inBuff, uint8_t* outBufff){
    uint8_t matrix[7][24] = {0};

    memcpy(matrix, outBufff,168);

    //Si la programacion no está activa, volvemos
    if(inBuff[0]!= 1) return 1;

    //Obtenemos la potencia programada para estas horas
    uint8_t power = (inBuff[4]*0x100 + inBuff[5])/100;

    //Obtenemos las horas de inicio y final
    uint8_t init = inBuff[2];
    uint8_t fin  = inBuff[3];

    //Obtener los dias de la semana
    uint8_t dias = inBuff[1];

    //Iterar por los dias de la semana
    for(uint8_t dia=0; dia < 7;dia++){

        //Si el dia está habilitado
        if ((dias>>dia)&1){
            //Iteramos por las horas activas
            for(uint8_t hora = init;hora <= fin;hora++){
                matrix[dia][hora]=power;
            }
        }
    }

    memcpy(outBufff,matrix,168);

    return 0;
}


uint8_t hex2int(char ch){
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}

uint16_t hexChar_To_uint16_t(const uint8_t num){
    char hexCar[3];
    sprintf(hexCar, "%02X", num);
    uint16_t valor = hex2int(hexCar[0]) *0x10 + hex2int(hexCar[1]);
    return valor;
}

void Hex_Array_To_Uint16A_Array(const char* inBuff, uint16_t* outBuff) {
    for(int i=0; i<strlen(inBuff); i++){
        outBuff[i] = hexChar_To_uint16_t((uint8_t)inBuff[i]);
    }
}

void printHex(const uint8_t num) {
  char hexCar[3];

  sprintf(hexCar, "%02X", num);
  Serial.println(hexCar);
  Serial.println();
}

void printHexBuffer(const char* num) {
  for(int i=0; i<strlen(num); i++){
    printHex((uint8_t)num[i]);
  }
  Serial.println();
}

void prueba(){
    String programaciones [4]{"AH8FBwkf", "AHcDCwkf", "AQMDCwyA", "ATAFCwkf"};

    uint8_t matrix[7][24] = {0};
    uint8_t plainMatrix[168]={0};

    //Decodificar las programaciones
    for(String x : programaciones){
        char dst[6] = {0};
        uint8_t decoded = base64_decode_chars(x.c_str(),8,dst);

        //Menos de 6 bits como resultado = error
        if(decoded != 6){
            continue;
        }
        uint16_t decodedBuf[6] = {0};
        
        Hex_Array_To_Uint16A_Array(dst, decodedBuf);

        //Si nos devuelve algo es que la matriz no es valida
        if(CreateMatrix(decodedBuf, plainMatrix) == 0){
            memcpy(matrix,plainMatrix,168);
        }
    }
    

    for(uint8_t dia=0;dia<7;dia++){
        for(uint8_t hora=0;hora<24;hora++){
            printf("%i ", matrix[dia][hora]);
        }
        printf("\n");
    }
}
