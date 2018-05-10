CC      = gcc
DEFS    = -D_XOPEN_SOURCE=500 -D_DEFAULT_SOURCE
CFLAGS  = -Wall -g -std=c99 -pedantic $(DEFS)

.PHONY: all clean

all: server client

avarage: average.o
	$(CC) -o $@ $^

server: server.o
	$(CC) -o $@ $^

client: client.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f server client
	rm -f server.o client.o
