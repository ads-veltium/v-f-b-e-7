#include "control.h"
#include "helpers.h"

bool str_to_uint16(const char *str, uint16_t *res){
  char *end;
  intmax_t val = strtoimax(str, &end, 10);
  if ( val < 0 || val > UINT16_MAX || end == str || *end != '\0')
    return false;
  *res = (uint16_t) val;
  return true;
}

void cls(){
#ifdef DEBUG
    Serial.write(27);   
    Serial.print("[2J");
    Serial.write(27);
    Serial.print("[H"); 
#endif
}

void print_table(carac_charger *table, char* table_name = "Grupo de cargadores", uint8_t size = 0){
#ifdef DEBUG_GROUPS
    printf("=============== %s ===================\n", table_name);
    printf("      ID     Fase   HPT   I      CONS   D    DT     CONEX\n");
    for(int i=0; i< size;i++){     //comprobar si el cargador ya está almacenado
        printf("   %s    %i    %s  %i   %i   %i   %i    %i\n", table[i].name,table[i].Fase,table[i].HPT,table[i].Current, table[i].Consigna, table[i].Delta,  table[i].Delta_timer, table[i].Conected);
    }
    printf("Memoria interna disponible: %i\n", esp_get_free_internal_heap_size());
    printf("Memoria total disponible:   %i\n", esp_get_free_heap_size());
    printf("=======================================================\n");
#endif
}
