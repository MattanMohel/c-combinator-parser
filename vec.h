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
#define VEC_LEN(V)  ((int*)V)[-1]
#define VEC_CAP(V)  ((int*)V)[-2]
#define VEC_LOC(V) &((int*)V)[-2]
#define VEC_FST(V) V[0]
#define VEC_LST(V) V[VEC_LEN(V)-1]

#define VEC_PUSH(V, E) vec_fit((void**)&V, sizeof(V[0])); *(V + VEC_LEN(V)++) = E
#define VEC_FREE(V) free(VEC_LOC(V))

void *vec_new     (int cap, int stride);
void *vec_fit     (void** buf, int stride);
void *vec_realloc (void** buf, int stride, int len);

#endif
