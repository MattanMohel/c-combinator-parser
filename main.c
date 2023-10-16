#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "vec.h"
#include "token.h"

int main(int argc, char** argv) {
  const char* str = "hello there-how are you  doing?\n>>=++";

  struct Token* toks = tokenize(str);
  printf("%s\n", str);  
  for (int i = 0; i < VEC_LEN(toks); i++) {
    printf("tok %d: %s\n", i, toks[i].str);
  }

  return 0;
}

