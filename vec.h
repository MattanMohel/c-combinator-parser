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

#define DEBUG_VEC 0

#define VEC_LEN(V) ((int*)V)[-1]
#define VEC_CAP(V) ((int*)V)[-2]
#define VEC_LOC(V) &((int*)V)[-2]

#define VEC_NEW(T, N) (T*)vec_new(N, sizeof(T))
#define VEC_PUSH(V, E) vec_realloc((void**)&V, sizeof(V[0])); *(V + VEC_LEN(V)++) = E

void* vec_new (int cap, int stride) {
  int* buf = (int*)malloc(cap * stride + 2 * sizeof(int));
  buf[0] = cap;
  buf[1] = 0;
  return (void*)(buf + 2);
}

void* vec_realloc(void** buf, int stride) {
  if (VEC_LEN(*buf) + 1 >= VEC_CAP(*buf)) {
#if DEBUG_VEC
    printf("reallocating vec buffer...\n");
#endif
    VEC_CAP(*buf) = 2 * (VEC_CAP(*buf) + 1);
    buf = realloc(VEC_LOC(*buf), VEC_CAP(*buf) * stride + 2 * sizeof(int));
    buf += 2 * sizeof(int);
  }

  return buf;
}

#endif
