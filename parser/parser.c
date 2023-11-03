#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "parser.h"

// TODO: introduce a way for fold arguments to ignore the result
// of some of their matches, like being able to "skip" over whitespace
// without allocating ANY memory for it


#define RESULT_STACK_SIZE 24
#define MARK_STACK_SIZE   24

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
  int len;
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
  PC_CHAR,
  PC_RANGE,
  PC_ONEOF,
  PC_NONEOF,
  PC_STRING,
  PC_ANY,
  PC_SOME,
  PC_MORE, 
  PC_REPEAT,
  PC_CHOOSE,
  PC_CHAIN,
  PC_APPLY,
  PC_INSERT,
  PC_INSPECT,
} pc_type_t;

typedef enum pc_error_type_t {
  PC_INFO,
  PC_WARN,
  PC_FAIL,
} pc_error_type_t;

typedef struct { char c; }                               pc_char_t;
typedef struct { char a; char b; }                       pc_range_t;
typedef struct { char *s; int l; }                       pc_string_t;
typedef struct { pc_parser_t *p; pc_fold_t f; }          pc_some_t;
typedef struct { int n; pc_parser_t *p; pc_fold_t f; }   pc_more_t;
typedef struct { int n; pc_parser_t *p; pc_fold_t f; }   pc_repeat_t;
typedef struct { pc_parser_t **ps; int n; }              pc_choose_t;
typedef struct { pc_parser_t **ps; int n; pc_fold_t f; } pc_chain_t;
typedef struct { pc_parser_t *p; pc_map_t a; }           pc_apply_t;
typedef struct { char *s; int l; }                       pc_insert_t;
typedef struct { char *s; int l; }                       pc_oneof_t;
typedef struct { char *s; int l; }                       pc_noneof_t;
typedef struct { }                                       pc_any_t;
typedef struct { char *s; pc_error_type_t state; }       pc_inspect_t;

typedef union {
  pc_char_t     _char;
  pc_range_t    _range;
  pc_string_t   _string;
  pc_some_t     _some; 
  pc_more_t     _more; 
  pc_repeat_t   _repeat;
  pc_choose_t   _choose;
  pc_chain_t    _chain;
  pc_apply_t    _apply;
  pc_insert_t   _insert;
  pc_oneof_t    _oneof;
  pc_noneof_t   _noneof;
  pc_any_t      _any;
  pc_inspect_t  _inspect;
} pc_data_t;

typedef struct pc_parser_t {
  char* name;
  pc_type_t type;
  pc_data_t data;
} pc_parser_t;

