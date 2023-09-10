// This is the implementation of the generic hash table ADT with binary search 
//   trees.

#include <stdlib.h>
#include "hashtable.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

// -----------------------------------------------------------------------
// DO NOT CHANGE THESE
// See hashtable.h for documentation
const int HT_SUCCESS        = 0;
const int HT_ALREADY_STORED = 1;
const int HT_NOT_STORED     = 2;
// -----------------------------------------------------------------------

// a generic bstnode
struct bstnode {
  void *key;
  int level;
  struct bstnode *left;
  struct bstnode *right;
};

// a generic BST
struct bst {
  struct bstnode *root;
};

struct hashtable {
  struct bst **table;
  int hash_len;                                 
  int ht_len;                                       // number of items in the table (array)
  int (*hash_func)(const void *, int);              // hash function
  void *(*key_clone)(const void *);                 // function that returns a pointer to a copy of the key
  int (*key_compare)(const void *, const void *);   // comparison function for void pointers
  void (*key_destroy)(void *);                      // free memory allocated for the key
  void (*key_print)(const void *);                  // print the key

};

// HELPER FUNCTION DECLERATIONS START ----------------------------------

static void bst_destroy(struct bst *bst, void (*key_destroy)(void *));
static void free_bstnode(struct bstnode *node, void (*key_destroy)(void *));
static struct bst *bst_create(void);
static int bst_insert(const void *key, struct bst *b, 
                                        int (*key_compare)(const void *, const void *),
                                        void *(*key_clone)(const void *));
static struct bstnode *new_leaf(const void *key, int counter, 
                                            void *(*key_clone)(const void *));
static int bst_remove(const void *key, struct bst *b, 
                                      int (*key_compare)(const void *, const void *),
                                      void (*key_destroy)(void *));
static void update_level(struct bstnode *parent);
static void bstnode_print(struct bstnode *node, bool *first, 
                                                void (*key_print)(const void *));
static void bstnodes_print(struct bstnode *node, bool *first, 
                                                void (*key_print)(const void *));
static void bst_print (struct bst *b, void (*key_print)(const void *));
static int pwr(int n);

// HELPER FUNCTION DECLERATIONS END ------------------------------------
// documentation for helper functions is available at location of definition

struct hashtable *ht_create(void *(*key_clone)(const void *),
                            int (*hash_func)(const void *, int), 
                            int hash_length,
                            int (*key_compare)(const void *, const void *),
                            void (*key_destroy)(void *),
                            void (*key_print)(const void *)) {
  assert(key_clone);
  assert(hash_func);
  assert(hash_length > 0);
  assert(key_compare);
  assert(key_destroy);
  assert(key_print);

  // allocate space for a structure to return (caller must destroy)
  struct hashtable *ht = malloc(sizeof(struct hashtable));

  // set the hash length and the hash table length
  ht->hash_len = hash_length;
  ht->ht_len = pwr(hash_length);

  // set the hash table functions
  ht->hash_func = hash_func;
  ht->key_clone = key_clone;
  ht->key_compare = key_compare;
  ht->key_destroy = key_destroy;
  ht->key_print = key_print;

  // allocate memory for the table and set all the BSTs to NULL
  ht->table = malloc(sizeof(struct bst) * ht->ht_len);
  for (int i = 0; i < ht->ht_len; i++) {
    ht->table[i] = NULL;
  }

  return ht;
}

void ht_destroy(struct hashtable *ht) {
  assert(ht);
  for (int i = 0; i < ht->ht_len; i++) {
    if (ht->table[i]) {
      bst_destroy(ht->table[i], ht->key_destroy);
    }
  }
  free(ht->table);
  free(ht);
}

int ht_insert(struct hashtable *ht, const void *key) {
  assert(key);
  assert(ht);
  const int index = ht->hash_func(key, ht->hash_len);
  if (ht->table[index] == NULL) {
    ht->table[index] = bst_create();
  }

  return bst_insert(key, ht->table[index], ht->key_compare, ht->key_clone);
}

