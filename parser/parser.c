#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "parser.h"

// TODO: introduce a way for fold arguments to ignore the result
// of some of their matches, like being able to "skip" over whitespace
// without allocating ANY memory for it:w

#define RESULT_STACK_SIZE 12
#define MARK_STACK_SIZE   128

#define CHAR(i) i->str[i->state.pos]

typedef pc_value_t*(*pc_fold_t)(int, pc_result_t*);
typedef void pc_value_t;

typedef struct {
  int col;
  int row;
  int pos;
} pc_state_t;

typedef struct pc_input_t {
  char *str;
  int   len;
  pc_state_t state;
  pc_state_t marks[MARK_STACK_SIZE];
  int mark;
} pc_input_t;

typedef struct pc_error_t {
} pc_error_t;

typedef struct pc_result_t {
  union {
    pc_value_t *value;
    pc_error_t *error;
  };
} pc_result_t;

// parser types

typedef enum {
  PC_TYPE_CHAR,
  PC_TYPE_RANGE,
  PC_TYPE_ANYOF,
  PC_TYPE_NONEOF,
  PC_TYPE_PASS,
  PC_TYPE_SOME,
  PC_TYPE_LEAST,
  PC_TYPE_ANY,
  PC_TYPE_AND,
  PC_TYPE_APPLY,
  PC_TYPE_INSERT,
  PC_TYPE_INSPECT,
} pc_type_t;

typedef enum pc_error_type_t {
  PC_INFO,
  PC_PASS,
  PC_WARN,
  PC_FAIL,
} pc_error_type_t;

typedef struct { char c; }                               pc_data_char_t;
typedef struct { char a; char b; }                       pc_data_range_t;
typedef struct { pc_parser_t *p; pc_fold_t f; }          pc_data_some_t;
typedef struct { int n; pc_parser_t *p; pc_fold_t f; }   pc_data_least_t;
typedef struct { pc_parser_t **ps; int n; }              pc_data_any_t;
typedef struct { pc_parser_t **ps; int n; pc_fold_t f; } pc_data_and_t;
typedef struct { pc_parser_t *p; pc_apply_t a; }         pc_data_apply_t;
typedef struct { char *s; int l; }                       pc_data_insert_t;
typedef struct { char *s; int l; }                       pc_data_anyof_t;
typedef struct { char *s; int l; }                       pc_data_noneof_t;
typedef struct { }                                       pc_data_pass_t;
typedef struct { char *s; pc_error_type_t state; }       pc_data_inspect_t;

typedef union {
  pc_data_char_t     _char;
  pc_data_range_t    _range;
  pc_data_some_t     _some; 
  pc_data_least_t    _least; 
  pc_data_any_t      _any;
  pc_data_and_t      _and;
  pc_data_apply_t    _apply;
  pc_data_insert_t   _insert;
  pc_data_anyof_t    _anyof;
  pc_data_noneof_t   _noneof;
  pc_data_pass_t     _pass;
  pc_data_inspect_t  _inspect;
} pc_data_t;

typedef struct pc_parser_t {
  char* name;
  pc_type_t type;
  pc_data_t data;
} pc_parser_t;

