#include <stdio.h>
#include <string.h>

#include "util.h"

void *
xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	return ptr;
}

noreturn void
die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	memset((void *) 1, 0, 1);
	exit(EXIT_FAILURE);
}
