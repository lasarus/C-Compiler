CC=gcc
CFLAGS=-Wall -Wextra -pedantic -g -Isrc/
#CFLAGS=-Wall -Wextra -pedantic -g -Isrc/ -fsanitize=address,undefined,pointer-subtract,pointer-compare

all: cc

clean:
	rm -f cc

cc: src/*.c src/*/*.c
	 $(CC) -o $@ $^ $(CFLAGS)
