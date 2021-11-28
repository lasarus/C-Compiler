# C Compiler

This is a compiler written from scratch in C, with fully supporting C18 as a goal.
It can currently compile itself, and most simple programs.
Some features are missing, but most have some level of support (including variable length arrays and _Generic.)
Avert your eyes from the source code. It is not very nice.

You can read the tests or (if you dare) the source code to get a sense of what this compiler currently supports.

## Build instructions
Just run

    make

to build.
## Compilation instructions
    cc input.c output.s -Iinclude_directory/ -DDEFINTION -DNAME=VALUE
The output will be an **x86-64** assembly file with AT&T syntax that can be assembled with your assembler of choice.
## Self compilation
To use musl for self compilation, create a directory called `musl/` and put the musl headers there.
Alternatively, if no `musl/` exists, the self compilation script will use the headers found in `/usr/include/` as well as those in `include/linux/`.
Only musl and glibc are tested, but other alternatives will likely work.
Compilation with glibc is approximately 3x slower than musl.

To start self compilation run the command:

    ./self_compile.sh
The compiler will then compile itself, and the resulting compiler will in turn also compile itself, and so on in an infinite loop.
Assembly output from the first generation compiler will be put in asm/, and the output from the following generations is put in asm2/. Currently the outputs of all generations of the compiler are identical (anything else would be a bug.)
## Testing
There is a very basic test suite implemented. It runs all `*.c` files in `tests/` and aborts if any of them fails, either during compilation, or runtime. It also self-compiles twice, and checks that the outputs are identical. Use the following command to run the tests:

    ./run_tests.sh
