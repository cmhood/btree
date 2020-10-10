#include <stdio.h>
#include <time.h>

#include "util.h"
#include "btree.h"

#include "numbers.h"

static int
compare(const void *a, const void *b, const void *cb_data)
{
	(void) cb_data;
	return *((uint64_t *) b) - *((uint64_t *) a);
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

	srand(time(0));

	struct Btree *btree = btree_new(branch_size, leaf_size, sizeof(uint64_t), compare, NULL);
	for (size_t i = 0; i < count; i++) {
		uint64_t nr = (test_numbers[i >> 16] << 16) + test_numbers[i & 65535];
		btree_insert(btree, &nr);
	}

	if (argc == 5)
		btree_display(btree);

	btree_free(btree);
}
