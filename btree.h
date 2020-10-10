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

typedef void Btree_Display_Entry(const void *);

struct Btree *btree_new(size_t, size_t, size_t, Btree_Compare *, const void *);
void btree_free(struct Btree *);

void btree_insert(struct Btree *, const void *);

const void *btree_fetch(const struct Btree *, size_t, size_t *);

void btree_display(const struct Btree *, Btree_Display_Entry *);

#endif
