#!/bin/bash

# I can't describe how happy I am to finally be able to write this script.

mkdir -p asm
mkdir -p asm2

MUSL=true

if [ ! -d "musl" ] 
then
	echo "Musl headers not provided, trying /usr/include instead"
	MUSL=false
fi

echo "COMPILING FIRST GENERATION..."
SOURCES=$(find src -name '*.c')

set -e

for SRC in $SOURCES
do
	echo -en "\r\033[KCOMPILING $SRC"
	OUT="$(basename -s .c $SRC).s"
	if [ "$MUSL" = "true" ]
	then
		./cc $SRC asm/$OUT -Isrc -Imusl
	else
		./cc $SRC asm/$OUT -Isrc -I/usr/include/ -Iinclude/
	fi
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
		if [ "$MUSL" = "true" ]
		then
			./cc_self $SRC asm2/$OUT -Isrc -Imusl
		else
			./cc_self $SRC asm2/$OUT -Isrc -I/usr/include/ -Iinclude/
		fi
	done
	echo -en "\r\033[KDONE!"
	echo

	gcc asm2/*.s -o cc_self -no-pie -g

	[ ! -z "$1" ] && [ "$NUM" -eq "$1" ] && break
done
