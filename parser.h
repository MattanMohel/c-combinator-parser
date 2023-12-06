#ifndef _PARSER_H_ 
#define _PARSER_H_

#include <stdio.h>
#include "ast.h"

#define PC_NEW(p) pc_parser_t *p = pc_new(#p)
#define PC_DEF(p, v) pc_parser_t *p = pc_new(#p); pc_define(p, v)

typedef void pc_value_t;
typedef struct pc_error_t pc_error_t;
typedef struct pc_state_t pc_state_t;
typedef union pc_result_t pc_result_t;

typedef struct pc_parser_t pc_parser_t;
typedef struct pc_input_t  pc_input_t;
typedef union pc_data_t pc_data_t;

typedef void (*pc_dtor_t) (void*);
typedef pc_value_t* (*pc_apply_fn) (pc_result_t*);
typedef pc_value_t* (*pc_fold_t) (int, pc_result_t*);

pc_input_t *pc_string_input (const char* s);
char pc_input_char  (pc_input_t *i);
int pc_input_eoi    (pc_input_t *i);
int pc_input_mark   (pc_input_t *i);
int pc_input_unmark (pc_input_t *i);
int pc_input_rewind (pc_input_t *i);
int pc_parse_match  (pc_input_t *i, pc_result_t *r, char c);

void *pc_parse (const char* str, pc_parser_t* p);
int pc_parse_run (pc_input_t *i, pc_result_t *r, pc_parser_t *p, int depth);
int pc_err (pc_result_t *r, pc_state_t s, const char *str, ...);

int pc_parse_char   (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_range  (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_string (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_oneof  (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_noneof (pc_input_t *i, pc_result_t *r, pc_data_t *d);
int pc_parse_any    (pc_input_t *i, pc_result_t *r);
int pc_parse_eoi    (pc_input_t *i);

pc_parser_t *pc_undefined (void);
pc_parser_t *pc_new (const char* name);
void pc_define (pc_parser_t *p, pc_parser_t *x);
void pc_err_expect (pc_parser_t *p, char *expect);

pc_parser_t *pc_any     (void);
pc_parser_t *pc_eoi     (void);
pc_parser_t *pc_char    (char x);
pc_parser_t *pc_range   (char x, char y);
pc_parser_t *pc_oneof   (const char *x);
pc_parser_t *pc_noneof  (const char *x);
pc_parser_t *pc_string  (const char *x);
pc_parser_t *pc_some    (pc_fold_t f, pc_parser_t *x);
pc_parser_t *pc_more    (pc_fold_t f, int n, pc_parser_t *x, pc_dtor_t d);
pc_parser_t *pc_count   (pc_fold_t f, int n, pc_parser_t *x, pc_dtor_t d);
pc_parser_t *pc_and     (pc_fold_t f, int n, ...);
pc_parser_t *pc_or      (int n, ...);
pc_parser_t *pc_apply   (pc_apply_fn f, pc_parser_t *x);
pc_parser_t *pc_expect  (pc_parser_t *x, const char *e);
pc_parser_t *pc_expectf (pc_parser_t *x, const char *e, ...); 

pc_parser_t *pc_ident   ();
pc_parser_t *pc_alpha   ();
pc_parser_t *pc_numeric ();
pc_parser_t *pc_alphnum ();
pc_parser_t *pc_whtspc  ();
pc_parser_t *pc_blank   ();
pc_parser_t *pc_wrap    (const char *l, pc_parser_t *p, const char *r, pc_dtor_t dtor);
pc_parser_t *pc_tok     (pc_parser_t *p);
pc_parser_t *pc_strip   (pc_parser_t *p);

void pc_delete_err (pc_result_t *r);
void pc_delete_parsers (int n, ...);
void pc_delete_parser  (pc_parser_t *p);
void pc_deref (pc_parser_t *p);

pc_value_t *pcf_concat      (int n, pc_result_t *r);
pc_value_t *pcf_1st         (int n, pc_result_t *r);
pc_value_t *pcf_2nd         (int n, pc_result_t *r);
pc_value_t *pcf_3rd         (int n, pc_result_t *r);
pc_value_t *pcf_strfold     (int n, pc_result_t *r);
pc_value_t *pcf_nat         (int n, pc_result_t *r);
pc_value_t *pcf_discard     (int n, pc_result_t *r);

pc_value_t *pca_binop    (pc_result_t *r);
pc_value_t *pca_pass     (pc_result_t *r);

char *pc_err_merge (pc_result_t *r, char *sep, int n);

void *grammar (const char *str);

#endif
