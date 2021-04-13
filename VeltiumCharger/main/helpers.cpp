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