// a recursive function that takes maps an input by a parser
// NOTE: a failed parser match MUST free any of its allocated memory
int pc_parse_run (pc_input_t *i, pc_result_t *r, pc_parser_t *p, int depth) {
  pc_result_t stk[RESULT_STACK_SIZE];
  r->value = NULL;
  char c = CHAR(i);
  int n = 0;

  switch (p->type) {
    case PC_TYPE_CHAR:  
      if (pc_input_eof(i) || c != p->data._char.c) return 0;
      return pc_parse_match(i, r, c);

    case PC_TYPE_RANGE: 
      if (pc_input_eof(i) || c < p->data._range.a || c > p->data._range.b) return 0;
      return pc_parse_match(i, r, c);

    case PC_TYPE_INSERT:
      r->value = malloc(p->data._insert.l+1);
      strcpy(r->value, p->data._insert.s);
      return 1;

    case PC_TYPE_SOME: 
      while (pc_parse_run(i, &stk[n], p->data._some.p, depth+1)) n++;
      r->value = (p->data._some.f)(n, stk);  
      return 1;
    
    case PC_TYPE_LEAST: 
      pc_input_mark(i);
      while (pc_parse_run(i, &stk[n], p->data._least.p, depth+1)) n++;
      if (n < p->data._least.n) {
        for (int i = 0; i < n; i++) free(stk[n].value);
        pc_input_rewind(i);
        pc_input_unmark(i);
        return 0;
      } 

      r->value = (p->data._least.f)(n, stk); 
      return 1;
    
    case PC_TYPE_ANY: 
      pc_input_mark(i);
      while (n < p->data._and.n) {
        if (pc_parse_run(i, stk, p->data._and.ps[n], depth+1)) {
          r->value = stk->value;
          pc_input_unmark(i);
          return 1;
        } 
        pc_input_rewind(i);
        n++;
      }

      pc_input_unmark(i);
      return 0;
    
    case PC_TYPE_AND: 
      while (n < p->data._and.n) {
        if (!pc_parse_run(i, &stk[n], p->data._and.ps[n], depth+1)) {
          for (int i = 0; i < n; i++) free(stk[n].value);
          return 0;
        }
        n++;
      }

      r->value = (p->data._and.f)(n, stk);
      return 1;

    case PC_TYPE_APPLY: 
      if (pc_parse_run(i, stk, p->data._apply.p, depth+1)) {
        r->value = (p->data._apply.a)(stk);
        return 1;
      }
      return 0;

    case PC_TYPE_ANYOF:
      while (n < p->data._anyof.l) {
        if (c == p->data._anyof.s[n]) return pc_parse_match(i, r, c);
        n++;
      }
      return 0;

    case PC_TYPE_NONEOF:
      while (n < p->data._noneof.l) {
        if (c == p->data._noneof.s[n]) return 0;
        n++;
      }
      return pc_parse_match(i, r, c);

    case PC_TYPE_PASS:
      if (pc_input_eof(i)) return 0;
      return pc_parse_match(i, r, c);

    case PC_TYPE_INSPECT:
      switch (p->data._inspect.state) {
        case PC_INFO: 
          printf("INFO (depth-%d): %s\n", depth, p->data._inspect.s);
          break;
        case PC_PASS: 
          printf("PASS (depth-%d): %s\n", depth, p->data._inspect.s);
          break;
        case PC_WARN: 
          printf("WARN (depth-%d): %s\n", depth, p->data._inspect.s);
          break;
        case PC_FAIL: 
          printf("FAIL (depth-%d): %s\n", depth, p->data._inspect.s);
          break;
      }

      return 1;
  }

  return 0;
}

void pc_delete_parsers (int n, ...) {
  va_list ps;
  va_start(ps, n);

  for (int i = 0; i <  n; i++) {
    pc_parser_t *p = va_arg(ps, pc_parser_t*);
    pc_delete_parser(p);
  }

  va_end(ps);
}

void pc_delete_parser (pc_parser_t *p) {
  switch (p->type) {
    case PC_TYPE_CHAR:
    case PC_TYPE_RANGE:
    case PC_TYPE_PASS:
      break;

    case PC_TYPE_SOME:
      if (p->data._some.p->name == NULL) {
        free(p->data._some.p);
      }
      break;
    
    case PC_TYPE_LEAST:
      if (p->data._least.p->name == NULL) {
        free(p->data._least.p);
      }
      break;
    
    case PC_TYPE_ANY:
      for (int i = 0; i < p->data._any.n; i++) {
        if (p->data._any.ps[i]->name == NULL) {
          free(p->data._any.ps[i]);
        }
      }
      free(p->data._any.ps);
      break;
    
    case PC_TYPE_AND: 
      for (int i = 0; i < p->data._and.n; i++) {
        if (p->data._and.ps[i]->name == NULL) {
          free(p->data._and.ps[i]);
        }
      }
      free(p->data._and.ps);
      break;
    
    case PC_TYPE_APPLY:
      if (p->data._apply.p->name == NULL) {
        free(p->data._apply.p);
      }
      break;

    case PC_TYPE_INSERT:
      free(p->data._insert.s);
      break;

    case PC_TYPE_ANYOF:
      free(p->data._anyof.s);
      break;

    case PC_TYPE_NONEOF:
      free(p->data._noneof.s);
      break;

    case PC_TYPE_INSPECT:
      free(p->data._inspect.s);
      break;
  }

  free(p);
}

