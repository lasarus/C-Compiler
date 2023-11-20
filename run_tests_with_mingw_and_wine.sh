#!/bin/bash

BUILD_DIR=mingw_build
MINGW_INCLUDE=/usr/x86_64-w64-mingw32/include/

mkdir -p $BUILD_DIR

cat <<EOF >$BUILD_DIR/config.h
static const char *default_include[] = {
	"$(realpath include/mingw/)",
	"$MINGW_INCLUDE",
	NULL
};

static const char *default_defs[] = {
	NULL
};

static enum {
	ABI_SYSV,
	ABI_MICROSOFT
} abi = ABI_MICROSOFT;

static int mingw_workarounds = 1;
EOF

cd $BUILD_DIR

make -f ../Makefile check-wine SRC_DIR=../src TEST_DIR=../tests