int pc_parse_run (pc_input_t *i, pc_result_t *r, pc_parser_t *p, int depth) {
  pc_result_t stk[RESULT_STACK_SIZE];
  r->value = NULL;
  char c = CHAR(i);
  int n = 0;

  switch (p->type) {
    case PC_CHAR:  
      if (pc_input_eof(i) || c != p->data._char.c) return 0;
      return pc_parse_match(i, r, c);

    case PC_RANGE: 
      if (pc_input_eof(i) || c < p->data._range.a || c > p->data._range.b) return 0;
      return pc_parse_match(i, r, c);

    case PC_STRING:
      pc_input_mark(i);
      while (n < p->data._string.l) {
        if (pc_input_eof(i) || CHAR(i) != p->data._string.s[n]) {
          pc_input_rewind(i);
          pc_input_unmark(i);
          return 0;
        }
        i->state.pos++;
        n++;
      }

      r->value = malloc(n + 1);
      strcpy((char*)r->value, p->data._string.s);
      return 1;

    case PC_INSERT:
      r->value = malloc(p->data._insert.l+1);
      strcpy(r->value, p->data._insert.s);
      return 1;

    case PC_SOME: 
      while (pc_parse_run(i, &stk[n], p->data._some.p, depth+1)) n++;
      r->value = (p->data._some.f)(n, stk);  
      return 1;
    
    case PC_MORE: 
      pc_input_mark(i);
      while (pc_parse_run(i, &stk[n], p->data._more.p, depth+1)) n++;
      if (n < p->data._more.n) {
        for (int i = 0; i < n; i++) free(stk[n].value);
        pc_input_rewind(i);
        pc_input_unmark(i);
        return 0;
      } 

      r->value = (p->data._more.f)(n, stk); 
      return 1;

   case PC_REPEAT:
      pc_input_mark(i);
      while (n < p->data._repeat.n && pc_parse_run(i, &stk[n], p->data._repeat.p, depth+1)) n++;
      if (n < p->data._repeat.n) {
        for (int i = 0; i < n; i++) free(stk[n].value);
        pc_input_rewind(i);
        pc_input_unmark(i);
        return 0;
      }

      r->value = (p->data._repeat.f)(n, stk);
      return 1;
    
    case PC_CHOOSE: 
      pc_input_mark(i);
      while (n < p->data._chain.n) {
        if (pc_parse_run(i, stk, p->data._chain.ps[n], depth+1)) {
          r->value = stk->value;
          pc_input_unmark(i);
          return 1;
        } 
        pc_input_rewind(i);
        n++;
      }

      pc_input_unmark(i);
      return 0;
    
    case PC_CHAIN: 
      while (n < p->data._chain.n) {
        if (!pc_parse_run(i, &stk[n], p->data._chain.ps[n], depth+1)) {
          for (int i = 0; i < n; i++) free(stk[n].value);
          return 0;
        }
        n++;
      }

      r->value = (p->data._chain.f)(n, stk);
      return 1;

    case PC_APPLY: 
      if (pc_parse_run(i, stk, p->data._apply.p, depth+1)) {
        r->value = (p->data._apply.a)(stk);
        return 1;
      }
      return 0;

    case PC_ONEOF:
      while (n < p->data._oneof.l) {
        if (c == p->data._oneof.s[n]) return pc_parse_match(i, r, c);
        n++;
      }
      return 0;

    case PC_NONEOF:
      while (n < p->data._noneof.l) {
        if (c == p->data._noneof.s[n]) return 0;
        n++;
      }
      return pc_parse_match(i, r, c);

    case PC_ANY:
      if (pc_input_eof(i)) return 0;
      return pc_parse_match(i, r, c);

    case PC_INSPECT:
      switch (p->data._inspect.state) {
        case PC_INFO: 
          printf("INFO (depth-%d): %s\n", depth, p->data._inspect.s);
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

  for (int i = 0; i < n; i++) {
    pc_parser_t *p = va_arg(ps, pc_parser_t*);
    pc_delete_parser(p);
  }

  va_end(ps);
}

void pc_delete_parser (pc_parser_t *p) {
  switch (p->type) {
    case PC_CHAR:
    case PC_RANGE:
    case PC_ANY:
      break;

    case PC_STRING:
      free(p->data._string.s);
      break;

    case PC_SOME:
      if (p->data._some.p->name == NULL) {
        pc_delete_parser(p->data._some.p);
      }
      break;
    
    case PC_MORE:
      if (p->data._more.p->name == NULL) {
        pc_delete_parser(p->data._more.p);
      }
      break;

    case PC_REPEAT:
      if (p->data._repeat.p->name == NULL) {
        pc_delete_parser(p->data._more.p);
      }
      break;
    
    case PC_CHOOSE:
      for (int i = 0; i < p->data._choose.n; i++) {
        if (p->data._choose.ps[i]->name == NULL) {
          pc_delete_parser(p->data._choose.ps[i]);
        }
      }
      free(p->data._choose.ps);
      break;
    
    case PC_CHAIN: 
      for (int i = 0; i < p->data._chain.n; i++) {
        if (p->data._chain.ps[i]->name == NULL) {
          pc_delete_parser(p->data._chain.ps[i]);
        }
      }
      free(p->data._chain.ps);
      break;
    
    case PC_APPLY:
      if (p->data._apply.p->name == NULL) {
        pc_delete_parser(p->data._apply.p);
      }
      break;

    case PC_INSERT:
      free(p->data._insert.s);
      break;

    case PC_ONEOF:
      free(p->data._oneof.s);
      break;

    case PC_NONEOF:
      free(p->data._noneof.s);
      break;

    case PC_INSPECT:
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
  p->type = PC_APPLY;
  p->data._apply.a = pc_apply_identity;
  int len = strlen(name);
  p->name = (char*)malloc(len + 1);
  strcpy(p->name, name);

  return p;
}

int pc_define (pc_parser_t *rule, pc_parser_t *p) {
  if (rule->type != PC_APPLY) {
    return 0;
  }
  rule->data._apply.p = p;
  return 1;
}

// parser constructors  

pc_parser_t *pc_char (char c) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_CHAR;
  p->data._char.c = c;

  return p;
}

pc_parser_t *pc_range (char a, char b) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_RANGE;
  p->data._range.a = a;
  p->data._range.b = b;

  return p;
}

pc_parser_t *pc_string (const char *s) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_STRING;
  p->data._string.l = strlen(s);
  p->data._string.s = (char*)malloc(p->data._insert.l + 1);
  strcpy((char*)p->data._string.s, s);
  return p;
}

