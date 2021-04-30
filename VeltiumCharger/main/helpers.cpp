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

void print_table(carac_chargers table){
#ifdef DEBUG_GROUPS
    printf("=============== Grupo de cargadores ===================\n");
    printf("      ID     Fase   HPT   I          IP         D\n");
    for(int i=0; i< table.size;i++){     //comprobar si el cargador ya está almacenado
        printf("   %s    %i    %s  %i   %s   %i\n", table.charger_table[i].name,table.charger_table[i].Fase,table.charger_table[i].HPT,table.charger_table[i].Current, table.charger_table[i].IP.toString().c_str(), table.charger_table[i].Delta);
    }
    printf("Memoria interna disponible: %i\n", esp_get_free_internal_heap_size());
    printf("Memoria total disponible:   %i\n", esp_get_free_heap_size());
    printf("=======================================================\n");
#endif
}