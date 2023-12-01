#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "parser.h"
#include "strop.h"

// TODO: add error messages
#define MIN(x, y) x < y ? x : y
#define MAX(x, y) x > y ? x : y
#define STACK_SIZE 32
#define PEEK 8
#define TRY(x) if (!x) { return 0; }
#define CHAR(i) *i->state.ptr

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define WHT   "\x1B[37m"
#define BOLD  "\x1B[1m"   
#define RESET "\x1B[0m"

typedef struct pc_error_t {
  char *mes;
  int type;
} pc_error_t;

typedef union pc_result_t {
  pc_value_t *value;
  pc_error_t *error;
} pc_result_t;

typedef struct pc_state_t {
  char *ptr;
  int col;
  int row;
  int pos;
} pc_state_t;

typedef struct pc_input_t {
  char *str;
  int len;
  int mark;
  pc_state_t state;
  pc_state_t marks[STACK_SIZE];
} pc_input_t;

enum {
  PC_TYPE_CHAR,
  PC_TYPE_RANGE,
  PC_TYPE_ONEOF,
  PC_TYPE_NONEOF,
  PC_TYPE_STRING,
  PC_TYPE_ANY,
  PC_TYPE_SOME,
  PC_TYPE_MORE, 
  PC_TYPE_COUNT,
  PC_TYPE_OR,
  PC_TYPE_AND,
  PC_TYPE_APPLY
};

typedef struct { char *x; } pc_string_t;
typedef struct { char x; char y; } pc_chars_t;
typedef struct { int n; pc_parser_t **xs; } pc_or_t;
typedef struct { int n; pc_fold_fn f; pc_dtor_fn d; pc_parser_t *x; } pc_count_t;
typedef struct { int n; pc_fold_fn f; pc_dtor_fn* ds; pc_parser_t **xs; } pc_and_t;
typedef struct { char *expect; pc_apply_fn f; pc_parser_t *x; } pc_apply_t;

typedef union pc_data_t {
  pc_chars_t chars;
  pc_string_t string;
  pc_count_t repeat;
  pc_apply_t apply;
  pc_or_t or;
  pc_and_t and;
} pc_data_t;

typedef struct pc_parser_t {
  int type;
  char *name;
  pc_data_t data;
  int ref_count;
} pc_parser_t;

pc_input_t *pc_string_input (const char *s) {
  pc_input_t *i = (pc_input_t*)malloc(sizeof(pc_input_t));
  i->str = pc_strcpy(s);
  i->len = strlen(s);
  i->mark = 0;
  
  i->state.col = 0;
  i->state.row = 0;
  i->state.pos = 0;
  i->state.ptr = i->str;
  
  return i;
}

int pc_input_eof (pc_input_t *i) {
  return i->state.pos == i->len || i->str[i->state.pos] == '\0';
}