int ht_remove(struct hashtable *ht, const void *key) {
  assert(ht);
  assert(key);

  const int index = ht->hash_func(key, ht->hash_len);
  
  if (ht->table[index] == NULL) {
    return HT_NOT_STORED;
  }
  return bst_remove(key, ht->table[index], ht->key_compare, ht->key_destroy);
}

void ht_print(const struct hashtable *ht) {
  assert(ht);
  for (int i = 0; i < ht->ht_len; i++) {
    printf("%d: ", i);
      printf("[");
    if (ht->table[i]) {
      bst_print(ht->table[i], ht->key_print);
    }
    printf("]\n");
  }
}


// HELPER FUNCTION DEFINITIONS START HERE -----------------------------------------------

// bst_destroy(bst) is a helper function that frees all memory allocated within a BST
// requires: all pointers are valid
// effects: frees memory
// time: O(m * ds) where m is the number of nodes in the bst and ds is the time complexity
//  of the key_destroy function
static void bst_destroy(struct bst *bst, void (*key_destroy)(void *)) {
  assert(bst);
  assert(key_destroy);
  free_bstnode(bst->root, key_destroy);
  free(bst);
}

// free_bstnode is a helper function that frees all memory allocated withing the node
//  and all of its subnodes
// requires: key_destroy is a valid pointer
// effects: frees memory
// time: O(m * ds) where m is the number of subnodes in node + 1 ds is the time complexity
//  of the key_destroy function
static void free_bstnode(struct bstnode *node, void (*key_destroy)(void *)) {
  assert(key_destroy);
  
  if (node) {
    free_bstnode(node->left, key_destroy);
    free_bstnode(node->right, key_destroy);
    key_destroy(node->key);
    free(node);
  }
}

// bst_create() is a helper function that returns a pointer to an empty BST
// effects: allocates memory (caller must call bst_destroy)
// time: O(1)
static struct bst *bst_create(void) {
  struct bst *b = malloc(sizeof(struct bst));
  b->root = NULL;
  return b;
}

// bst_insert(key, b, key_compare, key_clone) is a helper function that adds the key
//  into the bst b
// requires: all pointers are valid
// effects: allocates memory (must call bst_destroy)
//          modifies b
// time: O(cl + m * co) where cl is the time complexity of key_clone, m is the number
//  of items in bst, and co is the time complexity of key_compare
static int bst_insert(const void *key, struct bst *b, 
                                        int (*key_compare)(const void *, const void *),
                                        void *(*key_clone)(const void *)) {
  assert(key);
  assert(b);
  assert(key_compare);
  assert(key_clone);
  struct bstnode *node = b->root;
  struct bstnode *parent = NULL;
  int counter = 0;

  while (node && (key_compare(key, node->key) != 0)) {
    parent = node;
    if (key_compare(key, node->key) < 0) {
      node = node->left;
      counter++;
    } 
    else {
      node = node->right;
      counter++;
    }
  }
  if(node) {
    return HT_ALREADY_STORED;
  }
  else if(parent == NULL) {
    b->root = new_leaf(key, counter, key_clone);
    return HT_SUCCESS;
  }
  else if((key_compare(key, parent->key) < 0)) {
    parent->left = new_leaf(key, counter, key_clone);
    return HT_SUCCESS;
  }
  else {
    parent->right = new_leaf(key, counter, key_clone);
    return HT_SUCCESS;
  }
}

// new_leaf(key, counter, key_clone) is a helper function that returns a pointer
//  to a leaf node with the given key
// requires: all pointers are valid
// effects: allocates memory (caller must call bst_destroy)
// time: O(cl) where cl is the time complexity of key_clone
static struct bstnode *new_leaf(const void *key, const int counter, 
                                            void *(*key_clone)(const void *)){
  assert(key);
  assert(key_clone);

  struct bstnode *leaf = malloc(sizeof(struct bstnode));
  leaf->level = counter;
  leaf->key = key_clone(key);
  leaf->left = NULL;
  leaf->right = NULL;
  return leaf;
}

