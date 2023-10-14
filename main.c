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
  int* arr = VEC_NEW(int, 1);
  int i = 99;
  VEC_ADD(arr, &i); 
  VEC_ADD(arr, &i);

  printf("%d, %d, %d\n", arr[0], VEC_LEN(arr), VEC_CAP(arr));

  return 0;
}

enum TokenType {
  NUMBER,
  COMMA,
  COLON,
  SPACE,
};

struct Token {
  enum TokenType type;
  const char* src;
};
  
void tokenize (const char* src) {
 // struct Vec* toks = VEC(struct Token, 256);
  char* lex_buf = (char*)malloc(MAX_TOKEN_SIZE);
  int lex_ptr = 0; 

  memset(lex_buf, 0, MAX_TOKEN_SIZE);
  
  int len = strlen(src);
  for (int i = 0; i < len; i++) {
    bool is_delimeter = false;
    for (int j = 0; j < DELIMETER_COUNT; j++) {
      if (src[i] == DELIMETERS[j]) {
        is_delimeter = true;
        break;
      }
    }

    if (is_delimeter) {
      struct Token tok;
      switch (src[i]) {
        case ' ':
          tok.type = SPACE;
          break;
        case ',':
          tok.type = COMMA;
          break;
        case ':':
          tok.type = COLON;
          break;
      }

     // vec_push(toks, &tok);

      printf("%s", lex_buf);
      memset(lex_buf, 0, MAX_TOKEN_SIZE);
      lex_ptr = 0;
    } else {
      lex_buf[lex_ptr] = src[i];
      lex_ptr++;
    }
  }

  free(lex_buf);
} 

