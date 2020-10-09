#ifndef _BTREE_H
#define _BTREE_H

#include <stdint.h>

struct Btree;

/*
 * Comparison function should return 0 if the second argument matches the first
 * argument, a negative value if the second argument comes before the first
 * argument, and a positive value if the second argument comes after the first
 * argument.
 */
typedef int Btree_Compare(const void *, const void *, const void *);

struct Btree *btree_new(size_t, size_t, size_t, Btree_Compare *, const void *);
void btree_free(struct Btree *);
void *btree_search(struct Btree *, void *);
void btree_insert(struct Btree *, const void *);
void btree_remove(struct Btree *, void *);

void btree_display(const struct Btree *);

#endif