// update_level(parent) is a helper function that reduces the level of parent and
//  every subnode in it by 1
// effects: modifies parent
// time: O(m) where m is the number of nodes in parent + 1
static void update_level(struct bstnode *parent) {
  if (parent) {
    parent->level--;
    update_level(parent->left);
    update_level(parent->right);
  }
}

// bst_remove(key, b, key_compare, key_destroy) is a helper function that removes 
//  the key from the BST, and returns an error code if the key was not found
// requires: all pointers are valid
// effects: frees memory
//          modifies b
// time: O(m * co + ds) where m is the number of items in bst, co is the time
//  complexity of key_compare and ds is the time complexity of key_destroy
static int bst_remove(const void *key, struct bst *b, 
                                      int (*key_compare)(const void *, const void *),
                                      void (*key_destroy)(void *)) {
  assert(key);
  assert(b);
  assert(key_compare);
  assert(key_destroy);
  
  struct bstnode *target = b->root;
  struct bstnode *target_parent = NULL;
  // find the target (and its parent)
  while (target && (key_compare(key, target->key) != 0)) {
    target_parent = target;
    if ((key_compare(key, target->key) < 0)) {
      target = target->left;
    } else {
      target = target->right;
    }
  }

  if (target == NULL) {
    return HT_NOT_STORED; // key not found
  }

  // find the node to "replace" the target
  struct bstnode *replacement = NULL;
  if (target->left == NULL) {
    replacement = target->right;
    update_level(replacement);
  } else if (target->right == NULL) {
    replacement = target->left;
    update_level(replacement);
  } else {
    // neither child is NULL:
    // find the replacement node and its parent
    replacement = target->right;
    struct bstnode *replacement_parent = target;
    while (replacement->left) {
      replacement_parent = replacement;
      replacement = replacement->left;
    }
    // update the child links for the replacement and its parent
    replacement->left = target->left;
    update_level(replacement->right);
    if (replacement_parent != target) {
      replacement_parent->left = replacement->right;
      replacement->right = target->right;
    }
    replacement->level = target->level;
  }

  // free the target, and update the target's parent
  key_destroy(target->key);
  free(target);
  if (target_parent == NULL) {
    b->root = replacement;
  } else if ((key_compare(key, target_parent->key) > 0)) {
    target_parent->right = replacement;
  } else {
    target_parent->left = replacement;
  }
  return HT_SUCCESS;
}

// bstnode_print(node, first, key_print) is a helper function that prints a node followed by a comma
//  if it is not the first node to be printed in the tree
// requires: all pointers are valid
// effects: produces output
// time: O(cp) where cp is the time complexity of key_print
static void bstnode_print(struct bstnode *node, bool *first, 
                                                void (*key_print)(const void *)) {
  assert(node);
  assert(first);
  assert(key_print);
  
  if (*first) {
    *first = false;
  } else {
    printf(",");
  }
  printf("%d-", node->level);
  key_print(node->key);
}

// bstnodes_print(node, first) prints the sub-tree rooted at node in
//   in order from smallest to largest. Procced by a comma if not *first, 
//   otherwise updates *first.
// requires : first and key_print are valid pointers
// effects : prints output, may mutate first
// time : O(m * cp) where m is the number of subnodes in node + 1 and cp
//  is the time complexity of key_print
static void bstnodes_print(struct bstnode *node, bool *first, 
                                                void (*key_print)(const void *)) { 
  assert(first);
  assert(key_print);
  if (node) {
    bstnodes_print(node->left, first, key_print);
    bstnode_print(node, first, key_print);
    bstnodes_print(node->right, first, key_print);
  }
}

// bst_print(t) is a helper function that prints a BST
// requires: all pointers are valid
// effects: produces output
// time: O(m * cp) where m is the number of items in b and cp
//  is the time complexity of key_print
static void bst_print (struct bst *b, void (*key_print)(const void *)) {
  assert(b);  
  assert(key_print);
  bool first = true;
  bstnodes_print(b->root, &first, key_print);
}

// pwr(n) is a helper function that returns 2^n
// requires: n > 0
// time: O(n)
static int pwr(int n) {
  assert(n > 0);
  int val = 2;
  for (int i = 1; i < n; i++) {
    val *= 2;
  }
  return val;
}