int pc_input_mark (pc_input_t *i) {
  if (i->mark + 1 == STACK_SIZE) {
    printf("overflowed input stack!\n");
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

int pc_input_index (pc_input_t *i) {
  return i->state.ptr - i->str;
}

int pc_parse_match (pc_input_t *i, pc_result_t *r, char c) {
  r->value = malloc(2);
  ((char*)r->value)[0] = c;
  ((char*)r->value)[1] = '\0';

  i->state.col++;
  i->state.pos++;
  i->state.ptr++;
  
  if (i->str[i->state.pos] == '\n') {
    i->state.col = 0;
    i->state.row++;
    i->state.pos++;
    i->state.ptr++;
  }

  return 1;
}

void *pc_parse (const char *str, pc_parser_t *p) {
  pc_input_t *i = pc_string_input(str);
  pc_result_t r;
  
  if (!pc_parse_run(i, &r, p, 0)) {
    int idx = pc_input_index(i);

    int min = MAX(idx - PEEK, 0);
    int max = MIN(idx + PEEK, i->len);
    char *buf = (char*)malloc(max - min + 1);
    buf[max - min] = '\0';

    for (int j = 0; j < max - min; j++) {
      buf[j] = *(i->str + min + j);
    }
    
    printf("%serror%s - expected %s%s%s, but found:\n", RED, RESET, GRN, r.error->mes, RESET);
    int offset = idx - min + printf("    %d:%d| ", i->state.row, i->state.col);
   
    if (min != 0) { 
      offset += printf("..");
    }
    printf("%s", buf);
    if (max != i->len) { printf(".."); }
    printf("\n");

    char *padding = malloc(offset + 1);
    memset(padding, ' ', offset - 1);
    padding[offset] = '\0';

    printf("%s ^\n", padding);
    
    free(buf);
    free(padding);
    pc_delete_err(&r);
    
    return NULL;
  }

  return r.value;
}

int pc_parse_run (pc_input_t *i, pc_result_t *r, pc_parser_t *p, int depth) {
  pc_result_t stk[STACK_SIZE];
  r->value = NULL;
  int n = 0;

  switch (p->type) {
    case PC_TYPE_CHAR:   return pc_parse_char(i, r, &p->data);               
    case PC_TYPE_RANGE:  return pc_parse_range(i, r, &p->data);
    case PC_TYPE_ONEOF:  return pc_parse_oneof(i, r, &p->data);
    case PC_TYPE_STRING: return pc_parse_string(i, r, &p->data);
    case PC_TYPE_NONEOF: return pc_parse_noneof(i, r, &p->data);
    case PC_TYPE_ANY:    return pc_parse_any(i, r);

    // TODO: free error variants!!!

    case PC_TYPE_SOME: 
      while (pc_parse_run(i, stk+n, p->data.repeat.x, depth+1)) { n++; }
      r->value = (p->data.repeat.f)(n, stk);  
      return 1;
    
    case PC_TYPE_MORE: 
      TRY(pc_input_mark(i));
      while (pc_parse_run(i, stk+n, p->data.repeat.x, depth+1)) { n++; }
      if (n < p->data.repeat.n) {
        for (int i = 0; i < n-1; i++) { (p->data.repeat.d)(stk[i].value); }
        pc_input_rewind(i);
        pc_input_unmark(i);
        return pc_err(r, "at least %d more of %s", p->data.repeat.n - n, stk[n].error->mes);
      } 
      r->value = (p->data.repeat.f)(n, stk); 
      return 1;

   case PC_TYPE_COUNT: 
      TRY(pc_input_mark(i));
      while (pc_parse_run(i, stk+n, p->data.repeat.x, depth+1)) { 
        if (n > p->data.repeat.n) { 
          for (int i = 0; i < n; i++) { (p->data.repeat.d)(stk[n].value); }
          pc_input_rewind(i);
          pc_input_unmark(i);
          return pc_err(r, "exactly %d more of %s", p->data.repeat.n - n, stk[n].error->mes);
        }
        n++; 
      }
      r->value = (p->data.repeat.f)(n, stk);
      return 1;
    
    case PC_TYPE_OR: 
      while (n < p->data.or.n) {
        if (pc_parse_run(i, stk+n, p->data.or.xs[n], depth+1)) {
          r->value = stk[n].value;
          return 1;
        } 
        n++;
      }
      return pc_err(r, "%s", pc_err_merge(stk, ", or ", n));
      
    case PC_TYPE_AND: 
      pc_input_mark(i);
      while (n < p->data.and.n) {
        if (!pc_parse_run(i, stk+n, p->data.and.xs[n], depth+1)) {
          for (int i = 0; i < n; i++) { (p->data.and.ds[i])(stk[i].value); }
          pc_input_rewind(i);
          pc_input_unmark(i);
          return pc_err(r, "%s", stk[n].error->mes);
        }
        n++;
      }
      pc_input_unmark(i);
      r->value = (p->data.and.f)(n, stk);
      return 1;

    case PC_TYPE_APPLY: 
      if (pc_parse_run(i, stk, p->data.apply.x, depth+1)) {
        r->value = (p->data.apply.f)(stk);
        return 1;
      }

      if (p->data.apply.expect) {
        return pc_err(r, "%s", p->data.apply.expect);
      }
      return pc_err(r, "%s", stk[n].error->mes);

    default: return 0;
  }
}

int pc_err (pc_result_t *r, char *str, ...) {
  va_list fmt;
  va_start(fmt, str);
  size_t size = vsnprintf(NULL, 0, str, fmt);
  va_end(fmt);

  va_start(fmt, str);

  char *buf = malloc(size+1);
  vsnprintf(buf, size+1, str, fmt);
  va_end(fmt);

  r->error = (pc_error_t*)malloc(sizeof(pc_error_t));
  r->error->mes = buf;

  return 0;
}

int pc_parse_char (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  char c = CHAR(i);
  if (pc_input_eof(i) || c != d->chars.x) {
    return pc_err(r, "char '%c'", d->chars.x);
  }

  return pc_parse_match(i, r, c);
}

int pc_parse_range (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  char c = CHAR(i);
  if (pc_input_eof(i) || c < d->chars.x || c > d->chars.y) { 
    return pc_err(r, "char between '%c' and '%c'", d->chars.x, d->chars.y); 
  }

  return pc_parse_match(i, r, c);
}

int pc_parse_string (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  pc_input_mark(i);
  for (char *c = d->string.x; *c != '\0'; c++) {
    if (pc_input_eof(i) || CHAR(i) != *c) {
      pc_input_rewind(i);
      pc_input_unmark(i);
      return pc_err(r, "string \"%s\"", d->string.x);
    }
    i->state.pos++;
    i->state.ptr++;
  }
  
  r->value = pc_strcpy(d->string.x);
  return 1;
}

int pc_parse_oneof (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  if (pc_input_eof(i)) { return pc_err(r, "reached EOF!"); }

  for (char *c = d->string.x; *c != '\0'; c++) {
    if (CHAR(i) == *c) {
      return pc_parse_match(i, r, *c); 
    }
  }

  return pc_err(r, "any of \"%s\"", d->string.x);
}

int pc_parse_noneof (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  if (pc_input_eof(i)) { return pc_err(r, "reached EOF!"); }

  for (char *c = d->string.x; *c != '\0'; c++) {
    if (CHAR(i) == *c) { 
      return pc_err(r, "none of \"%s\"", d->string.x); 
    }
  }

  return pc_parse_match(i, r, *i->state.ptr);
}

int pc_parse_any (pc_input_t *i, pc_result_t *r) {
  if (pc_input_eof(i)) { return pc_err(r, "reached EOF!"); }
  return pc_parse_match(i, r, *i->state.ptr);
}

// Parser Constructors

pc_parser_t *pc_undefined (void) {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->ref_count = 0;
  p->name = NULL;

  return p;
}

pc_parser_t *pc_new (const char* name) {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->type = PC_TYPE_APPLY;
  p->data.apply.f = pc_apply_identity;
  p->data.apply.expect = NULL;
  p->name = pc_strcpy(name);

  return p;
}

void pc_define (pc_parser_t *p, pc_parser_t *x) {
  p->data.apply.x = x;
}

void pc_err_expect (pc_parser_t *p, char *expect) {
  int len = strlen(expect);
  p->data.apply.expect = malloc(len + 1);
  strcpy(p->data.apply.expect, expect);
}

pc_parser_t *pc_char (char x) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_CHAR;
  p->data.chars.x = x;

  return p;
}

pc_parser_t *pc_range (char x, char y) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_RANGE;
  p->data.chars.x = x;
  p->data.chars.y = y;

  return p;
}

