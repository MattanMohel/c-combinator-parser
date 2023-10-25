#ifndef _PARSER_H_ 
#define _PARSER_H_

#include <stdio.h>

struct pc_parser_t;
struct pc_input_t;
struct pc_value_t;
typedef struct pc_parser_t pc_parser_t;
typedef struct pc_input_t pc_input_t;
typedef struct pc_value_t pc_value_t;

int pc_input_eof (pc_input_t *i);

// parser contructors

pc_parser_t* pc_char  (char c);
pc_parser_t* pc_range (char a, char b);
pc_parser_t* pc_some  (pc_parser_t* p);
pc_parser_t* pc_or    (int n, ...);

// parser functions 

int pc_parse_char  (pc_input_t *i, char c);
int pc_parse_range (pc_input_t *i, char a, char b);
int pc_parse_some  (pc_input_t *i, pc_parser_t *ip);
int pc_parse_or    (pc_input_t *i, int n, pc_parser_t **ps);

int pc_parse_match    (pc_input_t* i);
int pc_parse_no_match (pc_input_t* i);
int pc_parse_run (pc_input_t *i, pc_parser_t *p);

pc_input_t *pc_string_input (const char *s);

void test();


int main() {
  test();
  return 0;
}


#endif
