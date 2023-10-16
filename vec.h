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

#define VEC_LEN(V) ((int*)V)[-1]
#define VEC_CAP(V) ((int*)V)[-2]
#define VEC_LOC(V) &((int*)V)[-2]
#define VEC_NEW(T, N)  (T*)vec_new(N, sizeof(T))
#define VEC_PUSH(V, E) vec_realloc((void**)&V, sizeof(V[0])); *(V + VEC_LEN(V)++) = E

void* vec_new (int cap, int stride);
void* vec_realloc(void** buf, int stride);

#endif
