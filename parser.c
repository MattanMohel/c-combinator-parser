#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "parser.h"
#include "strop.h"

#define MIN(x, y) x < y ? x : y
#define MAX(x, y) x > y ? x : y
#define TRY(x) if (!x) { return 0; }

#define PEEK 8
#define STACK_SIZE 32
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define WHT   "\x1B[37m"
#define BOLD  "\x1B[1m"   
#define RESET "\x1B[0m"

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

typedef struct pc_error_t {
  char *mes;
  pc_state_t state;
  int fatal;
} pc_error_t;

typedef union pc_result_t {
  pc_value_t *value;
  pc_error_t *error;
} pc_result_t;

enum {
  PC_TYPE_CHAR,
  PC_TYPE_RANGE,
  PC_TYPE_ONEOF,
  PC_TYPE_NONEOF,
  PC_TYPE_STRING,
  PC_TYPE_ANY,
  PC_TYPE_EOI,
  PC_TYPE_SOME,
  PC_TYPE_MORE, 
  PC_TYPE_COUNT,
  PC_TYPE_OR,
  PC_TYPE_AND,
  PC_TYPE_APPLY,
  PC_TYPE_EXPECT
};

typedef struct { char *x; }                                               pc_string_t;
typedef struct { char x; char y; }                                        pc_chars_t;
typedef struct { pc_parser_t *x; char *e; }                               pc_expect_t;
typedef struct { int n; pc_parser_t **xs; }                               pc_or_t;
typedef struct { pc_apply_fn f; pc_parser_t *x; }                         pc_apply_t;
typedef struct { int n; pc_fold_t f; pc_dtor_t d; pc_parser_t *x; }     pc_count_t;
typedef struct { int n; pc_fold_t f; pc_dtor_t* ds; pc_parser_t **xs; } pc_and_t;

typedef union pc_data_t {
  pc_chars_t  chars;
  pc_string_t string;
  pc_count_t  repeat;
  pc_apply_t  apply;
  pc_or_t     or;
  pc_and_t    and;
  pc_expect_t expect;
} pc_data_t;

typedef struct pc_parser_t {
  int type;
  char *name;
  pc_data_t data;
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

char pc_input_char (pc_input_t *i) {
  return *i->state.ptr;
}

int pc_input_eoi (pc_input_t *i) {
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
    int idx = r.error->state.ptr - i->str;

    int min = MAX(idx - PEEK, 0);
    int max = MIN(idx + PEEK, i->len);
    char *buf = (char*)malloc(max - min + 1);
    buf[max - min] = '\0';

    for (int j = 0; j < max - min; j++) {
      buf[j] = *(i->str + min + j);
    }
    
    printf("%serror%s - expected %s%s%s, but found:\n", RED, RESET, GRN, r.error->mes, RESET);
    int offset = idx - min + printf("    %d:%d| ", r.error->state.row, r.error->state.col);
   
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

    // primitives
    case PC_TYPE_CHAR:   return pc_parse_char(i, r, &p->data);               
    case PC_TYPE_RANGE:  return pc_parse_range(i, r, &p->data);
    case PC_TYPE_ONEOF:  return pc_parse_oneof(i, r, &p->data);
    case PC_TYPE_STRING: return pc_parse_string(i, r, &p->data);
    case PC_TYPE_NONEOF: return pc_parse_noneof(i, r, &p->data);
    case PC_TYPE_ANY:    return pc_parse_any(i, r);
    case PC_TYPE_EOI:    return pc_parse_eoi(i);

    case PC_TYPE_EXPECT:
      if (pc_parse_run(i, stk, p->data.expect.x, depth+1)) {
        r->value = stk->value;
        return 1;
      }      
      return pc_err(r, i->state, p->data.expect.e);

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
        return pc_err(r, stk[n].error->state, "at least %d more %s", p->data.repeat.n - n, stk[n].error->mes);
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
          return pc_err(r, stk[n].error->state, "exactly %d more of %s", p->data.repeat.n - n, stk[n].error->mes);
        }
        n++; 
      }
      r->value = (p->data.repeat.f)(n, stk);
      return 1;
    
    case PC_TYPE_OR: 
      while (n < p->data.or.n) {
        if (pc_parse_run(i, stk+n, p->data.or.xs[n], depth+1)) {
          for (int j = 0; j < n-1; j++) { pc_delete_err(stk+j); }
          r->value = stk[n].value;
          return 1;
        } 
        n++;
      }
      // TODO: free errors on failure!
      return pc_err(r, i->state, "%s", pc_err_merge(stk, ", or ", n));
      
    case PC_TYPE_AND: 
      pc_input_mark(i);
      while (n < p->data.and.n) {
        if (!pc_parse_run(i, stk+n, p->data.and.xs[n], depth+1)) {
          for (int i = 0; i < n; i++) (p->data.and.ds[i])(stk[i].value);
          pc_input_rewind(i);
          pc_input_unmark(i);
          return pc_err(r, stk[n].error->state, "%s", stk[n].error->mes);
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

      return pc_err(r, stk[n].error->state, "%s", stk[n].error->mes);

    default: 
      return pc_err(r, i->state, "undefined parser");
  }
}

