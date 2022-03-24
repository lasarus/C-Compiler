#!/bin/bash

test_source() {
	OUT="$(basename -s .c $1).s"

	if [ "$3" == "true" ]; then
		$4 $1 -o $TEST_DIR/$OUT -S -D$2

		musl-gcc $TEST_DIR/$OUT -o test -no-pie -lm -Wall -Werror
		./test
	else
		! $4 $1 -o $TEST_DIR/$OUT -S -D$2 >/dev/null
	fi
}

run_tests() {
	for SRC in $SOURCES
	do
		echo -en "\r\033[KTESTING $SRC"

		if [ -z "${SRC##*should_fail*}" ]; then
			found=0
			for D in $(sed -n -e 's/\/\/ DEFS //p' <$SRC); do
				found=1
				test_source $SRC $D false $1
			done
			[ $found -eq 0 ] && test_source $SRC $MUSL AAA false $1
		else
			found=0
			for D in $(sed -n -e 's/\/\/ DEFS //p' <$SRC); do
				test_source $SRC $D true $1
			done
			[ $found -eq 0 ] && test_source $SRC AAA true $1
		fi
	done
}

TEST_DIR=test_asm

mkdir -p $TEST_DIR

SOURCES=$(find tests -name '*.c')

set -e

echo "TESTING SOURCES IN tests/"
run_tests ./cc
echo -en "\r\033[KNo errors"
echo

echo "TESTING SELF COMPILATION"
./self_compile.sh 2
diff asm/ asm2/
echo "No errors"

echo "TESTING SECOND GENERATION ON SOURCES IN tests/"
TEST_DIR=test_asm2
mkdir -p $TEST_DIR
run_tests ./cc_self
echo -en "\r\033[KNo errors"
echo

diff test_asm test_asm2 -x 'std_macros.s' # Ignore difference in __TIME__.