pc_parser_t *pc_some (pc_fold_t f, pc_parser_t *c) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_SOME;
  p->data._some.p = c;
  p->data._some.f = f;

  return p;
}

pc_parser_t *pc_more (pc_fold_t f, int n, pc_parser_t *c) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_MORE;
  p->data._more.n = n;
  p->data._more.p = c;
  p->data._more.f = f;

  return p;
}

pc_parser_t *pc_repeat (pc_fold_t f, int n, pc_parser_t *c) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_REPEAT;
  p->data._repeat.n = n;
  p->data._repeat.p = c;
  p->data._repeat.f = f;

  return p;
}

pc_parser_t *pc_choose (int n, ...) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_CHOOSE;
  p->data._choose.n = n;
  p->data._choose.ps = (pc_parser_t**)malloc(n * sizeof(pc_parser_t*));

  va_list ps;
  va_start(ps, n);

  for (int i = 0; i < n; i++) {
    p->data._choose.ps[i] = va_arg(ps, pc_parser_t*);
  }

  va_end(ps);
  return p;
}

pc_parser_t *pc_chain (pc_fold_t f, int n, ...) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_CHAIN;
  p->data._chain.n = n;
  p->data._chain.f = f;
  p->data._chain.ps = (pc_parser_t**)malloc(n * sizeof(pc_parser_t*));

  va_list ps;
  va_start(ps, n);

  for (int i = 0; i < n; i++) {
    p->data._chain.ps[i] = va_arg(ps, pc_parser_t*);
  }

  va_end(ps);
  return p;
}

pc_parser_t *pc_apply (pc_map_t a, pc_parser_t *c) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_APPLY;
  p->data._apply.p = c;
  p->data._apply.a = a;

  return p; 
}

pc_parser_t *pc_insert (const char *s) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_INSERT;
  p->data._insert.l = strlen(s);
  p->data._insert.s = (char*)malloc(p->data._insert.l + 1);
  strcpy((char*)p->data._insert.s, s);
  return p;
}

pc_parser_t *pc_oneof (const char *s) { 
  pc_parser_t *p = pc_uninit();
  p->type = PC_ONEOF;
  p->data._oneof.l = strlen(s);
  p->data._oneof.s = (char*)malloc(p->data._insert.l + 1);
  strcpy((char*)p->data._oneof.s, s);
  return p;
}

pc_parser_t *pc_noneof (const char *s) { 
  pc_parser_t *p = pc_uninit();
  p->type = PC_NONEOF;
  p->data._noneof.l = strlen(s);
  p->data._noneof.s = (char*)malloc(p->data._insert.l + 1);
  strcpy(p->data._noneof.s, s);
  return p;
}

pc_parser_t *pc_any () {
  pc_parser_t *p = pc_uninit();
  p->type = PC_ANY;
  return p;
}

pc_parser_t *pc_inspect(pc_error_type_t state, const char *s) {
  pc_parser_t *p = pc_uninit();
  p->type = PC_INSPECT;
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

  pc_define(expr, pc_choose(2, 
      pc_apply(
        pc_apply_binop,
        pc_chain(pc_fold_concat, 3, fact, add, expr)),
      fact));

  pc_define(fact, pc_choose(2, 
      pc_apply(
        pc_apply_binop, 
        pc_chain(pc_fold_concat, 3, term, mul, fact)),
      term));

  pc_define(term, pc_choose(2,
      pc_apply(
        pc_apply_binop,
        pc_chain(pc_fold_concat, 3, lpr, expr, rpr)),
      nat));

  pc_result_t r;

  pc_parse_run(i, &r, expr, 0);
  printf("source: %s, parsed to index %d, parsed out %d\n", s, i->state.pos, *((int*)r.value));
}

