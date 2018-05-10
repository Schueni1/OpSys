
# @file schedule
# @author Aaron Duxler 1427540 <e1427540@student.tuwien.ac.at>
# @date 15.04.2017

CC	= gcc
DEFS    = -D_DEFAULT_SOURCE
CFLAGS  = -Wall -g -std=c99 -pedantic $(DEFS)

.PHONY: all clean

all: clean rventgas rshutdown schedule

rventgas:  rventgas.o
	$(CC) -o $@ $^

rshutdown: rshutdown.o
	$(CC) -o $@ $^

schedule: schedule.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f rventgas
	rm -f rventgas.o
	rm -f rshutdown
	rm -f rshutdown.o
	rm -f schedule
	rm -f schedule.o
