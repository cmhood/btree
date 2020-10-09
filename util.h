#ifndef _UTIL_H
#define _UTIL_H

#include <stdlib.h>
#include <stdnoreturn.h>

#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

void *xmalloc(size_t);
noreturn void die(const char *);

#endif