pc_parser_t *pc_string (const char *x) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_STRING;
  p->data.string.x = pc_strcpy(x);

  return p;
}

pc_parser_t *pc_some (pc_fold_fn f, pc_parser_t *x) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_SOME;
  p->data.repeat.x = x;
  p->data.repeat.f = f;

  x->ref_count++;

  return p;
}

pc_parser_t *pc_more (pc_fold_fn f, int n, pc_parser_t *x, pc_dtor_fn d) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_MORE;
  p->data.repeat.n = n;
  p->data.repeat.x = x;
  p->data.repeat.f = f;
  p->data.repeat.d = d;

  x->ref_count++;

  return p;
}

pc_parser_t *pc_count (pc_fold_fn f, int n, pc_parser_t *x, pc_dtor_fn d) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_COUNT;
  p->data.repeat.n = n;
  p->data.repeat.x = x;
  p->data.repeat.f = f;
  p->data.repeat.d = d;

  x->ref_count++;

  return p;
}

pc_parser_t *pc_or (int n, ...) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_OR;
  p->data.or.n = n;
  p->data.or.xs = (pc_parser_t**)malloc(n * sizeof(pc_parser_t*));

  va_list xs;
  va_start(xs, n);

  for (int i = 0; i < n; i++) {
    pc_parser_t *x = va_arg(xs, pc_parser_t*);
    p->data.or.xs[i] = x;
    x->ref_count++;
  }

  va_end(xs);

  return p;
}

