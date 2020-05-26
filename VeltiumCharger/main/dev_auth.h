#ifndef dev_auth_h
#define dev_auth_h

// inicializa el mecanismo de autenticación del dispositivo.
// parametro: device_serial: puntero a un buffer de 10 bytes con el número de serie del dispositivo
void dev_auth_init(const void* device_serial);

// calcula el reto de autenticación
// parámetro: puntero a vector de 8 bytes con el valor de entrada del reto, debería
//            ser un numero aleatorio
// valor de retorno: puntero a vector de 8 bytes con el valor resultado del reto.
const void* dev_auth_challenge(const void* challenge_input);

// libera recursos asociados con el mecanismo de autenticacion
void dev_auth_close(void);

#endif /* dev_auth_h */
