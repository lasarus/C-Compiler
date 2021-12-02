CC=gcc
CFLAGS=-Wall -Wextra -pedantic -g -Isrc/

all: cc

clean:
	rm -f cc

cc: src/*.c src/*/*.c
	 $(CC) -o $@ $^ $(CFLAGS)
