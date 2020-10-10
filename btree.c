#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdbool.h>

#include "btree.h"
#include "util.h"

struct Btree {
	/* The maximum number of children of a non-leaf node. */
	size_t branch_child_count_max;
	/* The maximum number of entries stored in each leaf. */
	size_t leaf_entry_count_max;
	/* The size of each leaf entry, in bytes */
	size_t entry_size;
	/* The number of entries in the entire btree */
	size_t entry_count;

	Btree_Compare *compare;
	const void *compare_cb_data;

	struct Btree_Node *root;
};

struct Btree_Node {
	/*
	 * If the node has no children, it is a leaf node
	 */
	size_t child_count;
	size_t entry_count;

	/* Data is inserted at the end of the structure.
	 *
	 * For leaf nodes, the data is structured like this:
	 *
	 *     [entry][entry][entry][entry]...
	 *
	 * with `btree->leaf_entry_count_max` total elements, each
	 * `btree->entry_size` bytes in size.
	 *
	 * For branch nodes, the the first part of `data` is structured like
	 * this:
	 *
	 *     [struct Btree_Node *][key][struct Btree_Node *][key]...[struct Btree_Node *]
	 *
	 * with `btree->branch_child_count_max`-many child node pointers, and
	 * `btree->branch_child_count_max - 1`-many keys.  Each key is just a
	 * copy of an entry and is thus the same size as an entry
	 * (`btree->entry_size` bytes).  The second part of data is an array of
	 * `size_t` values representing the cumulative sums of the number of
	 * entries of the branch's children.
	 */
	alignas(max_align_t) uint8_t data[];
};

/*
 * Returns a pointer to an entry of a leaf node
 */
static inline void *
get_leaf_entry_ptr(const struct Btree /*restrict */*btree, const struct Btree_Node /*restrict */*leaf, size_t entry_index)
{
	return (void *) (leaf->data + entry_index * btree->entry_size);
}

/*
 * Returns a pointer to a child pointer of a branch node
 */
static inline struct Btree_Node **
get_branch_child_ptr_ptr(const struct Btree /*restrict */*btree, const struct Btree_Node /*restrict */*branch, size_t child_index)
{
	return (struct Btree_Node **) (branch->data + child_index * (sizeof(struct Btree_Node *) + btree->entry_size));
}

/*
 * Returns a pointer to a key in a branch node
 */
static inline void *
get_branch_key_ptr(const struct Btree /*restrict */*btree, const struct Btree_Node /*restrict */*branch, size_t key_index)
{
	return (void *) (branch->data + key_index * sizeof(struct Btree_Node *) + (key_index - 1) * btree->entry_size);
}

/*
 * Returns a pointer to an array of the cumulative sum of the sizes of each of
 * the branch's children
 */
static inline size_t *
get_branch_cumulative_sizes(const struct Btree /*restrict */*btree, const struct Btree_Node /*restrict */*branch)
{
	return (size_t *) (branch->data + btree->branch_child_count_max * sizeof(struct Btree_Node *) + (btree->branch_child_count_max - 1) * btree->entry_size);
}

/*
 * Compares the two specified entries and returns the result of the `btree->compare` callback.
 */
static inline int
compare(const struct Btree *btree, const void *a, const void *b)
{
	return btree->compare(a, b, btree->compare_cb_data);
}

/*
 * Creates a new leaf node
 */
static struct Btree_Node *
create_leaf(const struct Btree /*restrict */*btree, size_t entry_count)
{
	struct Btree_Node *leaf = xmalloc(
		/* Base struct */
		sizeof(struct Btree_Node) +
		/* Array of entries */
		btree->leaf_entry_count_max * btree->entry_size
	);
	leaf->child_count = 0;
	leaf->entry_count = entry_count;
	return leaf;
}

/*
 * Creates a new branch node
 */
static struct Btree_Node *
create_branch(const struct Btree /*restrict */*btree, size_t child_count, size_t entry_count)
{
	struct Btree_Node *branch = xmalloc(
		/* Base struct */
		sizeof(struct Btree_Node) +
		/* Keys and child pointers, intermixed */
		btree->branch_child_count_max * sizeof(struct Btree_Node *) + (btree->branch_child_count_max - 1) * btree->entry_size +
		/* Array containing the cumulative entry counts of each child of the branch (excluding the last one) */
		(btree->branch_child_count_max - 1) * sizeof(size_t)
	);
	branch->child_count = child_count;
	branch->entry_count = entry_count;
	return branch;
}

