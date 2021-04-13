#include "group_control.h"
#include "control.h"
#include <string.h>

extern carac_Params Params;
extern carac_Status Status;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_chargers FaseChargers;

static void print_table(){
    Serial.write(27);   //Print "esc"
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H"); 
    printf("=============== Grupo de cargadores ===================\n");
    printf("      ID     Fase   HPT   V\n");
    for(int i=0; i< FaseChargers.size;i++){     //comprobar si el cargador ya está almacenado
        printf("   %s    %i    %s   %i  \n", FaseChargers.charger_table[i].name,FaseChargers.charger_table[i].Fase,FaseChargers.charger_table[i].HPT,FaseChargers.charger_table[i].Voltage);
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

                    print_table();
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
            print_table();
        }
    }
}