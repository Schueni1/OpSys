
# @file ProcDB
# @author Aaron Duxler 1427540 <e1427540@student.tuwien.ac.at>
# @date 20.05.2017

CC	= gcc
#DEFS    = -D_DEFAULT_SOURCE
DEFS    = -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809
CFLAGS  = -Wall -g -std=c99 -pedantic $(DEFS)

.PHONY: all clean documentation

all: clean server client

server:  server.o
	$(CC) -o $@ $^ -lrt -pthread

client: client.o
	$(CC) -o $@ $^ -lrt -pthread

documentation : ./doc/Doxyfile
	doxygen $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f server
	rm -f server.o
	rm -f client
	rm -f client.o
	rm -f /dev/shm/01427540_shm
	rm -f /dev/shm/sem.01427540_sem_1
	rm -f /dev/shm/sem.01427540_sem_2
