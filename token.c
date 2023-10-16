#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "token.h"
#include "vec.h"

#define ERROR(MES) printf("ERROR: %s\n", MES)

int is_delim_char (char c) {
  switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\0': 
      return 1;
    default: 
      return 0;
  }
}

int is_op_char (char c) {
  switch (c) {
    case ',':
    case ':':
    case ';':
    case '=':
    case '<':
    case '>':
    case '-':
    case '+': 
      return 1;
    default: 
      return 0; 
  }
}

enum TokenType match_op (const char* op, int* len) {
  *len = 2;
  if (strncmp(op, ">=", *len) == 0) return GREATER_EQ;
  if (strncmp(op, "<=", *len) == 0) return LESS_EQ;
  *len = 1;
  if (strncmp(op, ":", *len) == 0) return COLON;
  if (strncmp(op, ",", *len) == 0) return COMMA;
  if (strncmp(op, "<", *len) == 0) return LESS;
  if (strncmp(op, ">", *len) == 0) return GREATER;
  if (strncmp(op, "-", *len) == 0) return MINUS;
  if (strncmp(op, "+", *len) == 0) return PLUS;

  return NONE;
}

void push_tok (struct Token* toks, enum TokenType type, const char* str, int len) {
  struct Token tok;
  tok.type = type;
  strncpy(tok.str, str, len);
  tok.str[len] = '\0';

  VEC_PUSH(toks, tok);
}

struct Token* tokenize(const char* str) {
  char* lex_buf = (char*)malloc(MAX_TOKEN_SIZE + 1);
  char* lex_ptr = lex_buf;
  lex_buf[MAX_TOKEN_SIZE] = '\0';

  struct Token* toks = VEC_NEW(struct Token, 64);
  
  for (const char* ptr = str; *ptr != '\0'; ptr++) {
    if (is_delim_char(*ptr)) {
      if (lex_ptr - lex_buf > 1) {
        push_tok(toks, SYMBOL, lex_buf, lex_ptr - lex_buf); 
      }

      lex_ptr = lex_buf;
      continue;
    } 
    else if (is_op_char(*ptr)) {
      int op_len;
      enum TokenType op = match_op(ptr, &op_len);
      if (op != NONE) {
        if (lex_ptr - lex_buf > 1) {
          push_tok(toks, SYMBOL, lex_buf, lex_ptr - lex_buf); 
        }
        
        lex_ptr = lex_buf;
        push_tok(toks, op, ptr, op_len);
        
        continue;
      }        
    }

    if (lex_ptr - lex_buf > MAX_TOKEN_SIZE) {
      ERROR("token character limit exceeded!");
    }

    *lex_ptr = *ptr;
    lex_ptr++;
  }

  if (lex_ptr - lex_buf > 0) {
    push_tok(toks, SYMBOL, lex_buf, lex_ptr - lex_buf);
  }

  free(lex_buf);

  return toks;
}
