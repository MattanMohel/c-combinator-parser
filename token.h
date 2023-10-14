#ifndef TOKEN_H
#define TOKEN_H

#include "array.h"

enum TokenType {
  COMMA,
  COLON,
  SPACE,
};

struct Token {
  enum TokenType type;
  union {
    int      _int;
    float  _float;
    char* _symbol;
  }
}

struct Vec* tokenize (const char* txt);

#endif
