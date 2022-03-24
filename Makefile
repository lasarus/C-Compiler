CC=gcc
CFLAGS=-Wall -Wextra -pedantic -g -Isrc/

all: cc

clean:
	rm -f cc

cc: config.h src/*.c src/*/*.c
	 $(CC) -o $@ src/*.c src/*/*.c $(CFLAGS)

config.h:
	cp config.def.h $@
