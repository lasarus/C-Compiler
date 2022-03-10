#!/bin/bash

test_source() {
	OUT="$(basename -s .c $1).s"

	if [ "$4" == "true" ]; then
		if [ "$2" == "true" ]; then
			$5 $1 -o $TEST_DIR/$OUT -S -Imusl -D$3
		else
			$5 $1 -o $TEST_DIR/$OUT -S -I/usr/include -Iinclude/linux -D$3
		fi

		musl-gcc $TEST_DIR/$OUT -o test -no-pie -lm -Wall -Werror
		./test
	else
		if [ "$2" == "true" ]; then
			! $5 $1 -o $TEST_DIR/$OUT -S -Imusl -D$3 >/dev/null
		else
			! $5 $1 -o $TEST_DIR/$OUT -S -I/usr/include -Iinclude/linux -D$3 >/dev/null
		fi
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
				test_source $SRC $MUSL $D false $1
			done
			[ $found -eq 0 ] && test_source $SRC $MUSL AAA false $1
		else
			found=0
			for D in $(sed -n -e 's/\/\/ DEFS //p' <$SRC); do
				test_source $SRC $MUSL $D true $1
			done
			[ $found -eq 0 ] && test_source $SRC $MUSL AAA true $1
		fi
	done
}

TEST_DIR=test_asm
MUSL=true

if [ ! -d "musl" ] 
then
	echo "Musl headers not provided, trying /usr/include instead"
	MUSL=false
fi

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