pc_parser_t *pc_and (pc_fold_fn f, int n, ...) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_AND;
  p->data.and.n = n;
  p->data.and.f = f;
  p->data.and.xs = (pc_parser_t**)malloc(n * sizeof(pc_parser_t*));
  p->data.and.ds = (pc_dtor_fn*)malloc(n * sizeof(pc_dtor_fn*));

  va_list xs;
  va_start(xs, n);

  // parsers
  for (int i = 0; i < n; i++) {
    pc_parser_t *x = va_arg(xs, pc_parser_t*);
    p->data.and.xs[i] = x;
    x->ref_count++;
  }

  // parser destructors
  for (int i = 0; i < n; i++) {
    pc_dtor_fn d = va_arg(xs, pc_dtor_fn);
    p->data.and.ds[i] = d;
  }

  va_end(xs);

  return p;
}

pc_parser_t *pc_apply (pc_apply_fn f, pc_parser_t *x) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_APPLY;
  p->data.apply.x = x;
  p->data.apply.f = f;
  p->data.apply.expect = NULL;

  x->ref_count++;

  return p; 
}

pc_parser_t *pc_oneof (const char *x) { 
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_ONEOF;
  p->data.string.x = pc_strcpy(x);

  return p;
}

pc_parser_t *pc_noneof (const char *x) { 
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_NONEOF;
  p->data.string.x = pc_strcpy(x);

  return p;
}

pc_parser_t *pc_any (void) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_ANY;

  return p;
}

void pc_delete_err (pc_result_t *r) {
  free(r->error->mes);
  free(r->error);
}

void pc_delete_parsers (int n, ...) {
  va_list ps;
  va_start(ps, n);

  for (int i = 0; i < n; i++) {
    pc_parser_t *p = va_arg(ps, pc_parser_t*);
    pc_delete_parser(p);
  }

  va_end(ps);
}

void pc_delete_parser (pc_parser_t *p) {
  switch (p->type) {
    case PC_TYPE_ANY:
    case PC_TYPE_CHAR:
    case PC_TYPE_RANGE:
      break;

    case PC_TYPE_ONEOF:
    case PC_TYPE_NONEOF: 
    case PC_TYPE_STRING:
      free(p->data.string.x);
      break;

    case PC_TYPE_SOME:
    case PC_TYPE_MORE:
    case PC_TYPE_COUNT:
      pc_deref_parser(p->data.repeat.x);
      break;
        
    case PC_TYPE_OR:
      for (int i = 0; i < p->data.or.n; i++) {
        pc_deref_parser(p->data.or.xs[i]);
      }
      free(p->data.or.xs);
      break;
    
    case PC_TYPE_AND: 
      for (int i = 0; i < p->data.and.n; i++) { 
        pc_deref_parser(p->data.and.xs[i]); 
      }
      free(p->data.and.xs);
      break;   

    case PC_TYPE_APPLY:
      if (p->data.apply.expect) {
        free(p->data.apply.expect); 
      }
      pc_deref_parser(p->data.apply.x);
      break;
  }

  free(p);
}

