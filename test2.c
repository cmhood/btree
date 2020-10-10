#include <stdio.h>
#include <string.h>

#include "util.h"
#include "btree.h"

#include "test_data/people.h"

static int
compare(const void *void_a, const void *void_b, const void *data)
{
	(void) data;
	const struct Person *a = void_a;
	const struct Person *b = void_b;
	return -strcmp(a->name, b->name);
}

static void
display_person(const struct Person *person)
{
	printf("{ \"%s\", %d }", person->name, person->age);
}

static void
display(const void *ptr)
{
	display_person(ptr);
}

int
main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "Invalid argc\n");
		return EXIT_FAILURE;
	}

	size_t branch_size = atol(argv[1]);
	size_t leaf_size = atol(argv[2]);
	size_t count = atol(argv[3]);
	if (branch_size < 4 || leaf_size < 2 || !(0 <= count && count <= COUNT_OF(test_people))) {
		fprintf(stderr, "Invalid argv\n");
		return EXIT_FAILURE;
	}

	struct Btree *btree = btree_new(branch_size, leaf_size, sizeof(struct Person), compare, NULL);
	for (size_t i = 0; i < count; i++)
		btree_insert(btree, &test_people[i]);

	btree_display(btree, display);

	for (size_t i = 0; i < count; i++) {
		size_t count;
		const struct Person *person = btree_fetch(btree, i, &count);
		printf("[%lu] = ", i);
		display_person(person);
		printf("    (%lu contiguous)\n", count);
	}

	btree_free(btree);
}