int pc_err (pc_result_t *r, pc_state_t s, const char *str, ...) {
  va_list fmt;
  va_start(fmt, str);
  r->error = (pc_error_t*)malloc(sizeof(pc_error_t));
  r->error->mes = pc_format(str, fmt);
  r->error->state = s;
  return 0;
}

int pc_parse_char (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  char c = pc_input_char(i);
  if (pc_input_eoi(i) || c != d->chars.x) return 0;
  return pc_parse_match(i, r, c);
}

int pc_parse_range (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  char c = pc_input_char(i);
  if (pc_input_eoi(i) || c < d->chars.x || c > d->chars.y) return 0;
  return pc_parse_match(i, r, c);
}

int pc_parse_string (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  pc_input_mark(i);
  for (char *c = d->string.x; *c != '\0'; c++) {
    if (pc_input_eoi(i) || pc_input_char(i) != *c) {
      pc_input_rewind(i);
      pc_input_unmark(i);
      return 0;
    }
    i->state.pos++;
    i->state.ptr++;
  }
  
  r->value = pc_strcpy(d->string.x);
  return 1;
}

int pc_parse_oneof (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  if (pc_input_eoi(i)) return 0;
  char  x = pc_input_char(i);
  char *c = strchr(d->string.x, x);
  return c ? pc_parse_match(i, r, *c) : 0;
}

int pc_parse_noneof (pc_input_t *i, pc_result_t *r, pc_data_t *d) {
  if (pc_input_eoi(i)) return 0;
  char  x = pc_input_char(i);
  char *c = strchr(d->string.x, x);
  return c ? 0 : pc_parse_match(i, r, x);
}

int pc_parse_any (pc_input_t *i, pc_result_t *r) {
  if (pc_input_eoi(i)) return 0;
  return pc_parse_match(i, r, pc_input_char(i));
}

int pc_parse_eoi (pc_input_t *i) {
  return pc_input_eoi(i);
}

pc_parser_t *pc_undefined (void) {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->name = NULL;

  return p;
}

pc_parser_t *pc_new (const char* name) {
  pc_parser_t *p = (pc_parser_t*)malloc(sizeof(pc_parser_t));
  p->type = PC_TYPE_APPLY;
  p->data.apply.f = pca_pass;
  p->name = pc_strcpy(name);

  return p;
}

void pc_define (pc_parser_t *p, pc_parser_t *x) {
  p->data.apply.x = x;
}

pc_parser_t *pc_char (char x) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_CHAR;
  p->data.chars.x = x;
  return pc_expectf(p, "'%c'", x);
}

pc_parser_t *pc_range (char x, char y) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_RANGE;
  p->data.chars.x = x;
  p->data.chars.y = y;
  return pc_expectf(p, "'%c' to '%c'", x, y);
}

pc_parser_t *pc_string (const char *x) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_STRING;
  p->data.string.x = pc_strcpy(x);
  return pc_expectf(p, "\"%s\"", x);
}

