#include "ast.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

pc_node_t *pc_node (const char *str) {
  pc_node_t *node = (pc_node_t*)malloc(sizeof(pc_node_t));
  int len = strlen(str);
  node->str = (char*)malloc(len + 1);
  strcpy(node->str, str);
  return node;
}

pc_node_t *pc_push_node (pc_node_t *node, pc_node_t *leaf) {
  node->n++;
  node->leafs = realloc(node->leafs, node->n * sizeof(pc_node_t*));
  node->leafs[node->n - 1] = leaf;
  return leaf;
}

void pc_push_nodes (pc_node_t *node, int n, ...) {
  node->leafs = realloc(node->leafs, node->n + n);

  va_list leafs;
  va_start(leafs, n);

  for (int i = node->n; i < node->n + n; i++) {
    node->leafs[i] = va_arg(leafs, pc_node_t*);
  }

  va_end(leafs);
  
  node->n += n;
}

char *pc_strcat (char *dest, char *src) {
  int a = strlen(dest);
  int b = strlen(src);
  
  dest = realloc(dest, a + b + 1);
  strcat(dest, src);
  return dest;
}

char *pc_strcats (char *dest, int n, ...) {
  va_list strs;
  va_start(strs, n);

  for (int i = 0; i < n; i++) {
    dest = pc_strcat(dest, va_arg(strs, char*));
  }

  va_end(strs);
  return dest;
}

void pc_ast_put (pc_node_t *node) {
  char *buf = pc_ast_str(node, 0);
  printf("%s", buf);
  free(buf);
} 

char *pc_ast_str (pc_node_t *node, int depth) {
  char *idt = malloc(depth + 1);
  memset(idt, '\t', depth);
  *(idt + depth) = '\0';

  char *buf = (char*)malloc(1);
  *buf = '\0';

  buf = pc_strcats(buf, 2, idt, node->str);
  if (node->n != 0) buf = pc_strcat(buf, " {\n"); 

  for (int i = 0; i < node->n; i++) {
    char *tmp = pc_ast_str(node->leafs[i], depth+1);
    buf = pc_strcat(buf, tmp);
    free(tmp);
  }
  
  buf = strcat(buf, idt);
  if (node->n != 0) buf = pc_strcat(buf, "}");
  return pc_strcat(buf, "\n");
}