pc_parser_t *pc_uninit () {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->name = NULL;
  return p;
}

pc_parser_t *pc_rule (const char* name) {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->type = PC_TYPE_APPLY;
  p->data._apply.a = pc_apply_identity;
  int len = strlen(name);
  p->name = (char*)malloc(len + 1);
  strcpy(p->name, name);

  return p;
}

int pc_define (pc_parser_t *rule, pc_parser_t *p) {
  if (rule->type != PC_TYPE_APPLY) {
    return 0;
  }
  rule->data._apply.p = p;
  return 1;
}

// parser constructors  

pc_parser_t *pc_char (char c) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_CHAR;
  p->data._char.c = c;

  return p;
}

pc_parser_t *pc_range (char a, char b) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_RANGE;
  p->data._range.a = a;
  p->data._range.b = b;

  return p;
}

pc_parser_t *pc_some (pc_fold_t f, pc_parser_t *c) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_SOME;
  p->data._some.p = c;
  p->data._some.f = f;

  return p;
}

pc_parser_t *pc_least (int n, pc_fold_t f, pc_parser_t *c) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_LEAST;
  p->data._least.n = n;
  p->data._least.p = c;
  p->data._least.f = f;

  return p;
}

pc_parser_t *pc_any (int n, ...) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_ANY;
  p->data._any.n = n;
  p->data._any.ps = (pc_parser_t**)malloc(n * sizeof(pc_parser_t*));

  va_list ps;
  va_start(ps, n);

  for (int i = 0; i < n; i++) {
    p->data._any.ps[i] = va_arg(ps, pc_parser_t*);
  }

  va_end(ps);
  return p;
}

pc_parser_t *pc_and (pc_fold_t f, int n, ...) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_AND;
  p->data._and.n = n;
  p->data._and.f = f;
  p->data._and.ps = (pc_parser_t**)malloc(n * sizeof(pc_parser_t*));

  va_list ps;
  va_start(ps, n);

  for (int i = 0; i < n; i++) {
    p->data._and.ps[i] = va_arg(ps, pc_parser_t*);
  }

  va_end(ps);
  return p;
}

pc_parser_t *pc_apply (pc_apply_t a, pc_parser_t *c) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_APPLY;
  p->data._apply.p = c;
  p->data._apply.a = a;

  return p; 
}

pc_parser_t *pc_insert (const char *s) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_INSERT;
  p->data._insert.l = strlen(s);
  p->data._insert.s = (char*)malloc(p->data._insert.l + 1);
  strcpy((char*)p->data._insert.s, s);
  return p;
}

pc_parser_t *pc_anyof (const char *s) { 
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_ANYOF;
  p->data._anyof.l = strlen(s);
  p->data._anyof.s = (char*)malloc(p->data._insert.l + 1);
  strcpy((char*)p->data._anyof.s, s);
  return p;
}

pc_parser_t *pc_noneof (const char *s) { 
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_NONEOF;
  p->data._noneof.l = strlen(s);
  p->data._noneof.s = (char*)malloc(p->data._insert.l + 1);
  strcpy(p->data._noneof.s, s);
  return p;
}

pc_parser_t *pc_pass () {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_PASS;
  return p;
}

pc_parser_t *pc_inspect(pc_error_type_t state, const char *s) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_TYPE_INSPECT;
  p->data._inspect.state = state;
  
  int len = strlen(s);
  p->data._inspect.s = (char*)malloc(len + 1);
  strcpy(p->data._inspect.s, s);
  return p;
}