void pc_deref_parser (pc_parser_t *p) {
  p->ref_count--;
  if (p->ref_count == 0) { pc_delete_parser(p); }
}

// fold functions

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

char *pc_err_merge (pc_result_t *r, char *sep, int n) {
  int len = n > 1 
    ? strlen(sep) * (n - 1) 
    : 0;

  for (int i = 0; i < n; i++) {
    len += strlen(r[i].error->mes);
  }

  char *concat = malloc(len + 1);
  memset(concat, '\0', len);

  for (int i = 0; i < n; i++) {
    strcat(concat, r[i].error->mes);
    if (i + 1 < n) { strcat(concat, sep); }
  }

  return concat;
} 

pc_value_t *pc_fold_first (int n, pc_result_t *r) {
  for (int i = 1; i < n; i++) {
    free(r[i].value);
  }
  return r[0].value;
}

pc_value_t *pc_fold_second (int n, pc_result_t *r) {
  free(r[0].value);
  for (int i = 2; i < n; i++) {
    free(r[i].value);
  } 

  return r[1].value;
}

pc_parser_t *pc_wrap (char lhs, pc_parser_t *p, char rhs) {
  return pc_and(
      pc_fold_str, 
      3,
      pc_char(lhs), 
      p, 
      pc_char(rhs),
      free, free, free);
}

pc_parser_t *pc_join (pc_parser_t *lhs, char c, pc_parser_t *rhs) {
  return pc_and(
      pc_fold_str,
      3,
      lhs,
      pc_char(c),
      rhs,
      free, free, free);
}

pc_parser_t *whtspc () {
  return pc_some(
      pc_fold_str, 
      pc_or(3, pc_char(' '), pc_char('\t'), pc_char('\n')));
}

pc_parser_t *pc_token (pc_parser_t *p) {
  return pc_and(
      pc_fold_str, 
      2, 
      whtspc(), 
      p, 
      free, free);
}

pc_parser_t *pc_integer () {
  return pc_and(
      pc_fold_str,
      2,
      pc_range('1', '9'),
      pc_range('0', '9'));
}

pc_parser_t *pc_alpha () {
  return pc_or(
      2, 
      pc_range('a', 'z'), 
      pc_range('A', 'Z'));
}

pc_parser_t *pc_ident () {
  return pc_and(
      pc_fold_str,
      2,
      pc_or(
        2,
        pc_char('_'),
        pc_alpha()),
      pc_some(
        pc_fold_str,
        pc_or(
          3,
          pc_char('_'),
          pc_range('0', '9'),
          pc_alpha())));
}

