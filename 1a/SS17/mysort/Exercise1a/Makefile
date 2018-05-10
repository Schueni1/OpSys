
# @file mysort
# @author Aaron Duxler 1427540 <e1427540@student.tuwien.ac.at>
# @date 15.03.2015

CC	= gcc
DEFS    = -D_DEFAULT_SOURCE
CFLAGS  = -Wall -g -std=c99 -pedantic $(DEFS)

.PHONY: all documentation clean

all: clean mysort

documentation : ./doc/Doxyfile
	doxygen $<

mysort:  mysort.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f mysort
	rm -f mysort.o
