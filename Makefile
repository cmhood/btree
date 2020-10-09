.POSIX:

PROG_CFLAGS=-D_DEFAULT_SOURCE ${CFLAGS}
PROG_LDFLAGS=${LDFLAGS}

OBJ=btree.o test.o util.o

default: test

btree.o: btree.h util.h
test.o: btree.h util.h
util.o: util.h

.c.o:
	${CC} -c $< -o $@ ${PROG_CFLAGS}

test: ${OBJ}
	${CC} ${OBJ} -o $@ ${PROG_LDFLAGS}

clean:
	rm -f test *.o

.PHONY: default clean