/*
 * Creates a new btree.  `branch_child_count_max` and `leaf_entry_count_max`
 * can be as small as 2, but in practice they should be at least an order of
 * magnitude larger than that.
 */
struct Btree *
btree_new(size_t branch_child_count_max, size_t leaf_entry_count_max, size_t entry_size, Btree_Compare *compare, const void *compare_cb_data)
{
	struct Btree *btree = xmalloc(sizeof(struct Btree));
	btree->leaf_entry_count_max = leaf_entry_count_max;
	btree->branch_child_count_max = branch_child_count_max;
	btree->entry_size = entry_size;
	btree->entry_count = 0;
	btree->compare = compare;
	btree->compare_cb_data = compare_cb_data;
	btree->root = create_leaf(btree, 0);
	return btree;
}

/*
 * Frees a node and all of its children (if it has any)
 */
static void
free_node(struct Btree /*restrict */*btree, struct Btree_Node /*restrict */*node)
{
	for (size_t i = 0; i < node->child_count; i++)
		free_node(btree, *get_branch_child_ptr_ptr(btree, node, i));
	free(node);
}

/*
 * Frees a btree
 */
void
btree_free(struct Btree *btree)
{
	free_node(btree, btree->root);
	free(btree);
}

/*
 * Conducts a binary search on a btree leaf.  Returns true if an exact match
 * was found, or false otherwise.  If no exact match was found, the index is
 * set to the index of the first entry greater than the target.  Otherwise,
 * `index` is set to the index of the matched entry.
 */
static bool
leaf_search(const struct Btree /*restrict */*btree, const struct Btree_Node /*restrict */*leaf, const void /*restrict */*target_entry, size_t /*restrict */*index)
{
	size_t low = 0;
	size_t high = leaf->entry_count;
	while (low != high) {
		size_t middle = (low + high) / 2;
		void *middle_entry = get_leaf_entry_ptr(btree, leaf, middle);
		int comparison = compare(btree, middle_entry, target_entry);
		if (comparison == 0) {
			*index = middle;
			return true;
		}
		if (comparison < 0)
			high = middle;
		else
			low = middle + 1;
	}
	*index = low;
	return false;
}

/*
 * Conducts a binary search on a btree branch.  If a key in the branch is an
 * exact match, the function returns a pointer to that key.  Otherwise, NULL
 * is returned, and `index` is set to the index of the first entry that
 * comes after the target.
 */
static void *
branch_search(const struct Btree /*restrict */*btree, const struct Btree_Node /*restrict */*branch, const void /*restrict */*target_entry, size_t /*restrict */*index)
{
	size_t low = 0;
	size_t high = branch->child_count - 1;
	while (low != high) {
		size_t middle = (low + high + 1) / 2;
		void *middle_key = get_branch_key_ptr(btree, branch, middle);
		int comparison = compare(btree, middle_key, target_entry);
		if (comparison == 0)
			return middle_key;
		if (comparison < 0)
			high = middle - 1;
		else
			low = middle;
	}
	*index = low;
	return NULL;
}

/*
 * Inserts an entry into a leaf, determining the index by conducting a binary
 * search.  The leaf MUST NOT be full when calling this function.
 */
static void
leaf_insert(const struct Btree /*restrict */*btree, struct Btree_Node /*restrict */*leaf, const void /*restrict */*entry)
{
	size_t insertion_index;
	if (leaf_search(btree, leaf, entry, &insertion_index))
		die("Found an exact match in a leaf.  That's not supposed to happen since insertions should never be duplicates.");
	memmove(get_leaf_entry_ptr(btree, leaf, insertion_index + 1), get_leaf_entry_ptr(btree, leaf, insertion_index), btree->entry_size * (leaf->entry_count - insertion_index));
	memcpy(get_leaf_entry_ptr(btree, leaf, insertion_index), entry, btree->entry_size);
	leaf->entry_count++;
}

/*
 * Inserts a child into a branch at the specified index.  The branch MUST
 * NOT be full when calling this function.  `child_index` MUST BE GREATER THAN
 * ZERO.
 */
static void
branch_insert(const struct Btree /*restrict */*btree, struct Btree_Node /*restrict */*branch, const void /*restrict */*key, size_t child_index, struct Btree_Node /*restrict */*child)
{
	memmove(get_branch_key_ptr(btree, branch, child_index + 1), get_branch_key_ptr(btree, branch, child_index), (branch->child_count - child_index) * (sizeof(struct Btree_Node *) + btree->entry_size));
	memcpy(get_branch_key_ptr(btree, branch, child_index), key, btree->entry_size);
	*get_branch_child_ptr_ptr(btree, branch, child_index) = child;
	branch->child_count++;

	/* Update cumulative sizes array */
	size_t *cumulative_sizes = get_branch_cumulative_sizes(btree, branch);
	for (size_t i = branch->child_count - 2; i > child_index; i--)
		cumulative_sizes[i] = cumulative_sizes[i - 1] + 1;
	if (child_index < btree->branch_child_count_max - 1)
		cumulative_sizes[child_index] = cumulative_sizes[child_index - 1] + child->entry_count;
}

