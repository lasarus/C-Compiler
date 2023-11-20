#!/bin/bash

# I can't describe how happy I am to finally be able to write this script.

mkdir -p asm
mkdir -p asm2

echo "COMPILING FIRST GENERATION..."
SOURCES=$(find src -name '*.c')

set -e

for SRC in $SOURCES
do
	echo -en "\r\033[KCOMPILING $SRC"
	OUT="$(basename -s .c $SRC).s"
	bin/cc $SRC -o asm/$OUT -S -Isrc
done
echo -en "\r\033[KDONE!"
echo

# GCC is used for assembling and linking.
# No C code is compiled by GCC.
gcc asm/*.s -o cc_self -no-pie -g

[ ! -z "$1" ] && [ "1" -eq "$1" ] && exit

NUM=1
while true
do
	NUM=$((NUM+1))
	echo "COMPILING GENERATION #$NUM..."

	for SRC in $SOURCES
	do
		echo -en "\r\033[KCOMPILING $SRC"
		OUT="$(basename -s .c $SRC).s"
		./cc_self $SRC -o asm2/$OUT -S -Isrc
	done
	echo -en "\r\033[KDONE!"
	echo

	gcc asm2/*.s -o cc_self -no-pie -g

	[ ! -z "$1" ] && [ "$NUM" -eq "$1" ] && break
done
