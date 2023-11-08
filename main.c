#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"
#include "vec.h"
#include "ast.h"

int main(int argc, char** argv) {
  hashmap *hmap = hmap_new();


  return 0;
}

/*   +       +
 *  / \  =  / \
 * a   b   b   a 
 */

typedef struct pf_pattern_t {
  pc_node_t *a;
  pc_node_t *b;
} pf_pattern_t;

pc_node_t *apply_pattern (pf_pattern_t *pat, pc_node_t *a) {
  return NULL;
}