pc_parser_t *pc_some (pc_fold_t f, pc_parser_t *x) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_SOME;
  p->data.repeat.x = x;
  p->data.repeat.f = f;
  return p;
}

pc_parser_t *pc_more (pc_fold_t f, int n, pc_parser_t *x, pc_dtor_t d) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_MORE;
  p->data.repeat.n = n;
  p->data.repeat.x = x;
  p->data.repeat.f = f;
  p->data.repeat.d = d;
  return p;
}

pc_parser_t *pc_count (pc_fold_t f, int n, pc_parser_t *x, pc_dtor_t d) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_COUNT;
  p->data.repeat.n = n;
  p->data.repeat.x = x;
  p->data.repeat.f = f;
  p->data.repeat.d = d;
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
  }

  va_end(xs);
  return p;
}

pc_parser_t *pc_and (pc_fold_t f, int n, ...) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_AND;
  p->data.and.n = n;
  p->data.and.f = f;
  p->data.and.xs = (pc_parser_t**)malloc(n * sizeof(pc_parser_t*));
  p->data.and.ds = (pc_dtor_t*)malloc((n-1) * sizeof(pc_dtor_t*));

  va_list xs;
  va_start(xs, n);

  for (int i = 0; i < n; i++) {
    pc_parser_t *x = va_arg(xs, pc_parser_t*);
    p->data.and.xs[i] = x;
  }

  for (int i = 0; i < n-1; i++) {
    pc_dtor_t d = va_arg(xs, pc_dtor_t);
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
  return p; 
}

pc_parser_t *pc_expect (pc_parser_t *x, const char *e) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_EXPECT;
  p->data.expect.x = x;
  p->data.expect.e = pc_strcpy(e);
  return p;
}

pc_parser_t *pc_expectf (pc_parser_t *x, const char *e, ...) {
  va_list fmt;
  va_start(fmt, e);
  
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_EXPECT;
  p->data.expect.x = x;
  p->data.expect.e = pc_format(e, fmt);
  return p;
} 

pc_parser_t *pc_oneof (const char *x) { 
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_ONEOF;
  p->data.string.x = pc_strcpy(x);
  return pc_expectf(p, "one of \"%s\"", x);
}

pc_parser_t *pc_noneof (const char *x) { 
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_NONEOF;
  p->data.string.x = pc_strcpy(x);
  return pc_expectf(p, "none of \"%s\"", x);
}

pc_parser_t *pc_any (void) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_ANY;
  return pc_expect(p, "any");
}

pc_parser_t *pc_eoi (void) {
  pc_parser_t *p = pc_undefined();
  p->type = PC_TYPE_EOI;
  return pc_expect(p, "eoi");
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
      pc_deref(p->data.repeat.x);
      break;
        
    case PC_TYPE_OR:
      for (int i = 0; i < p->data.or.n; i++) {
        pc_deref(p->data.or.xs[i]);
      }
      free(p->data.or.xs);
      break;
    
    case PC_TYPE_AND: 
      for (int i = 0; i < p->data.and.n; i++) { 
        pc_deref(p->data.and.xs[i]); 
      }
      free(p->data.and.xs);
      break;   

    case PC_TYPE_APPLY:
      pc_deref(p->data.apply.x);
      break;

    case PC_TYPE_EXPECT:
      free(p->data.expect.e);
      pc_deref(p->data.expect.x);
      break;
  }

  free(p);
}

void pc_deref (pc_parser_t *p) {
  if (!p->name) pc_delete_parser(p);
}

// fold functions

pc_value_t *pcf_concat (int n, pc_result_t *r) {
  void** concat = (void**)malloc(n * sizeof(void*));

  for (int i = 0; i < n; i++) {
    concat[i] = r[i].value;
  }

  r[0].value = (void*)concat;
  return r->value;
}

pc_value_t *pcf_1st (int n, pc_result_t *r) {
  if (n < 1) {
    printf("tried to get 3rd element, but only folded %d\n", n);
    for (int i = 0; i < n; i++) free(r[i].value);
    return NULL;
  }

  for (int i = 1; i < n; i++) {
    free(r[i].value);
  }

  return r[0].value;
}

