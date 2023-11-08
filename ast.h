#ifndef _AST_H_
#define _AST_H_

typedef struct pc_node_t pc_node_t;

typedef struct pc_node_t {
  char *str;
  pc_node_t **leafs;
  int n;
} pc_node_t;

pc_node_t *pc_node (const char *str);
pc_node_t *pc_push_node  (pc_node_t *node, pc_node_t *leaf);
void pc_push_nodes (pc_node_t *node, int n, ...);

char *pc_strcat   (char *dest, char *src);
char *pc_strcats  (char *dest, int n, ...);
void pc_ast_put (pc_node_t *node);
char *pc_ast_str (pc_node_t *node, int depth);

#endif
