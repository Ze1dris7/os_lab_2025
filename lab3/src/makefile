CC = gcc
CFLAGS = -Wall -Wextra -O2 -I.

.PHONY: all clean

all: sequential_min_max parallel_min_max run_sequential

sequential_min_max: sequential_min_max.c utils.o find_min_max.o utils.h find_min_max.h
	$(CC) $(CFLAGS) sequential_min_max.c utils.o find_min_max.o -o sequential_min_max

parallel_min_max: parallel_min_max.c utils.o find_min_max.o utils.h find_min_max.h
	$(CC) $(CFLAGS) parallel_min_max.c utils.o find_min_max.o -o parallel_min_max

run_sequential: run_sequential.c sequential_min_max
	$(CC) $(CFLAGS) run_sequential.c -o run_sequential

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c -o utils.o

find_min_max.o: find_min_max.c find_min_max.h utils.h
	$(CC) $(CFLAGS) -c find_min_max.c -o find_min_max.o

clean:
	rm -f utils.o find_min_max.o sequential_min_max parallel_min_max run_sequential