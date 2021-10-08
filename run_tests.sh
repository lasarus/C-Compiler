#!/bin/bash

MUSL=true

if [ ! -d "musl" ] 
then
	echo "Musl headers not provided, trying /usr/include instead"
	MUSL=false
	#STDC_FLAGS=-I/usr/include -Iinclude -Isrc
fi

mkdir -p test_asm

SOURCES=$(find tests -name '*.c')

set -e

echo "TESTING SOURCES IN tests/"
for SRC in $SOURCES
do
	echo -en "\r\033[KTESTING $SRC"
	OUT="$(basename -s .c $SRC).s"
	if [ "$MUSL" = "true" ]
	then
		./cc $SRC test_asm/$OUT -Imusl -Isrc -D_POSIX_SOURCE
	else
		./cc $SRC test_asm/$OUT -I/usr/include -Iinclude -Isrc
	fi
	gcc test_asm/$OUT -o test -no-pie
	./test
done
echo -en "\r\033[KNo errors"
echo

echo "TESTING SELF COMPILATION"
./self_compile.sh 2
diff asm/ asm2/
echo "No errors"
