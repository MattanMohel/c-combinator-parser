#ifndef TOKEN_H
#define TOKEN_H

#define MAX_TOKEN_SIZE  32
#define MAX_OP_SIZE     2

enum TokenType {
  NONE = 0,
  MINUS,
  PLUS,
  COMMA,
  COLON,
  SPACE,
  EQUAL,
  LESS,
  LESS_EQ,
  GREATER,
  GREATER_EQ,
  SYMBOL,
};

struct Token {
  enum TokenType type;
  char str[MAX_TOKEN_SIZE + 1];
};

int is_delim_char (char c);
int is_op_char (char c);
void push_tok (struct Token* toks, enum TokenType type, const char* str, int len);

enum TokenType match_op (const char* op, int* len);

struct Token* tokenize (const char* txt);

#endif
