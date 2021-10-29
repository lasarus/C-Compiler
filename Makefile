CC=gcc
CFLAGS=-Wall -Wextra -pedantic -g -Isrc/

all: cc

clean:
	rm -f cc

cc: src/*.c src/parser/*.c src/preprocessor/*.c src/codegen/*.c src/arch/*.c src/ir/*.c
	 $(CC) -o $@ $^ $(CFLAGS)
