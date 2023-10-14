#ifndef _VEC_H_
#define _VEC_H_

//  _____________________
// | Byte | Format       |
// |---------------------|
// |  1   |  cap         |
// |  2   |  len         |
// |  3   |  1st element | <- array points here
// |  n   |  nth element | <- NOTE: nth byte depends on array stride 
//  ---------------------

#define VEC_NEW(T, N) (T*)vec_new(N, sizeof(T))
#define VEC_ADD(V, E) vec_add((void**)&V, (void*)E, sizeof(V[0]))
#define VEC_LEN(V) ((int*)V)[-1]
#define VEC_CAP(V) ((int*)V)[-2]

void* vec_new (int cap, int stride) {
  int* buf = (int*)malloc(cap * stride + 2 * sizeof(int));
  buf[0] = cap;
  buf[1] = 0;
  return (void*)(buf + 2);
}

void vec_add (void** buf, void* elem, int stride) {
  if (VEC_LEN(*buf) + 1 >= VEC_CAP(*buf)) {
    VEC_CAP(*buf) *= 2;
    int* tmp = (int*)realloc(&VEC_CAP(*buf), VEC_CAP(*buf) * stride + 2 * sizeof(int));
    *buf = (void*)(tmp + 2);
  }

  memcpy(*buf + VEC_LEN(*buf) * stride, elem, stride);
  VEC_LEN(*buf)++;
}

#endif
