#!/bin/sh

# This is meant as an example for how to compile the compiler without using the
# provided makefile.

gcc src/*.c src/*/*.c -o cc -Isrc/ -std=c17
