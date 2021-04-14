#include "contador.h"

extern carac_Status Status;
extern xParametrosPing ParametrosPing;
extern carac_Coms Coms;
extern carac_Contador   ContadorExt;

Cliente_HTTP CounterClient("192.168.1.1", 1000);

//Contador de Iskra
/*********** Clase Contador ************/

//Buscar el contador
void Contador::find(){
    Serial.println("Iniciando fase busqueda ");
    IP4_ADDR(&ParametrosPing.BaseAdress , ip4_addr1(&Coms.ETH.IP),ip4_addr2(&Coms.ETH.IP),ip4_addr3(&Coms.ETH.IP),ip4_addr4(&Coms.ETH.IP));
    xTaskCreate( BuscarContador_Task, "BuscarContador", 4096*4, NULL, 5, NULL);   
}

void Contador::begin(String Host){
    CounterUrl = "http://";
    CounterUrl+=Host+"/get_command?command=get_measurements";
    Serial.println(CounterUrl);

    CounterClient.set_url(CounterUrl);
    CounterClient.begin();
    Inicializado = true;
}

void Contador::end(){
    CounterClient.end();
}

bool Contador::read(){

    CounterClient.Send_Command(CounterUrl,READ);

    if (!CounterClient.Send_Command(CounterUrl,READ)) {
        Serial.printf("Counter reading error\n");
        return false;
    }

    Measurements.clear();
    deserializeJson(Measurements,CounterClient.ObtenerRespuesta());

    return true;
}
/*********** Convertir los datos del json ************/
void Contador::parse(){
    //Podemos medir lo que nos salga, pero de momento solo queremos intensidades
    String medida;

    medida = Measurements["measurements"]["I1"].as<String>();
    ContadorExt.DomesticCurrentA = medida.toFloat() *100;
    medida = Measurements["measurements"]["I2"].as<String>();
    ContadorExt.DomesticCurrentB = medida.toFloat() *100;
    medida = Measurements["measurements"]["I3"].as<String>();
    ContadorExt.DomesticCurrentC = medida.toFloat() *100;

    
    
    /*medida = Measurements["measurements"]["U1"].as<String>();
    Status.Measures.instant_voltage = medida.toFloat() *100;
    Serial.println(Status.Measures.instant_voltage);
    medida = Measurements["measurements"]["P1"].as<String>();
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