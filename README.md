# C Compiler

This is a C17 compiler written from scratch in C.
It can compile itself and most portable C programs.

You can read the tests or the source code to get a sense of what this compiler supports.

Only a linker is required to build programs.
Both the preprocessor and assembler are included in the compiler.

I have successfully built TinyCC, Git, and a few other medium to large code-bases using this compiler.
Some small patching is often needed, but seldom anything major.

## Build instructions
To compile, use the command:

    make

Configuration is done through `config.h`.
This file will be generated from `config.def.h` at the first run of `make`.

Alternatively a `CMakeLists.txt`, and a simple `compile.sh` script is provided.
When not using `make`, the config.h file needs to be created manually.

## Compilation instructions

	bin/cc input.c -o output.s -S -Iinclude/directory -DDEFINITION -DNAME=VALUE
	bin/cc input.c -o output.o -c -Iinclude/directory -DDEFINITION -DNAME=VALUE

`output.s` is a x86-64 assembly file with AT&T syntax, and `output.o` is a 64-bit relocatable elf file.
The command line format is similar to that of the `c99` POSIX utility.

Without any `-S` or `-c` flag, the compiler will try to link the input into an executable elf file.
The linker is still under development, and will most likely not work for any non-trivial program.

## Self compilation
For self compilation, use the command:

    make self-compile

The resulting binaries are `bin/cc2`, and `bin/cc3` for the second and third generations.
The binaries are expected to be exactly identical.

## Testing
To run tests, use the command:

	make check

This compiles and runs all `tests/*.c` files and ensures that there are no errors during compilation or run time.
It also self compiles and checks that the second and third generations are identical.