/*
 * Gets a pointer to the first entry in a node
 */
static void *
get_first_entry_ptr(const struct Btree /*restrict */*btree, const struct Btree_Node /*restrict */*node)
{
	while (node->child_count != 0)
		node = *get_branch_child_ptr_ptr(btree, node, 0);
	return get_leaf_entry_ptr(btree, node, 0);
}

/*
 * Inserts an entry into a btree node.  If the node is a branch, then this
 * function will recurse on itself to find a leaf.  If the specified node
 * is full, then this function will split the node into two, and the newly
 * created node, which contains the upper half of the original node, will
 * be returned.  In that case, `*key` will be set to a pointer to the entry
 * that should be used as the key placed between the old node and the new node
 * in the branch containing them.  If no new node is created, then NULL is
 * returned.
 */
static struct Btree_Node *
node_insert(const struct Btree /*restrict */*btree, struct Btree_Node /*restrict */*node, const void /*restrict */*entry, const void /*restrict */*/*restrict */*key)
{
	if (node->child_count == 0) {
		/* If the leaf is full, split it in half */
		if (node->entry_count == btree->leaf_entry_count_max) {
			node->entry_count = btree->leaf_entry_count_max / 2;
			size_t middle_index = node->entry_count;
			void *middle_entry = get_leaf_entry_ptr(btree, node, middle_index);

			struct Btree_Node *new_leaf = create_leaf(btree, btree->leaf_entry_count_max - middle_index);
			void *leaf_entries_start = get_leaf_entry_ptr(btree, new_leaf, 0);
			memcpy(leaf_entries_start, middle_entry, new_leaf->entry_count * btree->entry_size);

			/* Now insert the new entry */
			struct Btree_Node *insertion_target = compare(btree, middle_entry, entry) < 0 ? node : new_leaf;
			leaf_insert(btree, insertion_target, entry);

			*key = leaf_entries_start;
			return new_leaf;
		}

		leaf_insert(btree, node, entry);
		return NULL;
	}

	size_t child_index;
	void *found_key = branch_search(btree, node, entry, &child_index);
	if (found_key != NULL)
		die("Found an exact match in a branch.  That's not supposed to happen since insertions should never be duplicates.");

	struct Btree_Node **child_ptr_ptr = get_branch_child_ptr_ptr(btree, node, child_index);
	struct Btree_Node *new_child = node_insert(btree, *child_ptr_ptr, entry, key);
	if (new_child != NULL) {
		/*
		 * The child node that this node passed on the insertion to had
		 * to be split, so now a new node needs to be added to this
		 * node.
		 */

		size_t new_child_index = child_index + 1;

		if (node->child_count == btree->branch_child_count_max) {
			/* This branch is full, so it must be split */

			size_t middle_index = btree->branch_child_count_max / 2;
			struct Btree_Node **middle_child_ptr_ptr = get_branch_child_ptr_ptr(btree, node, middle_index);
			node->child_count = middle_index;

			/*
			 * Create a new branch and copy its child pointers and
			 * keys from the other branch (`node`)
			 */
			struct Btree_Node *new_branch = create_branch(btree, btree->branch_child_count_max - middle_index, 0);
			struct Btree_Node **new_branch_first_child_ptr_ptr = get_branch_child_ptr_ptr(btree, new_branch, 0);
			memcpy(new_branch_first_child_ptr_ptr, middle_child_ptr_ptr, new_branch->child_count * sizeof(struct Btree_Node *) + (new_branch->child_count - 1) * btree->entry_size);

			/* Update entry counts */
			size_t tmp = node->entry_count;
			node->entry_count = get_branch_cumulative_sizes(btree, node)[middle_index - 1];
			new_branch->entry_count = tmp - node->entry_count;

			/* Set cumulative entry count array for the new branch */
			for (size_t i = 0; i < new_branch->child_count - 1; i++)
				get_branch_cumulative_sizes(btree, new_branch)[i] = get_branch_cumulative_sizes(btree, node)[middle_index + i] - node->entry_count;

			/*
			 * Now insert the new child, `new_child`, into the same
			 * branch as `*child_ptr_ptr`
			 */
			if (child_index < middle_index) {
				/* Insert into `node` */

				size_t *cumulative_sizes = get_branch_cumulative_sizes(btree, node);
				cumulative_sizes[child_index] = (*child_ptr_ptr)->entry_count;
				if (child_index > 0)
					cumulative_sizes[child_index] += cumulative_sizes[child_index - 1];

				branch_insert(btree, node, *key, new_child_index, new_child);

				/*
				 * We haven't incremented the entry count from
				 * the insertion yet, so do it now.
				 */
				node->entry_count++;
			} else {
				/* Insert into `new_branch` */

				size_t new_branch_child_index = child_index - middle_index;
				size_t new_branch_new_child_index = new_child_index - middle_index;

				size_t *cumulative_sizes = get_branch_cumulative_sizes(btree, new_branch);
				cumulative_sizes[new_branch_child_index] = (*child_ptr_ptr)->entry_count;
				if (new_branch_child_index > 0)
					cumulative_sizes[new_branch_child_index] += cumulative_sizes[new_branch_child_index - 1];

				branch_insert(btree, new_branch, *key, new_branch_new_child_index, new_child);

				/*
				 * We haven't incremented the entry count from
				 * the insertion yet, so do it now.
				 */
				new_branch->entry_count++;
			}

			*key = get_first_entry_ptr(btree, new_branch);
			return new_branch;
		}

		size_t *cumulative_sizes = get_branch_cumulative_sizes(btree, node);
		cumulative_sizes[child_index] = (*child_ptr_ptr)->entry_count;
		if (child_index > 0)
			cumulative_sizes[child_index] += cumulative_sizes[child_index - 1];

		/* Insert new child */
		branch_insert(btree, node, *key, new_child_index, new_child);
	} else {
		/* Update the cumulative sizes array */
		for (size_t i = child_index; i < node->child_count - 1; i++) {
			get_branch_cumulative_sizes(btree, node)[i]++;
		}
	}
	node->entry_count++;
	return NULL;
}

