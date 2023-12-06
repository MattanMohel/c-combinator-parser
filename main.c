#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "hashmap.h"
#include "vec.h"
#include "ast.h"
#include "strop.h"

int main() {
  pc_result_t *r = grammar("<a> <b> ((<c>)|<d>)"); 
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

/*pc_node_t *apply_pattern (pf_pattern_t *pat, pc_node_t *a) { */
/*return NULL;*/
/*}*/
