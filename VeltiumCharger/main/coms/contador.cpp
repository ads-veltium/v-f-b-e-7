#include "contador.h"

extern carac_Status Status;
extern xParametrosPing ParametrosPing;

//Contador de Iskra
/*********** Clase Contador ************/

//Buscar el contador
void Contador::find(){

    IP4_ADDR(&ParametrosPing.BaseAdress , 192,168,20,100);
    ParametrosPing.max = 220;
    xTaskCreate( BuscarContador_Task, "BuscarContador", 4096*4, NULL, 5, NULL);

}
void Contador::begin(String Host){
    CounterUrl = "http://";
    CounterUrl+=Host+"/get_command?command=get_measurements";
    Serial.println(CounterUrl);

    CounterClient.begin(CounterUrl);
    CounterClient.addHeader("Content-Type", "application/json"); 
    CounterClient.setTimeout(10000);
    CounterClient.setConnectTimeout(2000);
    CounterClient.setReuse(true);
    Inicializado = true;
}

void Contador::end(){
    CounterClient.end();
}

bool Contador::read(){
    int response;

    response = CounterClient.GET();

    if (response < 0) {
        Serial.println(response);
        Serial.printf("Counter reading error: %s\n", 
        CounterClient.errorToString(response).c_str());
        return false;
    }

    String payload = CounterClient.getString();
    Measurements.clear();
    deserializeJson(Measurements,payload);

    //conection refused
    if(response == -1){
        Serial.println("Counter conection refused");
        return false;
    }
    return true;
}
bool Contador::parseModel(){
    if(!strcmp(Measurements["header"]["model"].as<String>().c_str(), "IE38MD")){
        Serial.println("IE38MD Encontrado!");
        return true;
    }
    return false;
}
/*********** Convertir los datos del json ************/
void Contador::parse(){
    //Podemos medir lo que nos salga, pero de momento solo queremos intensidades
    String medida;

    medida = Measurements["measurements"]["I1"].as<String>();
    Status.Measures.instant_current = medida.toFloat() *100;
    medida = Measurements["measurements"]["I2"].as<String>();
    Status.MeasuresB.instant_current = medida.toFloat() *100;
    medida = Measurements["measurements"]["I3"].as<String>();
    Status.MeasuresC.instant_current = medida.toFloat() *100;

    
    
    medida = Measurements["measurements"]["U1"].as<String>();
    Status.Measures.instant_voltage = medida.toFloat() *100;
    Serial.println(Status.Measures.instant_voltage);
    /*medida = Measurements["measurements"]["P1"].as<String>();
    Status.Measures.active_power = medida.toFloat() *100;

    medida = Measurements["measurements"]["U2"].as<String>();
    Status.MeasuresB.instant_voltage = medida.toFloat() *100;
    medida = Measurements["measurements"]["P2"].as<String>();
    Status.MeasuresB.active_power = medida.toFloat() *100;

    medida = Measurements["measurements"]["U3"].as<String>();
    Status.MeasuresC.instant_voltage = medida.toFloat() *100;
    medida = Measurements["measurements"]["P3"].as<String>();
    Status.MeasuresC.active_power = medida.toFloat() *100;*/

}

//Contador de Carlo Gavazzi