pc_value_t *pcf_2nd (int n, pc_result_t *r) {
  if (n < 2) {
    printf("tried to get 2nd element, but only folded %d\n", n);
    for (int i = 0; i < n; i++) free(r[i].value);
    return NULL;
  }

  free(r[0].value);
  for (int i = 2; i < n; i++) {
    free(r[i].value);
  } 

  return r[1].value;
}

pc_value_t *pcf_3rd (int n, pc_result_t *r) {
  if (n < 3) {
    printf("tried to get 3rd element, but only folded %d\n", n);
    for (int i = 0; i < n; i++) free(r[i].value);
    return NULL;
  }
  
  free(r[0].value);
  free(r[1].value);
  for (int i = 3; i < n; i++) {
    free(r[i].value);
  } 

  return r[2].value;
}

pc_value_t *pcf_strfold (int n, pc_result_t *r) {
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

pc_value_t *pcf_nat (int n, pc_result_t *r) {
  char *str = (char*)pcf_strfold(n, r); 
  int tmp;

  char *ptr;
  tmp = strtol(str, &ptr, 10);
  
  if (*ptr != '\0') {
    printf("tried to parse '%s' as number\n", str);
    free(str);
    return NULL;
  } 

  free(str);
  int *nat = (int*)malloc(sizeof(int));
  *nat = tmp;
  
  return (void*)nat;
}

pc_value_t *pca_pass (pc_result_t *r) {
  return r[0].value;
}

pc_value_t *pca_binop (pc_result_t *r) {
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

pc_parser_t *pc_ident () {
  return pc_expect(
      pc_and(
        pcf_strfold,
        2,
        pc_or(
          2,
          pc_char('_'),
          pc_alpha()),
        pc_more(
          pcf_strfold,
          1,
          pc_or(
            2,
            pc_char('_'),
            pc_alphnum()),
          free)),
      "identifier");
}

pc_parser_t *pc_alpha () {
  return pc_expect(
      pc_or(
        2, 
        pc_range('a', 'z'), 
        pc_range('A', 'Z')),
      "alpha");
}

pc_parser_t *pc_numeric () {
  return pc_expect(
      pc_and(
        pcf_strfold,
        2,
        pc_range('1', '9'),
        pc_range('0', '9')),
      "numeric");
}

pc_parser_t *pc_alphnum() {
  return pc_expect(
      pc_or(
        2, 
        pc_numeric(), 
        pc_alpha()),
      "alphanumeric");
}

pc_parser_t *pc_blank () {
  return pc_expect(
      pc_oneof("\t\n "),
      "\\t \\n or ' '");
}

pc_parser_t *whtspc () {
  return pc_some(
      pcf_strfold, 
      pc_blank());
}

pc_parser_t *pc_wrap(const char* l, pc_parser_t *p, const char *r, pc_dtor_t dtor) {
  return pc_and(
     pcf_2nd,
     3,
     pc_string(l),
     p,
     pc_string(r),
     free, dtor);
}

pc_parser_t *pc_tok (pc_parser_t *p) {
  return pc_and(
      pcf_2nd, 
      2, 
      whtspc(), 
      p, 
      free);
}

pc_parser_t *pc_strip (pc_parser_t *p) {
  return pc_and(
      pcf_2nd,
      3,
      whtspc(),
      p,
      whtspc(),
      free, free);
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

  PC_NEW(expr);
  PC_NEW(term);
  PC_NEW(atom);

  pc_define(expr,
      pc_or(2, 
        pc_and(pcf_strfold, 3, term, pc_char('|'), expr, free, free),
        term));

  pc_define(term,
      pc_or(2,
        pc_and(pcf_strfold, 3, atom, pc_char(' '), term, free, free),
        atom));

  pc_define(atom,
      pc_or(2,
        pc_and(pcf_strfold, 3, pc_char('<'), pc_alpha(), pc_char('>'), free, free),
        pc_and(pcf_strfold, 3, pc_char('('), expr, pc_char(')'), free, free)));

  return pc_parse(s, pc_and(pcf_1st, 2, expr, pc_eoi(), free));
} 

