#ifndef HELPERS_H
#define HELPERS_H

bool str_to_uint16(const char *str, uint16_t *res);
void print_table(carac_charger *table, char* table_name, uint8_t size);

void cls();

#endif