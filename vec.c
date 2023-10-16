#include <stdlib.h>
#include "vec.h"

void* vec_new (int cap, int stride) {
  int* buf = (int*)malloc(cap * stride + 2 * sizeof(int));
  buf[0] = cap;
  buf[1] = 0;
  return (void*)(buf + 2);
}

void* vec_realloc(void** buf, int stride) {
  if (VEC_LEN(*buf) + 1 >= VEC_CAP(*buf)) {
    VEC_CAP(*buf) = 2 * (VEC_CAP(*buf) + 1);
    buf = realloc(VEC_LOC(*buf), VEC_CAP(*buf) * stride + 2 * sizeof(int));
    buf += 2 * sizeof(int);
  }

  return buf;
}