/*
 * Inserts an entry into a btree
 */
void
btree_insert(struct Btree /*restrict */*btree, const void /*restrict */*entry)
{
	const void *key;
	struct Btree_Node *new_node = node_insert(btree, btree->root, entry, &key);
	if (new_node != NULL) {
		/*
		 * The root was full and had to be split.  Construct a new root
		 * that contains the original root and the new node.
		 */
		struct Btree_Node *old_root = btree->root;
		btree->root = create_branch(btree, 2, old_root->entry_count + new_node->entry_count);
		*get_branch_child_ptr_ptr(btree, btree->root, 0) = old_root;
		memcpy(get_branch_key_ptr(btree, btree->root, 1), key, btree->entry_size);
		*get_branch_child_ptr_ptr(btree, btree->root, 1) = new_node;
		get_branch_cumulative_sizes(btree, btree->root)[0] = old_root->entry_count;
	}
	btree->entry_count++;
}

/*
 * Writes `i`-many tab characters to stdout
 */
static void
indent(int i)
{
	while (i > 0) {
		printf("\t");
		i--;
	}
}

/*
 * Prints out the contents of a node and its children
 */
static void
display_node(const struct Btree /*restrict */*btree, const struct Btree_Node /*restrict */*node, int depth)
{
	indent(depth);
	if (node->child_count == 0) {
		printf("%p -> [%lu entries]{ ", (void *) node, node->entry_count);
		for (size_t i = 0; i < node->entry_count; i++)
			printf("%lu ", *((uint64_t *) get_leaf_entry_ptr(btree, node, i)));
		printf("}\n");
	} else {
		printf("%p -> [%lu children, %lu entries]{\n", (void *) node, node->child_count, node->entry_count);
		for (size_t i = 0; i < node->child_count; i++) {
			if (i != 0) {
				indent(depth + 1);
				printf("(%lu)\n", *((uint64_t *) get_branch_key_ptr(btree, node, i)));
			}
			display_node(btree, *get_branch_child_ptr_ptr(btree, node, i), depth + 1);
			if (i + 1 < node->child_count) {
				indent(depth + 1);
				printf("[%lu cumulative entries]\n", get_branch_cumulative_sizes(btree, node)[i]);
			}
		}
		indent(depth);
		printf("}\n");
	}
}

/*
 * Prints out the contents of a btree in a human-readable format.  For debugging purposes.
 */
void
btree_display(const struct Btree *btree)
{
	printf("Btree contains %lu entries\n", btree->entry_count);
	display_node(btree, btree->root, 0);
}
