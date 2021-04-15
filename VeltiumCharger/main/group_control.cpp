#include "group_control.h"
#include "control.h"
#include <string.h>

extern carac_Params Params;
extern carac_Status Status;
extern carac_Firebase_Configuration ConfigFirebase;
extern carac_chargers FaseChargers;
extern carac_group    ChargingGroup;

bool add_to_group(const char* ID, IPAddress IP, carac_chargers* group);
uint8_t check_in_group(const char* ID, carac_chargers* group);
IPAddress get_IP(const char* ID);

static void print_table(carac_chargers table){
    Serial.write(27);   //Print "esc"
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H"); 
    printf("=============== Grupo de cargadores ===================\n");
    printf("      ID     Fase   HPT   I           IP\n");
    for(int i=0; i< table.size;i++){     //comprobar si el cargador ya está almacenado
        printf("   %s    %i    %s   %i        %s\n", table.charger_table[i].name,table.charger_table[i].Fase,table.charger_table[i].HPT,table.charger_table[i].Current, table.charger_table[i].IP.toString().c_str());
    }
    printf("Memoria interna disponible: %i\n", esp_get_free_internal_heap_size());
    printf("Memoria total disponible: %i\n", esp_get_free_heap_size());
    printf("=======================================================\n");
}

//Funcion para procesar los nuevos datos recibidos
void New_Data(char* Data, int Data_size){
    if(memcmp(Data, ConfigFirebase.Device_Id, 8)){                         //comprobar que no son nuestros propios datos
        char fase = Data[8];
        char ID[9];
        ID[8]= '\0';
        memcpy(ID,Data,8); 
        if(Params.Fase == atoi(&fase)){                                    //Comprobar que está en nuestra misma fase
            uint8_t index = check_in_group(ID,&FaseChargers);               //comprobar si el cargador ya está almacenado en nuestro grupo de fase
            if(index < 255){                         
                memcpy(FaseChargers.charger_table[index].HPT,&Data[9],2);      //Si el cargador ya existe, actualizar sus datos
                    
                char current[Data_size-11+1];
                memcpy( current, &Data[11], Data_size-11 );
                current[Data_size-11] = '\0';          
                FaseChargers.charger_table[index].Current = atoi(current);

                print_table(FaseChargers);
                return;
            }

            //Si el cargador no está en la tabla, añadirlo y actualizar los datos
            add_to_group(ID, get_IP(ID),&FaseChargers);

            FaseChargers.charger_table[FaseChargers.size-1].Fase = Params.Fase;
            memcpy(FaseChargers.charger_table[FaseChargers.size-1].HPT,&Data[9],2);
           
            char current[Data_size-11+1];
            memcpy( current, &Data[11], Data_size-11 );
            current[Data_size-11] = '\0';          
            FaseChargers.charger_table[FaseChargers.size-1].Current = atoi(current);

            print_table(FaseChargers);
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