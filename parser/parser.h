#ifndef _PARSER_H_ 
#define _PARSER_H_

#include <stdio.h>

typedef struct pc_result_t pc_result_t;
typedef struct pc_parser_t pc_parser_t;
typedef struct pc_input_t  pc_input_t;
typedef struct pc_result_t pc_result_t;
typedef void pc_value_t;

typedef pc_value_t*(*pc_fold_t)(int, pc_result_t*);
typedef pc_value_t*(*pc_apply_t)(pc_result_t*);

pc_input_t *pc_string_input (const char *s);
int pc_input_eof (pc_input_t *i);

int pc_input_push_mark (pc_input_t *i);
int pc_input_pop_mark  (pc_input_t *i);
int pc_input_peek_mark (pc_input_t *i);

// parser contructors

pc_parser_t *pc_uninit ();
pc_parser_t *pc_rule (const char* name);

pc_parser_t *pc_char   (char c);
pc_parser_t *pc_range  (char a, char b);
pc_parser_t *pc_some   (pc_fold_t f, pc_parser_t *c);
pc_parser_t *pc_any    (int n, ...);
pc_parser_t *pc_and    (pc_fold_t f, int n, ...);
pc_parser_t *pc_apply  (pc_apply_t a, pc_parser_t *c);

pc_value_t *pc_fold_concat (int n, pc_result_t *r);
pc_value_t *pc_fold_str (int n, pc_result_t *r);
pc_value_t *pc_fold_nat (int n, pc_result_t *r);
pc_value_t *pc_apply_binop (pc_result_t *r);
pc_value_t *pc_apply_identity (pc_result_t *r);

// parser functions 

int pc_parse_char  (pc_input_t *i, pc_result_t *r, char c);
int pc_parse_range (pc_input_t *i, pc_result_t *r, char a, char b);

int pc_parse_match    (pc_input_t *i, pc_result_t *r, char c);
int pc_parse_no_match (pc_input_t *i);
int pc_parse_run (pc_input_t *i, pc_result_t *r, pc_parser_t *p, int depth);


void unordered_arithmetic();
void pemdas();

int main() {
  pemdas();
  return 0;
}


#endif