int pc_parse_char (pc_input_t *i, pc_result_t *r, char c) {
  if (pc_input_eof(i) || i->str[i->state.pos] != c) {
    return 0;
  } else {
    return pc_parse_match(i, r, c);
  }
}

int pc_parse_range (pc_input_t *i, pc_result_t *r, char a, char b) {
  char c = i->str[i->state.pos];
  if (pc_input_eof(i) || c < a || c > b) {
    return 0;
  } else {
    return pc_parse_match(i, r, c);
  }
}

pc_value_t *pc_fold_concat (int n, pc_result_t *r) {
  void** concat = (void**)malloc(n * sizeof(void*));

  for (int i = 0; i < n; i++) {
    concat[i] = r[i].value;
  }

  r[0].value = (void*)concat;
  return r->value;
}

pc_value_t *pc_fold_str (int n, pc_result_t *r) {
  int len = 0;
  for (int i = 0; i < n; i++) {
    len += strlen((char*)r[i].value);
  }

  char *str = (char*)malloc(len + 1);
  memset(str, '\0', len);

  for (int i = 0; i < n; i++) {
    strcat(str, (char*)r[i].value);
    free(r[i].value);
  }

  return (void*)str;
}

// TODO: make this a general str -> num function
// that analyzes the chars one by one instead of 
// converting char* -> str -> int
pc_value_t *pc_fold_nat (int n, pc_result_t *r) {
  char *str = (char*)pc_fold_str(n, r); 
  int *nat = (int*)malloc(sizeof(int));
  
  *nat = atoi(str);
  free(str);
  
  return (void*)nat;
}

pc_value_t *pc_apply_identity (pc_result_t *r) {
  return r[0].value;
}

pc_value_t *pc_apply_binop (pc_result_t *r) {
  void** nest = (void**)r[0].value;
  int* result = (int*)malloc(sizeof(int));
  
  if (*((char*)nest[0]) == '(' && *((char*)nest[2]) == ')') {
    *result = *((int*)nest[1]);
  } else {
    int a = *((int*)nest[0]);
    int b = *((int*)nest[2]);
    char op = *((char*)nest[1]);
    
    switch (op) {
      case '+': 
        *result = a + b;
        break;
      case '-': 
        *result = a - b;
        break;
      case '*': 
        *result = a * b;
        break;
      case '/': 
        *result = a / b;
        break;
    }
  }
  for (int i = 0; i < 3; i++) {
    free(nest[i]);
  }
  free(nest);

  r[0].value = (void*)result;
  return r[0].value;
}

pc_parser_t *pc_alpha () {
  return pc_any(2, pc_range('a', 'z'), pc_range('A', 'Z'));
}

pc_parser_t *pc_word () {
  return pc_least(1, pc_fold_str, pc_alpha());
}

pc_parser_t *pc_digit () {
 return pc_range('0', '9');
}

pc_parser_t *pc_ident () {
  return pc_and(pc_fold_str, 2, 
      pc_any(2, 
        pc_alpha(), 
        pc_char('_')), 
      pc_some(
        pc_fold_str, 
        pc_any(3, 
          pc_alpha(), 
          pc_char('_'), 
          pc_digit())));
}

pc_input_t *pc_string_input (const char *s) {
  pc_input_t *i = (pc_input_t*)malloc(sizeof(pc_input_t));
  i->len = strlen(s);
  i->state.col = 0;
  i->state.row = 0;
  i->state.pos = 0;
  i->mark = 0;
  i->str = (char*)malloc(i->len + 1);
  strcpy(i->str, s);
  
  return i;
}

int pc_parse_match (pc_input_t *i, pc_result_t *r, char c) {
  r->value = malloc(2);
  ((char*)r->value)[0] = c;
  ((char*)r->value)[1] = '\0';

  i->state.col++;
  i->state.pos++;
  
  if (i->str[i->state.pos] == '\n') {
    i->state.col = 0;
    i->state.row++;
    i->state.pos++;
  }

  return 1;
}

int pc_input_eof (pc_input_t *i) {
  return i->state.pos == i->len || i->str[i->state.pos] == '\0';
}

