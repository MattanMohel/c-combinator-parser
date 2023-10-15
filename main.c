#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//#include "array.h"
#include "vec.h"

#define DELIMETERS " "
#define DELIMETER_COUNT 1
#define MAX_TOKEN_SIZE  256

int main(int argc, char** argv) {
  float* vec = VEC_NEW(float, 1);
  VEC_PUSH(vec, 1.1);
  VEC_PUSH(vec, 1.2);
  VEC_PUSH(vec, 1.3);

  for (int i = 0; i < VEC_LEN(vec); i++) {
    printf("element %d is %f\n", i, vec[i]);
  }

  return 0;
}

