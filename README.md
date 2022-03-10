# C Compiler

This is a compiler written from scratch in C, with fully supporting C18 as a goal.
It can currently compile itself, and most simple programs.

You can read the tests or (if you dare) the source code to get a sense of what this compiler currently supports.

## Build instructions

    make

## Compilation instructions

	cc input.c -o output.s -S -Iinclude/directory -DDEFINITION -DNAME=VALUE
	cc input.c -o output.o -c -Iinclude/directory -DDEFINITION -DNAME=VALUE

`output.s` is a x86-64 assembly file with AT&T syntax, and `output.o` is a 64-bit relocatable elf file.
The command line format is similar to that of the `c99` POSIX utility.

Without any `-S` or `-c` flag, the compiler will try to link the input into an executable elf file.
The linker is still under development, and will most likely not work for any non-trivial program.

## Self compilation
To use musl for self compilation, create a directory called `musl/` and put the musl headers there.
Alternatively, if no `musl/` exists, the self compilation script will use the headers found in `/usr/include/` as well as those in `include/linux/`.

To start self compilation run the command:

    ./self_compile.sh
The compiler will then compile itself, and the resulting compiler will in turn also compile itself, and so on in an infinite loop.
Assembly output from the first generation compiler will be put in asm/, and the output from the following generations is put in asm2/. Currently the outputs of all generations of the compiler are identical (anything else would be a bug.)
## Testing
There is a very basic test suite implemented. It runs all `*.c` files in `tests/` and aborts if any of them fails, either during compilation, or runtime. It also self-compiles twice, and checks that the outputs are identical. Use the following command to run the tests:

    ./run_tests.sh
