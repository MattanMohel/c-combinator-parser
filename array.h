#ifndef VEC_H
#define VEC_H

#include <stdlib.h>

typedef struct Vec {
  void* buf;
  int  size;
  int  used;
  int  stride;
};

#define VEC(T, SIZE) vec_create(sizeof(T), SIZE)

struct Vec* vec_create (int stride, int size) {
  struct Vec* vec = (struct Vec*)malloc(sizeof(struct Vec));
  vec->buf = malloc(size * stride);
  vec->stride = stride;
  vec->size = size;

  return vec;
}

void vec_push (struct Vec* vec, void* elem) {
  if (vec->used == vec->size) {
    vec->size *= 2;
    vec->buf = realloc(vec->buf, vec->size);
  }

  memcpy(vec->buf + vec->used * vec->stride, elem, vec->stride);
  vec->used++;
}

void* vec_get (struct Vec* vec, int i) {
  return vec->buf + i * vec->stride;
}

#endif
