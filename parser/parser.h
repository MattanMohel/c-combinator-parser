#ifndef _PARSER_H_ 
#define _PARSER_H_

#include <stdio.h>
#include "ast.h"

// TODO: reference counted parser/void* allocations

typedef struct pc_result_t pc_result_t;
typedef struct pc_parser_t pc_parser_t;
typedef struct pc_input_t  pc_input_t;
typedef struct pc_result_t pc_result_t;
typedef enum pc_error_type_t pc_error_type_t;
typedef void pc_value_t;

typedef pc_value_t*(*pc_fold_t)(int, pc_result_t*);
typedef pc_value_t*(*pc_map_t)(pc_result_t*);

pc_input_t *pc_string_input (const char *s);
int pc_input_eof (pc_input_t *i);

int pc_input_mark (pc_input_t *i);
int pc_input_unmark (pc_input_t *i);
int pc_input_rewind (pc_input_t *i);

// parser contructors

pc_parser_t *pc_uninit ();
pc_parser_t *pc_rule (const char* name);
int pc_define (pc_parser_t *rule, pc_parser_t *p);

pc_parser_t *pc_char     (char c);
pc_parser_t *pc_range    (char a, char b);
pc_parser_t *pc_oneof    (const char *s);
pc_parser_t *pc_noneof   (const char *s);
pc_parser_t *pc_string   (const char *s);
pc_parser_t *pc_any      ();
pc_parser_t *pc_some     (pc_fold_t f, pc_parser_t *c);
pc_parser_t *pc_more     (pc_fold_t f, int n, pc_parser_t *c);
pc_parser_t *pc_repeat   (pc_fold_t f, int n, pc_parser_t *c);
pc_parser_t *pc_choose   (int n, ...);
pc_parser_t *pc_chain    (pc_fold_t f, int n, ...);
pc_parser_t *pc_apply    (pc_map_t a, pc_parser_t *c);
pc_parser_t *pc_insert   (const char *s);
pc_parser_t *pc_inspect  (pc_error_type_t state, const char *s);

void pc_delete_parsers (int n, ...);
void pc_delete_parser  (pc_parser_t *p);
void pc_remove_parser (pc_parser_t *p);

pc_value_t *pc_fold_concat (int n, pc_result_t *r);
pc_value_t *pc_fold_str (int n, pc_result_t *r);
pc_value_t *pc_fold_nat (int n, pc_result_t *r);

pc_value_t *pc_apply_binop (pc_result_t *r);
pc_value_t *pc_apply_identity (pc_result_t *r);

int pc_parse_match    (pc_input_t *i, pc_result_t *r, char c);
int pc_parse_run (pc_input_t *i, pc_result_t *r, pc_parser_t *p, int depth);

void pemdas();
void lisp();
void test();
void grammar();

int main() {
  //pemdas();
  /*test();*/
  /*lisp();*/
  /*grammar();*/
  test();

  /*pc_node_t *a = pc_node("a");*/
  /*pc_node_t *b = pc_node("b");*/
  /*pc_node_t *c = pc_node("c");*/

  /*pc_push_nodes(a, 2, b, c);*/
  /*pc_push_node(c, b);*/

  /*pc_ast_put(a);*/

  return 0;
}


#endif
