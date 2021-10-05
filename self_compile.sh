#!/bin/bash

# I can't describe how happy I am to finally be able to write this script.

mkdir -p asm
mkdir -p asm2

if [ ! -d "musl" ] 
then
    echo "Please put the musl headers into a directory called musl/"
	exit 1
fi

echo "COMPILING FIRST GENERATION..."
SOURCES=$(find src -name '*.c')

set -e

for SRC in $SOURCES
do
	echo -en "\r\033[KCOMPILING $SRC"
	OUT="$(basename -s .c $SRC).s"
	./cc $SRC asm/$OUT -Imusl -Isrc -DNDEBUG -D_POSIX_SOURCE -D__STRICT_ANSI__
done
echo -en "\r\033[KDONE!"
echo

# GCC is used for assembling and linking.
# No C code is compiled by GCC.
gcc asm/*.s -o cc_self -no-pie -g

NUM=1
while true
do
	NUM=$((NUM+1))
	echo "COMPILING GENERATION #$NUM..."

	for SRC in $SOURCES
	do
		echo -en "\r\033[KCOMPILING $SRC"
		OUT="$(basename -s .c $SRC).s"
		./cc_self $SRC asm2/$OUT -Imusl -Isrc -DNDEBUG -D_POSIX_SOURCE -D__STRICT_ANSI__
	done
	echo -en "\r\033[KDONE!"
	echo

	gcc asm2/*.s -o cc_self -no-pie -g
done
