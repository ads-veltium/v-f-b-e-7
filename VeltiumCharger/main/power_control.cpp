#include "control.h"
#include "power_control.h"
#include "time.h"

/******** Globals********/
extern carac_Status Status;
extern Config Configuracion;


void powerControl(){

    uint8 weekday;
    
    weekday = ((Status.Time.actual_time/86400) + 4) % 7; 

	if(weekday==6 || weekday==7){
		Configuracion.data.potencia_contratada1 = Configuracion.data.potencia_contratada1;
	}else if(Status.Time.hora>= 22 || Status.Time.hora < 6){
		Configuracion.data.potencia_contratada1 = Configuracion.data.potencia_contratada1;
	}else if(Status.Time.hora >= 6 && Status.Time.hora < 22){
		Configuracion.data.potencia_contratada1 = Configuracion.data.potencia_contratada2;
	}

    /*if(calendar[0] == date_time_actual[2]){
		inicio=1;
		dias=10;
	}else{
		inicio=11;
		dias=20;
	}
	if(hoy!=date_time_actual[0]){
		for(i=inicio;i<dias; i=i+2){
			if(calendar[i] == date_time_actual[1] && calendar[i+1] == date_time_actual[0]){
				intensidad_contratada = intensidad_contratada_1;
			}
		}
		hoy=date_time_actual[0];
	}
    */
}