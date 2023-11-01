#ifndef _PARSER_H_ 
#define _PARSER_H_

#include <stdio.h>

// TODO: reference counted parser/void* allocations

typedef struct pc_result_t pc_result_t;
typedef struct pc_parser_t pc_parser_t;
typedef struct pc_input_t  pc_input_t;
typedef struct pc_result_t pc_result_t;
typedef enum pc_error_type_t pc_error_type_t;
typedef void pc_value_t;

typedef pc_value_t*(*pc_fold_t)(int, pc_result_t*);
typedef pc_value_t*(*pc_apply_t)(pc_result_t*);

pc_input_t *pc_string_input (const char *s);
int pc_input_eof (pc_input_t *i);

int pc_input_mark (pc_input_t *i);
int pc_input_unmark (pc_input_t *i);
int pc_input_rewind (pc_input_t *i);


pc_parser_t *pc_alpha ();
pc_parser_t *pc_word  ();
pc_parser_t *pc_digit ();
pc_parser_t *pc_ident ();

// parser contructors

pc_parser_t *pc_uninit ();
pc_parser_t *pc_rule (const char* name);
int pc_define (pc_parser_t *rule, pc_parser_t *p);

pc_parser_t *pc_char     (char c);
pc_parser_t *pc_range    (char a, char b);
pc_parser_t *pc_oneof    (const char *s);
pc_parser_t *pc_noneof   (const char *s);
pc_parser_t *pc_pass     ();
pc_parser_t *pc_some     (pc_fold_t f, pc_parser_t *c);
pc_parser_t *pc_least    (int n, pc_fold_t f, pc_parser_t *c);
pc_parser_t *pc_any      (int n, ...);
pc_parser_t *pc_and      (pc_fold_t f, int n, ...);
pc_parser_t *pc_apply    (pc_apply_t a, pc_parser_t *c);
pc_parser_t *pc_insert   (const char *s);
pc_parser_t *pc_inspect  (pc_error_type_t state, const char *s);

void pc_delete_parsers (int n, ...);
void pc_delete_parser  (pc_parser_t *p);

pc_value_t *pc_fold_concat (int n, pc_result_t *r);
pc_value_t *pc_fold_str (int n, pc_result_t *r);
pc_value_t *pc_fold_nat (int n, pc_result_t *r);

pc_value_t *pc_apply_binop (pc_result_t *r);
pc_value_t *pc_apply_identity (pc_result_t *r);

// parser functions 

int pc_parse_char  (pc_input_t *i, pc_result_t *r, char c);
int pc_parse_range (pc_input_t *i, pc_result_t *r, char a, char b);

int pc_parse_match    (pc_input_t *i, pc_result_t *r, char c);
int pc_parse_run (pc_input_t *i, pc_result_t *r, pc_parser_t *p, int depth);


void pemdas();
void test();
void lisp();

int main() {
  //pemdas();
  /*test();*/
  lisp();
  return 0;
}


#endif
