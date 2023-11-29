#ifndef _HASHMAP_H_
#define _HASHMAP_H_

#include <stddef.h>
#include "vec.h"

// TODO: make this be determined dyamically later
#define HMAP_SIZE 100

typedef size_t (*hash_fn) (char*);

typedef struct entry entry;

typedef struct entry {
  char  *key;
  void  *value;
  entry *next;
} entry;

typedef struct hashmap {
  hash_fn hash;
  entry **entries;
  size_t cap;
} hashmap;

hashmap *hmap_new ();
void hmap_delete  (hashmap *hmap);
void hmap_delete_entry (entry *e);
entry *hmap_entry_new (char *key, void *value);

void *hmap_add  (hashmap *hmap, char *key, void *value);
void *hmap_get  (hashmap *hmap, char *key);
int hmap_find   (hashmap *hmap, char *key);

size_t fnv1a_hash (char* str);

#endif
