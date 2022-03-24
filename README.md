# C Compiler

This is a C17 compiler written from scratch in C.
It can compile itself and most portable C programs.

You can read the tests or the source code to get a sense of what this compiler supports.

Only a linker is required to build programs.
Both the preprocessor and assembler are included in the compiler.

I have successfully built TinyCC, Git, and a few other medium to large code-bases using this compiler.
Some small patching is often needed, but seldom anything major.

## Build instructions

    make

Configuration is done through `config.h`.
This file will be generated from `config.def.h` at the first run of `make`.

## Compilation instructions

	cc input.c -o output.s -S -Iinclude/directory -DDEFINITION -DNAME=VALUE
	cc input.c -o output.o -c -Iinclude/directory -DDEFINITION -DNAME=VALUE

`output.s` is a x86-64 assembly file with AT&T syntax, and `output.o` is a 64-bit relocatable elf file.
The command line format is similar to that of the `c99` POSIX utility.

Without any `-S` or `-c` flag, the compiler will try to link the input into an executable elf file.
The linker is still under development, and will most likely not work for any non-trivial program.

## Self compilation
To start self compilation run the command:

    ./self_compile.sh
The compiler will then compile itself, and the resulting compiler will in turn also compile itself, and so on in an infinite loop.
Assembly output from the first generation compiler will be put in asm/, and the output from the following generations is put in asm2/.
These directories can then be compared to find bugs, they should be exactly identical.
## Testing
A basic test runner script is implemented.
It runs all `*.c` files in `tests/` and aborts if any of them fails during compilation or runtime.
It also self-compiles twice, and checks that the outputs are identical.
Use the following command to run the tests:

    ./run_tests.sh
