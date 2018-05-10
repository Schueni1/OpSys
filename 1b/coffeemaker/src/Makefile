
# @file coffeemaker
# @author Aaron Duxler 1427540 <e1427540@student.tuwien.ac.at>
# @date 25.03.2017

CC	= gcc
DEFS    = -D_DEFAULT_SOURCE
CFLAGS  = -Wall -g -std=c99 -pedantic $(DEFS)

.PHONY: all documentation clean

all: clean client server

client:  client.o
	$(CC) -o $@ $^

server: server.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f server
	rm -f server.o
	rm -f client
	rm -f client.o
