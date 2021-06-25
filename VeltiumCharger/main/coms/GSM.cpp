#include "../control.h"


#define SerialAT Serial1


String SendAt(String command){
	String inchar;

	Serial.printf("Enviando commando: ");
	
	uint8 count = 0;
	while(++count < 100){
		SerialAT.println(command);
		delay(500);
		if (SerialAT.available()){
			inchar = SerialAT.readString();
			if(inchar.indexOf("OK")>0){
				Serial.println(inchar);
				break;
			}
			else if(inchar.indexOf("ERROR")>0){
				Serial.println(inchar);
				break;
			}
			else if(inchar.indexOf(">")>0){
				Serial.println(inchar);
				break;
			}
		}
	}


	return inchar;
}



void StartGSM(){

    delay(10000);
	digitalWrite(GPIO_MODEM_PWR_EN, HIGH);


	SerialAT.begin(115200, SERIAL_8N1, GPIO_UART1_RX_MODEM, GPIO_UART1_TX_MODEM);

	delay(100);
	
	bool estado_red = 0;

	//comprobar configuracion
	SendAt("AT#REBOOT");

	delay(300);

    //Reinicar configuracion
	SendAt("AT+CFUN=4");
	SendAt("AT+CFUN=1");

	String CIMI = SendAt("AT+CIMI").substring(11, 16);
	
	SendAt("AT+COPS=4,2,"+CIMI+",0");
	SendAt("AT+COPS?");
	

    //Registrarnos en la red
	while(!estado_red){
		String response = SendAt("AT+CREG?");	
		if(strlen(response.c_str()) > 10){
			if(response.indexOf("+CREG: 0,1")>0){
				estado_red = true;
			}
		}
		
	}	

    //Conectarse al apn y obtener ip
    Serial.println("registrado en la puta red!!!!!");	
	SendAt("AT+CGDCONT=1,\"IP\",\"telefonica.es\"");
	SendAt("AT#SGACT=1,1");
	SendAt("AT+CGCONTRDP");
	SendAt("AT#SD=1,0,80,\"www.telit.com\"");

	Serial.println("Fin del script!!!!!");

}