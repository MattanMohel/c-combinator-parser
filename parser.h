#ifndef _PARSER_H_ 
#define _PARSER_H_

#include <stdio.h>
#include "ast.h"

typedef void pc_value_t;
typedef struct pc_error_t pc_error_t;
typedef union pc_result_t pc_result_t;

typedef struct pc_parser_t pc_parser_t;
typedef struct pc_input_t  pc_input_t;
typedef union pc_data_t pc_data_t;

typedef void (*pc_dtor_fn) (void*);
typedef pc_value_t* (*pc_apply_fn) (pc_result_t*);
typedef pc_value_t* (*pc_fold_fn) (int, pc_result_t*);

pc_input_t *pc_string_input (const char* s);
int pc_input_eof    (pc_input_t *i);
int pc_input_mark   (pc_input_t *i);
int pc_input_unmark (pc_input_t *i);
int pc_input_rewind (pc_input_t *i);
int pc_parse_match  (pc_input_t *i, pc_result_t *r, char c);

void *pc_parse (const char* str, pc_parser_t* p);
int pc_parse_run (pc_input_t *i, pc_result_t *r, pc_parser_t *p, int depth);
int pc_err (pc_result_t *r, char *str, ...);

int pc_parse_char   (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_range  (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_string (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_oneof  (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_noneof (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_any    (pc_input_t *i, pc_result_t *r);

pc_parser_t *pc_undefined (void);
pc_parser_t *pc_new (const char* name);
void pc_define (pc_parser_t *p, pc_parser_t *x);
void pc_err_expect (pc_parser_t *p, char *expect);

pc_parser_t *pc_any    (void);
pc_parser_t *pc_char   (char x);
pc_parser_t *pc_range  (char x, char y);
pc_parser_t *pc_oneof  (const char *x);
pc_parser_t *pc_noneof (const char *x);
pc_parser_t *pc_string (const char *x);
pc_parser_t *pc_some   (pc_fold_fn f, pc_parser_t *x);
pc_parser_t *pc_more   (pc_fold_fn f, int n, pc_parser_t *x, pc_dtor_fn d);
pc_parser_t *pc_count  (pc_fold_fn f, int n, pc_parser_t *x, pc_dtor_fn d);
pc_parser_t *pc_and    (pc_fold_fn f, int n, ...);
pc_parser_t *pc_or     (int n, ...);
pc_parser_t *pc_apply  (pc_apply_fn f, pc_parser_t *x);

/*pc_parser_t *pc_wrap  (char lhs, char rhs, pc_parser_t *p, pc_dtor_fn dtor);*/
/*pc_parser_t *pc_token (pc_parser_t *p, pc_dtor_fn dtor);*/
/*pc_parser_t *pc_ident ();*/
/*pc_parser_t *pc_float ();*/

void pc_delete_err (pc_result_t *r);

void pc_delete_parsers (int n, ...);
void pc_delete_parser  (pc_parser_t *p);
void pc_deref_parser   (pc_parser_t *p);

pc_value_t *pc_fold_concat (int n, pc_result_t *r);
pc_value_t *pc_fold_first  (int n, pc_result_t *r);
pc_value_t *pc_fold_second (int n, pc_result_t *r);
pc_value_t *pc_fold_str    (int n, pc_result_t *r);
pc_value_t *pc_fold_nat    (int n, pc_result_t *r);
pc_value_t *pc_fold_ignore (int n, pc_result_t *r);

pc_value_t *pc_apply_binop    (pc_result_t *r);
pc_value_t *pc_apply_identity (pc_result_t *r);

char *pc_err_merge (pc_result_t *r, char *sep, int n);

void *grammar (const char *str);
void pemdas();
void *lisp();
void *test(const char *s);

#endif