int pc_input_mark (pc_input_t *i) {
  if (i->mark + 1 == MARK_STACK_SIZE) {
    printf("reahced max!\n");
    return 0;
  }

  i->marks[i->mark++] = i->state;
  return 1;
}

int pc_input_unmark (pc_input_t *i) {
  if (i->mark == 0) {
    return 0;
  }

  i->mark--;
  return 1;
}

int pc_input_rewind (pc_input_t *i) {
  if (i->mark == 0) {
    return 0;
  }

  i->state = i->marks[i->mark-1];
  return 1;
}


void pemdas () {
  const char *s = "1+2*(3+4+5*2)";
  pc_input_t *i = pc_string_input(s);

  pc_parser_t *digit = pc_range('0', '9');
  pc_parser_t *nat = pc_some(pc_fold_nat, digit);
  pc_parser_t *add = pc_char('+');
  pc_parser_t *mul = pc_char('*');
  pc_parser_t *lpr = pc_char('(');
  pc_parser_t *rpr = pc_char(')');

  pc_parser_t *expr = pc_rule("expr");
  pc_parser_t *term = pc_rule("term");
  pc_parser_t *fact = pc_rule("fact");

  pc_define(expr, pc_any(2, 
      pc_apply(
        pc_apply_binop,
        pc_and(pc_fold_concat, 3, fact, add, expr)),
      fact));

  pc_define(fact, pc_any(2, 
      pc_apply(
        pc_apply_binop, 
        pc_and(pc_fold_concat, 3, term, mul, fact)),
      term));

  pc_define(term, pc_any(2,
      pc_apply(
        pc_apply_binop,
        pc_and(pc_fold_concat, 3, lpr, expr, rpr)),
      nat));

  pc_result_t r;

  pc_parse_run(i, &r, expr, 0);
  printf("source: %s, parsed to index %d, parsed out %d\n", s, i->state.pos, *((int*)r.value));
}

void test () {
  const char *s = "  _ya12y sjkbwe   oioioi";
  pc_input_t *i = pc_string_input(s);
  pc_insert(" ");
  pc_result_t r;

  pc_value_t *pc_fold_ident (int n, pc_result_t *r) {
    (void)n;  
    return r[1].value;
  }

  pc_parser_t *t = pc_some(pc_fold_str, 
      pc_and(pc_fold_ident, 2, 
        pc_some(pc_fold_str, pc_char(' ')), 
        pc_and(pc_fold_str, 2, pc_ident(), pc_insert(" "))));

  pc_parse_run(i, &r, t, 0);
  printf("source: %s, parsed to index %d, result in '%s'\n", 
      s, 
      i->state.pos, 
      (const char*)r.value);

  pc_delete_parser(t);
}

void lisp () {
  const char *s = "(+ 1 2 3 (+ 1 2 3) (+ a b (+ 1 2))  a b c (+ 1 2))";
  pc_input_t *i = pc_string_input(s);
  pc_result_t r;
  
  pc_parser_t *token (pc_parser_t *p) {
    return pc_and(pc_fold_str, 2,
        pc_some(pc_fold_str, pc_char(' ')),
        p);
  }

  pc_parser_t *l_pr = pc_char('(');
  pc_parser_t *r_pr = pc_char(')');
  pc_parser_t *ident = pc_least(1, pc_fold_str, pc_range('a', 'z'));
  pc_parser_t *num   = pc_least(1, pc_fold_str, pc_range('0', '9'));

  pc_parser_t *term = pc_rule("term");
  pc_parser_t *expr = pc_rule("expr");
  
  pc_define(expr, pc_any(2, 
        pc_and(pc_fold_str, 3, 
          token(l_pr), 
          term, 
          token(r_pr)), 
        term));

  pc_define(term, pc_and(pc_fold_str, 2,
        token(pc_anyof("+")), 
        pc_some(pc_fold_str, token(pc_any(3, ident, num, expr))))); 

  pc_parse_run(i, &r, expr, 0);

  printf("source: %s, parsed to index %d out of %d\n", 
      s, 
      i->state.pos, 
      i->len);

  pc_delete_parsers(2, expr, term);
}
