CC	= gcc
DEFS    = -D_XOPEN_SOURCE=500 -D_DEFAULT_SOURCE
CFLAGS  = -Wall -g -std=c99 -pedantic $(DEFS)

all: ispalindrom

ispalindrom:  ispalindrom.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f ispalindrom
	rm -f ispalindrom.o
