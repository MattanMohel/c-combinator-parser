#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "hashmap.h"
#include "vec.h"
#include "ast.h"

int main(int argc, char** argv) {
  void *r = test("(Lx.(xx))"); //grammar("      \t\t\n\n x");
  
  /*pc_parser_t *p = pc_or(2, pc_char('a'), pc_char('b'));*/
  /*void *r = pc_parse("b", p);*/

  if (r) { printf("%s\n", (char*)r); }

  return 0;
}

/* 
 *   +       +
 *  / \  =  / \
 * a   b   b   a 
 */

typedef struct pattern_t {
  pc_node_t *a;
  pc_node_t *b;
} pf_pattern_t;

pc_node_t *apply_pattern (pf_pattern_t *pat, pc_node_t *a) { 
  return NULL;
}
