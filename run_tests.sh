#!/bin/bash

mkdir -p test_asm

SOURCES=$(find tests -name '*.c')

set -e

echo "TESTING SOURCES IN tests/"
for SRC in $SOURCES
do
	echo -en "\r\033[KTESTING $SRC"
	OUT="$(basename -s .c $SRC).s"
	./cc $SRC test_asm/$OUT -Imusl -Isrc -D_POSIX_SOURCE
	gcc test_asm/$OUT -o test -no-pie
	./test
done
echo -en "\r\033[KNo errors"
echo

echo "TESTING SELF COMPILATION"
./self_compile.sh 2
diff asm/ asm2/
echo "No errors"