void *grammar(const char *s) {
  // EBNF grammar syntax:
  //
  // EXPRESSION: PATTERN 
  //
  //   _____________________________
  //  | construction |   pattern    |
  //  |--------------|--------------|
  //  | associative  | (xxx)        |
  //  | string       | "xxx"        |
  //  | sub-expr     | <xxx>        |
  //  | choice       | xxx | xxx    |
  //  | many         | xxx[+]       |
  //  | repeat       | xxx[3+]      |
  //   ----------------------------- 
  
  

  pc_parser_t *atom = pc_new("atom");
  pc_parser_t *term = pc_new("term");
  pc_parser_t *expr = pc_new("expr");
  
  pc_parser_t *pat = pc_wrap(
      '<', 
      pc_ident(),
      '>'); 

  pc_parser_t *str = pc_wrap(
      '\'',
      pc_more(
        pc_fold_str,
        1,
        pc_noneof("\'"),
        free),
      '\'');


  pc_define(expr, 
      pc_or(2, pc_join(term, '|', expr), term));

  pc_define(term, 
      pc_or(2, pc_join(atom, '&', term), atom)); 
 
  pc_define(atom, 
      pc_or(2,
        pat,
        pc_wrap('(', expr, ')')));

  void *r = pc_parse(s, expr);
  
  pc_delete_parsers(3, expr, atom, term);
  
  return r;
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

  pc_parser_t *expr = pc_new("expr");
  pc_parser_t *term = pc_new("term");
  pc_parser_t *fact = pc_new("fact");

  pc_define(expr, pc_or(2, 
      pc_apply(
        pc_apply_binop,
        pc_and(pc_fold_concat, 3, fact, add, expr, free, free, free)),
      fact));

  pc_define(fact, pc_or(2, 
      pc_apply(
        pc_apply_binop, 
        pc_and(pc_fold_concat, 3, term, mul, fact, free, free, free)),
      term));

  pc_define(term, pc_or(2,
      pc_apply(
        pc_apply_binop,
        pc_and(pc_fold_concat, 3, lpr, expr, rpr, free, free, free)),
      nat));

  pc_result_t r;

  pc_parse_run(i, &r, expr, 0);
  printf("source: %s, parsed to index %d, parsed out %d\n", s, i->state.pos, *((int*)r.value));
}

void* test(const char *s) {
  pc_parser_t *var = pc_new("var");
  pc_parser_t *abst = pc_new("abst");
  pc_parser_t *appl = pc_new("appl");
  pc_parser_t *expr = pc_new("expr");
  
  pc_define(var, pc_range('a', 'z'));
  
  pc_define(expr, 
      pc_or(
        3, 
        abst,
        appl,
        var));
 
  pc_define(appl, 
      pc_and(
        pc_fold_str, 
        4,
        pc_char('('),
        expr, 
        expr,
        pc_char(')'),
        free, free, free, free));

  pc_define(abst, 
      pc_and(
        pc_fold_str, 
        6,
        pc_char('('),
        pc_char('L'), 
        var, 
        pc_char('.'), 
        expr,
        pc_char(')'),
        free, free, free, free, free, free));

  return pc_parse(s, expr);
}

void *lisp() {
  const char *s = "(add 1 2 (sub (add 1 2) 3 4) (mul 1 2))";
  pc_input_t *i = pc_string_input(s);

  pc_value_t *map_node(pc_result_t *r) {
    pc_node_t *node = pc_node((char*)r->value);
    free(r->value);
    return node;
  }

  pc_value_t *fold_node(int n, pc_result_t *r) {
    pc_node_t *head = (pc_node_t*)r[0].value;
    for (int i = 1; i < n; i++) {
      pc_push_node(head, (pc_node_t*)r[i].value);
    }
    return head;
  }

  pc_value_t *choose_node(int n, pc_result_t *r) {
    return r[1].value;
  }
  

  pc_parser_t *tok (pc_parser_t *p) {
    return pc_and(choose_node, 
        2, 
        pc_some(pc_fold_str, pc_char(' ')), 
        p, 
        free, free);
  }


  pc_parser_t *alpha = pc_more(pc_fold_str, 1, pc_range('a', 'z'), free);
  pc_parser_t *num = pc_more(pc_fold_str, 1, pc_range('0', '9'), free);

  pc_parser_t *expr = pc_new("expr");
  pc_parser_t *term = pc_new("term");

  // -- grammar --
  //
  // expr : "(" <term> ")" | ( <term> | <expr> )*
  // term : <number> | <ident> 

  pc_define(expr, 
      pc_and(choose_node, 3,
        pc_char('('), 
        pc_more(fold_node, 1,
          pc_or(2, 
            pc_apply(map_node, tok(term)),
            tok(expr)), free),            
        pc_char(')'), 
        free, free, free));

  pc_define(term,
      pc_or(2, num, alpha));

  pc_result_t r;

  return pc_parse_run(i, &r, expr, 0);
}


