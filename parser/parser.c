#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "parser.h"

#define RESULT_STACK_SIZE 12
#define MARK_STACK_SIZE   128

typedef pc_value_t*(*pc_fold_t)(int, pc_result_t*);
typedef void pc_value_t;

// parser types

typedef enum {
  PC_TYPE_CHAR  = 0,
  PC_TYPE_RANGE = 1,
  PC_TYPE_SOME  = 2,
  PC_TYPE_ANY   = 3,
  PC_TYPE_AND   = 4,
  PC_TYPE_APPLY = 5,
} pc_type_t;

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

int pc_input_eof (pc_input_t *i) {
  return i->state.pos == i->len || i->str[i->state.pos] == '\0';
}

int pc_input_push_mark (pc_input_t *i) {
  if (i->mark + 1 == MARK_STACK_SIZE) {
    printf("reahced max!\n");
    return 0;
  }

  i->marks[i->mark++] = i->state;
  return 1;
}

int pc_input_pop_mark (pc_input_t *i) {
  if (i->mark == 0) {
    return 0;
  }

  i->mark--;
  return 1;
}

int pc_input_peek_mark (pc_input_t *i) {
  if (i->mark == 0) {
    return 0;
  }

  i->state = i->marks[i->mark-1];
  return 1;
}

typedef struct { char c; }                               pc_data_char_t;
typedef struct { char a; char b; }                       pc_data_range_t;
typedef struct { pc_parser_t *p; pc_fold_t f; }          pc_data_some_t;
typedef struct { pc_parser_t **ps; int n; }              pc_data_any_t;
typedef struct { pc_parser_t **ps; int n; pc_fold_t f; } pc_data_and_t;
typedef struct { pc_parser_t *p; pc_apply_t a; }         pc_data_apply_t;

typedef union {
  pc_data_char_t   _char;
  pc_data_range_t _range;
  pc_data_some_t   _some; 
  pc_data_any_t     _any;
  pc_data_and_t     _and;
  pc_data_apply_t _apply;
} pc_data_t;

typedef struct pc_error_t {
  // const char* mes;
  // int error;
} pc_error_t;

typedef struct pc_result_t {
  union {
    pc_value_t *value;
    pc_error_t *error;
  };
} pc_result_t;

typedef struct pc_parser_t {
  char* name;
  pc_type_t type;
  pc_data_t data;
} pc_parser_t;

int pc_parse_match (pc_input_t *i, pc_result_t *r, char c) {
  r->value = malloc(1);
  *((char*)r->value) = c;

  i->state.col++;
  i->state.pos++;
  
  if (i->str[i->state.pos] == '\n') {
    i->state.col = 0;
    i->state.row++;
    i->state.pos++;
  }

  return 1;
}

