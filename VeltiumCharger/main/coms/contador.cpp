#include "contador.h"
#ifdef CONNECTED

extern carac_Status Status;
extern carac_Coms Coms;
extern carac_Contador   ContadorExt;

Cliente_HTTP CounterClient("192.168.1.1", 1000);


//Contador de Iskra
/*********** Clase Contador ************/

//Buscar el contador
void Contador::find(){
    #ifdef DEBUG_MEDIDOR
    Serial.println("Iniciando fase busqueda ");
    #endif
    xTaskCreate( BuscarContador_Task, "BuscarContador", 4096*4, NULL, 5, NULL);   
}

void Contador::begin(String Host){
    CounterUrl = "http://";
    CounterUrl+=Host+"/get_command?command=get_measurements";

    CounterClient.set_url(CounterUrl);
    CounterClient.begin();
    Inicializado = true;
}

void Contador::end(){
    CounterClient.end();
    Inicializado = false;
}

bool Contador::read(){

    if (!CounterClient.Send_Command(CounterUrl,LEER)) {
        #ifdef DEVELOPMENT
        Serial.printf("Counter reading error\n");
        #endif
        ContadorExt.ConexionPerdida = true;
        Update_Status_Coms(MED_CONECTION_LOST);
        return false;
    }
    Measurements.clear();
    deserializeJson(Measurements,CounterClient.ObtenerRespuesta());

    return true;
}
/*********** Convertir los datos del json ************/
void Contador::parse(){
    //Podemos medir lo que nos salga, pero de momento solo queremos intensidades
    String medida , time;
    static String last_time = " ";
    static uint8_t old = 0, Same_time_count = 0;


    //comprobar que no se ha desconectado el medidor

    time = Measurements["header"]["local_time"].as<String>();
    if(time == last_time){
        if(++Same_time_count > 5){
            ContadorExt.ConexionPerdida = 1;
            Update_Status_Coms(MED_CONECTION_LOST);
            return;
        }
    }
    else{
        ContadorExt.ConexionPerdida = 0;
        Same_time_count = 0;
        Update_Status_Coms(MED_LEYENDO_MEDIDOR);
    }
    last_time = time;
    
    //Leer corrientes
    /*medida = Measurements["measurements"]["I1"].as<String>();
    ContadorExt.DomesticCurrentA = medida.toFloat() *100;
    medida = Measurements["measurements"]["I2"].as<String>();
    ContadorExt.DomesticCurrentB = medida.toFloat() *100;
    medida = Measurements["measurements"]["I3"].as<String>();
    ContadorExt.DomesticCurrentC = medida.toFloat() *100;*/

    //Leer potencias 
    medida = Measurements["measurements"]["P0"].as<String>();
    Serial.println(medida);

    ContadorExt.MeidorConectado = medida != "null";
    if(ContadorExt.MeidorConectado != old){
        if(old){ //Si ya estaba leyendo y perdemos comunicacion,
            ContadorExt.ConexionPerdida = 1;
            Update_Status_Coms(MED_CONECTION_LOST);
        }
        else{
            ContadorExt.ConexionPerdida = 0;
        }
        old = ContadorExt.MeidorConectado;
        Update_Status_Coms(MED_LEYENDO_MEDIDOR);
    }

    if(ContadorExt.ConexionPerdida){
        ContadorExt.DomesticPower = 0;
    }

    uint16_t medida_dom = 0;
    if(medida.indexOf("k W")>0){
        medida_dom = medida.toFloat()*1000;
    }
    else{
        medida_dom= medida.toFloat();
        if(medida_dom > 60000){
            medida_dom = 0;
        }
    }
    ContadorExt.DomesticPower = medida_dom;

    #ifdef DEBUG_MEDIDOR
    Serial.println(ContadorExt.DomesticPower);
    #endif


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

#endif