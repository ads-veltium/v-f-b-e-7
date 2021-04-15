#include "group_control.h"
#include "control.h"
#include <string.h>

extern carac_Params Params;
extern carac_Status Status;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_chargers FaseChargers;
extern carac_group    ChargingGroup;

bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);
bool check_in_group(const char* ID, carac_chargers* group);
IPAddress get_IP(const char* ID);

static void print_table(carac_chargers table){
    Serial.write(27);   //Print "esc"
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H"); 
    printf("=============== Grupo de cargadores ===================\n");
    printf("      ID     Fase   HPT   V     IP\n");
    for(int i=0; i< table.size;i++){     //comprobar si el cargador ya está almacenado
        printf("   %s    %i    %s   %i  %s\n", table.charger_table[i].name,table.charger_table[i].Fase,table.charger_table[i].HPT,table.charger_table[i].Voltage, table.charger_table[i].IP.toString().c_str());
    }
    printf("Memoria interna disponible: %i\n", esp_get_free_internal_heap_size());
    printf("Memoria total disponible: %i\n", esp_get_free_heap_size());
    printf("=======================================================\n");
}

//Funcion para procesar los nuevos datos recibidos
void New_Data(char* Data, int Data_size){
    if(memcmp(Data, ConfigFirebase.Device_Id, 8)!=0){                    //comprobar que no son nuestros propios datos
        char fase = Data[8];
        if(Params.Fase == atoi(&fase)){                                  //Comprobar que está en nuestra misma fase
            for(int i=0; i< FaseChargers.size;i++){                      //comprobar si el cargador ya está almacenado en nuestro grupo de fase
                if(!memcmp(Data, FaseChargers.charger_table[i].name,8)){ //Si el cargador ya existe, actualizar sus datos
                    memcpy(FaseChargers.charger_table[i].HPT,&Data[9],2);
                    
                    char current[Data_size-11+1];
                    memcpy( current, &Data[11], Data_size-11 );
                    current[Data_size-11] = '\0';          
                    FaseChargers.charger_table[i].Voltage = atoi(current);

                    //print_table();
                    return;
                }
            }

            //Si el cargador no está en la tabla, añadirlo
            memcpy(FaseChargers.charger_table[FaseChargers.size].name, Data,8);
            FaseChargers.charger_table[FaseChargers.size].name[8]='\0';
            FaseChargers.charger_table[FaseChargers.size].Fase = Params.Fase;
            memcpy(FaseChargers.charger_table[FaseChargers.size].HPT,&Data[9],2);
           
            char current[Data_size-11+1];
            memcpy( current, &Data[11], Data_size-11 );
            current[Data_size-11] = '\0';          
            FaseChargers.charger_table[FaseChargers.size].Voltage = atoi(current);

            FaseChargers.size++;
            //print_table();
        }
    }
}

void New_Params(char* Data, int Data_size){
    uint8_t numero_de_cargadores = (Data_size)/8;
    ChargingGroup.group_chargers.size = 0;

    //Copiar el nuevo grupo a nuestra tabla
    for(uint8_t i=0;i<numero_de_cargadores;i++){
        char ID[8];
        memcpy(ID, &Data[i*8],8);
        add_to_group(ID,get_IP(ID),&ChargingGroup.group_chargers);
        printf("Añadido %s %s\n",ID, get_IP(ID).toString().c_str());
    }    
    print_table(ChargingGroup.group_chargers);
}