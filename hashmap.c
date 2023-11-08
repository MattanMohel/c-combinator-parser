#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET 0xcbf29ce484222325 
#define FNV_PRIME  0x00000001b3

hashmap *hmap_new () {
  hashmap *hmap = (hashmap*)malloc(sizeof(hashmap));
  hmap->entries = (entry**)malloc(HMAP_SIZE * sizeof(entry*));
  hmap->hash = fnv1a_hash;
  hmap->cap = HMAP_SIZE;
  return hmap;
}

void hmap_delete (hashmap *hmap) {
  for (size_t i = 0; i < hmap->cap; i++) {
    entry *head = hmap->entries[i]->next;
    hmap_delete_entry(head);
  }

  free(hmap->entries);
  free(hmap);
}

void hmap_delete_entry (entry *e) {
  entry *next = e->next;
  free(e);

  if (next) {
    hmap_delete_entry(next);
  }
}
  
entry *hmap_entry_new (char *key, void *value) {
  entry *e = (entry*)malloc(sizeof(entry));
  e->key = (char*)malloc(strlen(key) + 1);
  strcpy(e->key, key);
  e->value = value;
  return e;
}

void *hmap_add (hashmap *hmap, char *key, void *value) {
  entry *e = hmap_entry_new(key, value);
  size_t hash = (hmap->hash)(e->key) % hmap->cap;   
  entry *head = hmap->entries[hash];
  entry *prev = NULL;

  
  if (!head) {
    hmap->entries[hash] = e;
    return NULL;
  } 

  while (1) { 
    if (strcmp(e->key, head->key) == 0) {
      if (!prev) {
         e->next = head->next;
         hmap->entries[hash] = e;
      } 
      else {
        e->next = head->next;
        prev->next = e;
      }

      free(head->key);
      return head->value;
    }

    if (!head->next) {
      break;
    }

    prev = head;
    head = head->next; 
  }

  head->next = e;
  return NULL;
}

void *hmap_get (hashmap *hmap, char *key) {
  size_t hash = (hmap->hash)(key) % hmap->cap;
  entry *head = hmap->entries[hash];

  while(1) {
    if (strcmp(key, head->key) == 0) {
      return head->value;
    }
    
    if (!head->next) {
      break;
    }

    head = head->next;
  }

  return NULL;
}

int hmap_find (hashmap *hmap, char *key) {
  size_t hash = (hmap->hash)(key) % hmap->cap;
  entry *head = hmap->entries[hash];

  while(1) {
    if (strcmp(key, head->key) == 0) {
      return 1;
    }
    
    if (!head->next) {
      break;
    }

    head = head->next;
  }

  return 0;
}

size_t fnv1a_hash (char *str) {
  int len = strlen(str);
  size_t hash = FNV_OFFSET;
  for (char *c = str; *c != '\0'; c++) {
    hash ^= *c;
    hash *= FNV_PRIME;
  }
  return 0;
  return hash;
}
