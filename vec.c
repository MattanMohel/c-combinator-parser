#include "vec.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void* vec_new (int cap, int stride) {
  int* buf = (int*)malloc(cap * stride + 2 * sizeof(int));
  buf[0] = cap;
  buf[1] = 0;
  return (void*)(buf + 2);
}

void* vec_fit (void** buf, int stride) {
  if (VEC_LEN(*buf) >= VEC_CAP(*buf)) {
    vec_realloc(buf, stride, 2 * VEC_CAP(*buf));
  }
  return buf;
}

void *vec_realloc (void** buf, int stride, int len) {
  VEC_CAP(*buf) = len;
  int *ptr = realloc(VEC_LOC(*buf), stride * VEC_CAP(*buf) + 2 * sizeof(int));
  ptr += 2;
  return ptr;
}

char *str_new (char *src) {
  int len = strlen(src);
  char *dest = (char*)vec_new(len + 1, 1);
  memset(dest, '\0', len);
  return strcat(dest, src);
}

char *str_cpy (char *dest, const char *src) {
  int len = strlen(src);
  if (VEC_CAP(dest) < len) {
    dest = vec_realloc((void**)&dest, 1, len + 1);
  }
  return strcpy(dest, src);
}

char *str_cat (char *dest, const char *src) {
  int len = strlen(src);
  if (VEC_CAP(dest) < len + VEC_LEN(dest)) {
    dest = vec_realloc((void**)&dest, 1, len + VEC_LEN(dest) + 1);
  }
  return strcat(dest, src);
}