int pc_parse_no_match (pc_input_t *i) {
  return 0;
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


int pc_parse_char (pc_input_t *i, pc_result_t *r, char c) {
  if (pc_input_eof(i) || i->str[i->state.pos] != c) {
    return pc_parse_no_match(i);
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
  char* str = (char*)malloc(n + 1);
  str[n] = '\0';

  for (int i = 0; i < n; i++) {
    str[i] = *((char*)r[i].value);
    free(r[i].value);
  }

  r[0].value = (void*)str;
  return r->value;
}

pc_value_t *pc_fold_nat (int n, pc_result_t *r) {
  char* str = (char*)malloc(n + 1);
  str[n] = '\0';

  for (int i = 0; i < n; i++) {
    str[i] = *((char*)r[i].value);
    free(r[i].value);
  }
  int* nat = (int*)malloc(sizeof(int));
  *nat = atoi(str);

  free(str);
  r[0].value = (void*)nat;
  return r->value;
}



int pc_parse_run (pc_input_t *i, pc_result_t *r, pc_parser_t *p, int depth) {
  pc_result_t stk[RESULT_STACK_SIZE];
  int n = 0;

  switch (p->type) {
    case PC_TYPE_CHAR:  return pc_parse_char(i, r, p->data._char.c);
    case PC_TYPE_RANGE: return pc_parse_range(i, r, p->data._range.a, p->data._range.b);
    
    case PC_TYPE_SOME: {
      while (pc_parse_run(i, &stk[n], p->data._some.p, depth+1)) n++;
      r->value = (p->data._some.f)(n, stk); 
    
      return n != 0;
    }

    case PC_TYPE_ANY: {
      pc_input_push_mark(i);
      
      while (n < p->data._and.n) {
        if (pc_parse_run(i, stk, p->data._and.ps[n], depth+1)) {
          r->value = stk->value;
          pc_input_pop_mark(i);
          return 1;
        } else {
          pc_input_peek_mark(i);
        }
    
        n++;
      }

      pc_input_pop_mark(i);

      return 0;
    }
    
    case PC_TYPE_AND: {
      for (; n < p->data._and.n; n++) {
        if (!pc_parse_run(i, &stk[n], p->data._and.ps[n], depth+1)) {
          return 0;
        }
      }

      r->value = (p->data._and.f)(n, stk);
    
      return 1;
    }

    case PC_TYPE_APPLY: {
      int result = pc_parse_run(i, stk, p->data._apply.p, depth+1);
      if (result) {
        r->value = (p->data._apply.a)(stk);
      } else {
      }
      return result;
    }
  }

  return 0;  
}


pc_value_t *pc_apply_binop (pc_result_t *r) {
  void** nest = (void**)r[0].value;
  int a = *((int*)nest[0]);
  int b = *((int*)nest[2]);
  char op = *((char*)nest[1]);
  int* result = (int*)malloc(sizeof(int));
  
  switch (op) {
    case '+': 
      *result = a + b;
      printf("adding %d and %d\n", a, b);
      break;
    case '-': 
      *result = a - b;
      break;
    case '*': 
      *result = a * b;
      printf("multiplying %d and %d\n", a, b);
      break;
    case '/': 
      *result = a / b;
      break;
  }

  for (int i = 0; i < 3; i++) {
    free(nest[i]);
  }
  free(nest);

  r[0].value = (void*)result;
  return r[0].value;
}

pc_value_t *pc_apply_identity (pc_result_t *r) {
  return r[0].value;
}

void unordered_arithmetic() {
  const char *s = "1*2+3*4*5";
  pc_input_t *i = pc_string_input(s);

  pc_parser_t *dgt = pc_range('0', '9');
  pc_parser_t *num = pc_some(pc_fold_nat, dgt);
  pc_parser_t *op_add = pc_char('+');
  pc_parser_t *op_mul = pc_char('*');

  pc_parser_t *expr = pc_rule("expr");
  pc_parser_t *term = pc_rule("term");

  expr->data._apply.p = pc_and(pc_fold_concat, 3, num, pc_any(2, op_mul, op_add), term);
  expr->data._apply.a = pc_apply_binop;

  term->data._apply.p = pc_any(2, expr, num);
  term->data._apply.a = pc_apply_identity;
  
  pc_result_t r;

  pc_parse_run(i, &r, expr, 0);
  printf("source: %s, parsed to index %d, parsed out %d\n", s, i->state.pos, *((int*)r.value));
}
 void pemdas () {
   const char *s = "1+2*3+4";
   pc_input_t *i = pc_string_input(s);

   pc_parser_t *digit = pc_range('0', '9');
   pc_parser_t *nat = pc_some(pc_fold_nat, digit);
   pc_parser_t *add = pc_char('+');
   pc_parser_t *mul = pc_char('*');

   pc_parser_t *expr = pc_rule("expr");
   pc_parser_t *term = pc_rule("term");

   expr->data._apply.p = pc_any(2, 
       pc_apply(
         pc_apply_binop,
         pc_and(pc_fold_concat, 3, term, add, expr)),
       term);

   term->data._apply.p = pc_any(2, 
       pc_apply(
         pc_apply_binop, 
         pc_and(pc_fold_concat, 3, nat, mul, nat)),
       nat);

   pc_result_t r;

   pc_parse_run(i, &r, expr, 0);
   printf("source: %s, parsed to index %d, parsed out %d\n", s, i->state.pos, *((int*)r.value));
 }
