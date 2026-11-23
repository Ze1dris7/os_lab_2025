CC = gcc
CFLAGS ?= -std=gnu11 -O2 -Wall -Wextra

.PHONY: all clean
all: client server

client: client.o libcommon.a
	$(CC) $(LDFLAGS) -o $@ $^ -pthread $(LDLIBS)

server: server.o libcommon.a
	$(CC) $(LDFLAGS) -o $@ $^ -pthread $(LDLIBS)

libcommon.a: common.o
	$(AR) rcs $@ $^

client.o server.o common.o: common.h

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -c $< -o $@

clean:
	$(RM) client server client.o server.o common.o libcommon.a
