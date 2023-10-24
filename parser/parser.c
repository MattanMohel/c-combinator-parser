#include "parser.h"

// parser types

enum {
  PC_TYPE_CHAR  = 0,
  PC_TYPE_RANGE = 1,
  PC_TYPE_SOME  = 2,
};

typedef struct {
  const char* buf;
  int len;
  int col;
  int row;
  int pos;  
} pc_input_t;

int pc_input_eof (pc_input_t* i) {
  return i->pos == i->len || i->buf[i->pos] == '\0';
}

int pc_parse_match (pc_input_t* i) {
  i->col++;
  i->pos++;

  if (i->buf[i->pos] == '\n') {
    i->col = 0;
    i->row++;
    i->pos++;
  }

  return 1;
}

int pc_parse_no_match (pc_input_t* i) {
  return 0;
}

typedef struct { char c; } pc_data_char_t;
typedef struct { char a; char b; } pc_data_range_t;
typedef struct { pc_parser_t* p; } pc_data_parser_t;

typedef union {
  pc_data_char_t*   single;
  pc_data_range_t*   range;
  pc_data_parser_t* parser; 
} pc_data_t;

typedef struct {
  pc_type_t type;
  pc_data_t data;
}

int pc_parse_char (pc_input_t* i, char c) {
  if (pc_input_eof(i) || i->buf[i->pos] != c) {
    return pc_parse_no_match(i);
  } else {
    return pc_parse_match(i);
  }
}

int pc_parse_range (pc_input_t* i, char a, char b) {
  char c = i->buf[i->pos];
  if (pc_input_eof(i) || c < a || c > b) {
    return pc_parse_no_match(i);
  } else {
    return pc_parse_match(i);
  }
}

int pc_parse_some (pc_input_t* i, pc_parser_t* p) {
  while (pc_parse_run(i, p)) {}
  return 1;
}

int pc_parse_run (pc_input_t* i, pc_parser_t* p) {
  switch (p->type) {
    case PC_TYPE_CHAR:
      pc_parse_char(i, p->data.single->c);
      break;
    case PC_TYPE_RANGE:
      pc_parse_char(i, p->data.range->a, p->data.range->b);
      break;
    case PC_TYPE_SOME:
      pc_parse_some(i, p->data.parser->p);
      break;
  }

  return 1;  
} 
