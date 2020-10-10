.POSIX:

PROG_CFLAGS=-D_DEFAULT_SOURCE ${CFLAGS}
PROG_LDFLAGS=${LDFLAGS}

OBJ=btree.o util.o

default: test1 test2

btree.o: btree.h util.h
test1.o: btree.h util.h
test2.o: btree.h util.h
util.o: util.h

.c.o:
	${CC} -c $< -o $@ ${PROG_CFLAGS}

test1: test1.o ${OBJ}
	${CC} test1.o ${OBJ} -o $@ ${PROG_LDFLAGS}

test2: test2.o ${OBJ}
	${CC} test2.o ${OBJ} -o $@ ${PROG_LDFLAGS}

clean:
	rm -f test1 test2 *.o

.PHONY: default clean
