CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O2

all: test

test: memory.o test_memory.o
	$(CC) $(CFLAGS) -o $@ $^

memory.o: memory.c memory.h
	$(CC) $(CFLAGS) -c -o $@ memory.c

test_memory.o: test_memory.c memory.h
	$(CC) $(CFLAGS) -c -o $@ test_memory.c

clean:
	rm -f *.o test

.PHONY: all clean