void lisp () {
  const char *s = "(12)";
  pc_input_t *i = pc_string_input(s);
  pc_result_t r;
  
  pc_parser_t *token (pc_parser_t *p) {
    return pc_chain(pc_fold_str, 2,
        pc_some(pc_fold_str, pc_char(' ')),
        p);
  }

  pc_value_t *map_node (pc_result_t *r) {
    return (void*)pc_node("test");
  }

  pc_value_t *fold_node (int n, pc_result_t *r) {
    printf("folded %d\n", n);
    pc_node_t *node = pc_node("unamed");

    for (int i = 0; i < n; i++) {
      printf("pushed %s\n", ((pc_node_t*)(&(r[i].value)))->str);
      /*pc_push_node(node, (pc_node_t*)(result + i));*/
    }

    return node;
  }

  pc_value_t *fold_discard (int n, pc_result_t *r) {
    /*printf("have %d, discarding\n", n);*/
    pc_ast_put((pc_node_t*)r[1].value);
    return r[1].value;
  }

  pc_parser_t *l_pr = pc_char('(');
  pc_parser_t *r_pr = pc_char(')');
  pc_parser_t *ident = pc_more(pc_fold_str, 1, pc_range('a', 'z'));
  pc_parser_t *num   = pc_more(pc_fold_str, 1, pc_range('0', '9'));

  pc_parser_t *term = pc_rule("term");
  pc_parser_t *expr = pc_rule("expr");
  
  pc_define(expr, 
    pc_choose(2, 
      pc_chain(fold_discard, 3, 
        token(l_pr), 
        pc_some(fold_node, pc_apply(map_node, pc_range('0', '9'))), //pc_some(fold_node, term), 
        token(r_pr)), 
      term));

  pc_define(term, pc_apply(map_node, token(pc_choose(2, ident, num)))); 

  pc_parse_run(i, &r, expr, 0);

  /*pc_ast_put((pc_node_t*)(&r));*/

  printf("source: %s, parsed to index %d out of %d\n", 
      s, 
      i->state.pos, 
      i->len);

  pc_delete_parsers(2, expr, term);
}

void test() {
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

  pc_value_t *choose_second (pc_result_t *r) {
    return r[1].value;
  }

  pc_parser_t *tok (pc_parser_t *p) {
    return pc_chain(choose_node, 
        2, 
        pc_some(pc_fold_str, pc_char(' ')), 
        p);
  }

  pc_parser_t *alpha = pc_more(pc_fold_str, 1, pc_range('a', 'z'));
  pc_parser_t *num = pc_more(pc_fold_str, 1, pc_range('0', '9'));

  pc_parser_t *expr = pc_rule("expr");
  pc_parser_t *term = pc_rule("term");

  pc_define(expr, 
      pc_chain(choose_node, 3, 
        pc_char('('), 
        pc_some(fold_node, 
          pc_choose(2, 
            pc_apply(map_node, tok(term)),
            tok(expr))),            
        pc_char(')')));

  pc_define(term,
      pc_choose(2, num, alpha));

  pc_result_t r;

  int result = pc_parse_run(i, &r, expr, 0);

  if (result == 0)  {
    printf("failed!\n");
  } else {
    printf("succeeded!\n");
    pc_ast_put(r.value);
  }

  printf("source: %s, parsed to index %d of %d\n", s, i->state.pos, i->len);
}

void grammar () {
  const char *s = "term : a b <|> c (a (a b));";
  pc_input_t *i = pc_string_input(s);
  pc_result_t r;
  

  pc_parser_t *token (pc_parser_t *p) {
    return pc_chain(pc_fold_str, 2, 
        pc_some(pc_fold_str, pc_char(' ')), p);
  } 

  pc_parser_t *define = pc_char(':');
  pc_parser_t *end = pc_char(';');

  pc_parser_t *operator = pc_chain(pc_fold_str, 3,
      pc_char('<'),
      pc_some(pc_fold_str, pc_noneof("<>")),
      pc_char('>'));

  pc_parser_t *alpha = pc_range('a', 'z');
  pc_parser_t *word  = pc_more(pc_fold_str, 1, alpha);

  pc_parser_t *bind = pc_rule("bind");
  pc_parser_t *expr = pc_rule("expr");
  pc_parser_t *term = pc_rule("term");

  pc_define(bind, pc_chain(pc_fold_str, 4,
    token(word),
    token(define),
    expr,
    token(end)));

  pc_define(expr, pc_more(pc_fold_str, 1, term));

  pc_define(term, pc_choose(3, 
        token(word), 
        token(operator), 
        pc_chain(pc_fold_str, 3, 
          token(pc_char('(')), 
          token(expr), 
          token(pc_char(')')))));

  // escape syntax --> 
  //
  // bind : <word> : expr ;
  // expr : <term> <1+>;
  // term : <word> <|> \< <ident> \> <|> \( <expr> \) 
  //
  // literal syntax -->
  //
  // bind : <word> ":" <expr> ";"
  // expr : <term> +
  // term : <word> <|> "<" <ident> ">" <|> "(" <expr> ")" 
  //
  // literal syntax is a lot more consistent

  if (!pc_parse_run(i, &r, pc_more(pc_fold_str, 1, bind), 0)) {
    printf("pattern failed!\n");
  }

  printf("source: %s, parsed to index %d out of %d\n", 
      s, 
      i->state.pos, 
      i->len);

  pc_delete_parsers(3, bind, expr, term);
}
