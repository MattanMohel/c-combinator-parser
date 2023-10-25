#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "parser.h"

// parser types

typedef enum {
  PC_TYPE_CHAR  = 0,
  PC_TYPE_RANGE = 1,
  PC_TYPE_SOME  = 2,
  PC_TYPE_OR    = 3,
} pc_type_t;

typedef struct pc_input_t {
  char *str;
  int len;
  int col;
  int row;
  int pos;  
} pc_input_t;

int pc_input_eof (pc_input_t *i) {
  return i->pos == i->len || i->str[i->pos] == '\0';
}

int pc_parse_match (pc_input_t *i) {
  i->col++;
  i->pos++;

  if (i->str[i->pos] == '\n') {
    i->col = 0;
    i->row++;
    i->pos++;
  }

  return 1;
}

int pc_parse_no_match (pc_input_t *i) {
  return 0;
}

typedef struct { char c; } pc_data_char_t;
typedef struct { char a; char b; } pc_data_range_t;
typedef struct { pc_parser_t *p; } pc_data_some_t;
typedef struct { pc_parser_t **ps; int n; } pc_data_or_t;

typedef union {
  pc_data_char_t  single;
  pc_data_range_t range;
  pc_data_some_t parser; 
  pc_data_or_t or;
} pc_data_t;

typedef struct pc_value_t {
} pc_value_t;

typedef struct pc_parser_t {
  pc_type_t type;
  pc_data_t data;
} pc_parser_t;

pc_parser_t *pc_char (char c) {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->type = PC_TYPE_CHAR;
  p->data.single.c = c;

  return p;
}

pc_parser_t *pc_range (char a, char b) {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->type = PC_TYPE_RANGE;
  p->data.range.a = a;
  p->data.range.b = b;

  return p;
}

pc_parser_t *pc_some (pc_parser_t *ip) {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->type = PC_TYPE_SOME;
  p->data.parser.p = ip;

  return p;
}

pc_parser_t *pc_or (int n, ...) {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->type = PC_TYPE_OR;
  p->data.or.n = n;
  p->data.or.ps = (pc_parser_t**)malloc(n * sizeof(pc_parser_t*));

  va_list ps;
  va_start(ps, n);

  for (int i = 0; i < n; i++) {
    p->data.or.ps[i] = va_arg(ps, pc_parser_t*);
  }

  va_end(ps);
  return p;
}

int pc_parse_char (pc_input_t *i, char c) {
  if (pc_input_eof(i) || i->str[i->pos] != c) {
    return pc_parse_no_match(i);
  } else {
    return pc_parse_match(i);
  }
}

int pc_parse_range (pc_input_t *i, char a, char b) {
  char c = i->str[i->pos];
  if (pc_input_eof(i) || c < a || c > b) {
    return 0;
  } else {
    return pc_parse_match(i);
  }
}

int pc_parse_some (pc_input_t *i, pc_parser_t *p) {
  while (pc_parse_run(i, p));
  return 1;
}

int pc_parse_or (pc_input_t *i, int n, pc_parser_t **ps) {
  for (int j = 0; j < n; j++) {
    pc_parser_t *p = ps[j];   
    if (pc_parse_run(i, p)) {
      return 1;
    }
  }
  return 0;
}

int pc_parse_run (pc_input_t *i, pc_parser_t *p) {
  switch (p->type) {
    case PC_TYPE_CHAR:  return pc_parse_char(i, p->data.single.c);
    case PC_TYPE_RANGE: return pc_parse_range(i, p->data.range.a, p->data.range.b);
    case PC_TYPE_SOME:  return pc_parse_some(i, p->data.parser.p);
    case PC_TYPE_OR:    return pc_parse_or(i, p->data.or.n, p->data.or.ps);
  }

  return 1;  
}

pc_input_t *pc_string_input (const char *s) {
  pc_input_t *i = (pc_input_t*)malloc(sizeof(pc_input_t));
  i->len = strlen(s);
  i->col = 0;
  i->row = 0;
  i->pos = 0;
  i->str = (char*)malloc(i->len + 1);

  strcpy(i->str, s);
  
  return i;
}

void test() {
  const char* test = "abc123";
  
  pc_input_t *i = pc_string_input(test);

  pc_parser_t *letter  = pc_range('a', 'z');
  pc_parser_t *numeric = pc_range('0', '9');
  pc_parser_t *num_or_let = pc_or(2, numeric, letter);
  pc_parser_t *word   = pc_some(num_or_let);

    
  pc_parse_run(i, word);
  
  printf("source: %s, parsed to index %d\n", i->str, i->pos);
}

