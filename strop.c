#include "strop.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

char *pc_strcpy (const char *str) {
  char *buf = (char*)malloc(strlen(str) + 1);
  return strcpy(buf, str);
}

char *pc_format (const char *str, va_list fmt) {
  va_list cpy;
  va_copy(cpy, fmt);
  size_t size = vsnprintf(NULL, 0, str, cpy);
  va_end(cpy);

  char *buf = malloc(size+1);
  vsnprintf(buf, size+1, str, fmt);
  va_end(fmt);

  return buf;
}

char *pc_formatf (const char *str, ...) {
  va_list fmt;
  va_start(fmt, str);

  return pc_format(str, fmt);
}

