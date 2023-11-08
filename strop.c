#include "strop.h"
#include <string.h>
#include <stdlib.h>

char *pc_strcpy (const char *str) {
  char *buf = (char*)malloc(strlen(str) + 1);
  return strcpy(buf, str);
}
