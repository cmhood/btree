#include <stdio.h>

#include "util.h"
#include "btree.h"

#include "data/numbers.h"

static int
compare(const void *void_a, const void *void_b, const void *data)
{
	(void) data;
	const uint64_t *a = void_a;
	const uint64_t *b = void_b;
	return *b - *a;
}

static void
display(const void *entry)
{
	const uint64_t *num = entry;
	printf("%lu", *num);
}

int
main(int argc, char **argv)
{
	if (!(4 <= argc && argc <= 5)) {
		fprintf(stderr, "Invalid argc\n");
		return EXIT_FAILURE;
	}

	size_t branch_size = atol(argv[1]);
	size_t leaf_size = atol(argv[2]);
	size_t count = atol(argv[3]);
	if (branch_size < 2 || leaf_size < 2 || !(0 < count && count <= (size_t) 65536 * 65536)) {
		fprintf(stderr, "Invalid argv\n");
		return EXIT_FAILURE;
	}

	struct Btree *btree = btree_new(branch_size, leaf_size, sizeof(uint64_t), compare, NULL);
	for (size_t i = 0; i < count; i++) {
		uint64_t nr = (test_numbers[i >> 16] << 16) + test_numbers[i % ((size_t) 1 << 16)];
		btree_insert(btree, &nr);
	}

	if (argc != 5)
		btree_display(btree, display);

	btree_free(btree);
}
