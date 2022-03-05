#include "preprocessor/preprocessor.h"
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "common.h"
#include "preprocessor/macro_expander.h"
#include "assembler/assembler.h"
#include "parser/symbols.h"
#include "abi/abi.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

struct arguments {
	const char *input;
	const char *output;
};

struct arguments parse_arguments(int argc, char **argv) {
	struct arguments args = {0};

	enum {
		STATE_INPUT,
		STATE_OUTPUT,
		STATE_END
	} state = STATE_INPUT;

	enum {
		ABI_SYSV,
		ABI_MICROSOFT
	} abi = ABI_SYSV;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' &&
			argv[i][1] == 'I') {

			input_add_include_path(argv[i] + 2);
		} else if (argv[i][0] == '-' &&
				   argv[i][1] == 'D') {
			char *name = argv[i] + 2, *value = NULL;
			for (unsigned j = 2; argv[i][j]; j++) {
				if (argv[i][j] == '=') {
					argv[i][j] = '\0';
					value = argv[i] + j + 1;
					break;
				}
			}
			define_string(name, value ? value : "1");
		} else if (argv[i][0] == '-' &&
			argv[i][1] == 'f') {
			if (strcmp(argv[i] + 2, "cmodel=small") == 0) {
				codegen_flags.cmodel = CMODEL_SMALL;
			} else if (strcmp(argv[i] + 2, "cmodel=large") == 0) {
				codegen_flags.cmodel = CMODEL_LARGE;
			} else if (strncmp(argv[i] + 2, "debug-stack-size", 16) == 0) {
				codegen_flags.debug_stack_size = 1;
				if (argv[i][18] == '=') {
					codegen_flags.debug_stack_min = atoi(argv[i] + 19);
					printf("DBG STACK MIN: %d\n", codegen_flags.debug_stack_min);
				}
			} else if (strcmp(argv[i] + 2, "abi=ms") == 0) {
				abi = ABI_MICROSOFT;
			} else if (strcmp(argv[i] + 2, "abi=sysv") == 0) {
				abi = ABI_SYSV;
			} else if (strcmp(argv[i] + 2, "mingw-workarounds") == 0) {
				abi_init_mingw_workarounds();
			} else {
				ARG_ERROR(i, "Invalid flag.");
			}
		} else if (argv[i][0] == '-' &&
				   argv[i][1] == 'd') {
			if (strcmp(argv[i] + 2, "half-assemble") == 0) {
				assembler_flags.half_assemble = 1;
			} else if (strcmp(argv[i] + 2, "elf") == 0) {
				assembler_flags.elf = 1;
			}
		} else {
			switch (state) {
			case STATE_INPUT:
				args.input = argv[i];
				state++;
				break;

			case STATE_OUTPUT:
				args.output = argv[i];
				state++;
				break;

			case STATE_END:
				ARG_ERROR(i, "Too many arguments.");
			}
		}
	}

	if (!args.input || !args.output) {
		ARG_ERROR(0, "requires input and output.");
	}

	switch (abi) {
	case ABI_SYSV: abi_init_sysv(); break;
	case ABI_MICROSOFT: abi_init_microsoft(); break;
	}

	return args;
}

void add_implementation_defs(void) {
	define_string("NULL", "(void*)0");
	static const char months[][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	time_t ti = time(NULL);
	struct tm tm = *localtime(&ti);
	define_string("__DATE__",
				  allocate_printf("\"%s %2d %04d\"", months[tm.tm_mon], tm.tm_mday, 1900 + tm.tm_year));

	define_string("__TIME__", allocate_printf("\"%02d:%02d:%02d\"", tm.tm_hour, tm.tm_min, tm.tm_sec));
	define_string("__STDC__", "1");
	define_string("__FUNCTION__", "__func__");
	define_string("__STDC_HOSTED__", "0");
	define_string("__STDC_VERSION__", "201710L");
	define_string("__x86_64__", "1");
}

int main(int argc, char **argv) {
	symbols_init();

	struct arguments arguments = parse_arguments(argc, argv);

	init_source_character_set();

	add_implementation_defs();

	preprocessor_init(arguments.input);
	parse_into_ir();
	codegen(arguments.output);

	return 0;
}
