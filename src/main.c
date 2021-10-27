#include "preprocessor/preprocessor.h"
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "common.h"
#include "preprocessor/search_path.h"
#include "preprocessor/macro_expander.h"
#include "arch/builtins.h"
#include "parser/symbols.h"

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

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' &&
			argv[i][1] == 'I') {
			add_include_path(argv[i] + 2);
		} else if (argv[i][0] == '-' &&
			argv[i][1] == 'D') {
			struct define def = define_init(argv[i] + 2);
			define_map_add(def);
		} else if (argv[i][0] == '-' &&
			argv[i][1] == 'f') {
			if (strcmp(argv[i] + 2, "cmodel=small") == 0) {
				codegen_flags.cmodel = CMODEL_SMALL;
			} else if (strcmp(argv[i] + 2, "cmodel=large") == 0) {
				codegen_flags.cmodel = CMODEL_LARGE;
			} else if (strcmp(argv[i] + 2, "dmodel=ILP64") == 0) {
				parser_flags.dmodel = DMODEL_ILP64;
			} else if (strcmp(argv[i] + 2, "dmodel=LLP64") == 0) {
				parser_flags.dmodel = DMODEL_LLP64;
			} else if (strcmp(argv[i] + 2, "dmodel=LP64") == 0) {
				parser_flags.dmodel = DMODEL_LP64;
			} else if (strncmp(argv[i] + 2, "debug-stack-size", 16) == 0) {
				codegen_flags.debug_stack_size = 1;
				if (argv[i][18] == '=') {
					codegen_flags.debug_stack_min = atoi(argv[i] + 19);
					printf("DBG STACK MIN: %d\n", codegen_flags.debug_stack_min);
				}
			} else {
				ERROR("invalid flag %s", argv[i]);
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
				ERROR("Too many arguments");
			}
		}
	}

	if (!args.input || !args.output) {
		ERROR("Requires input and output");
	}

	return args;
}

void add_implementation_defs(void) {
	struct define def = define_init("NULL");
	struct token tokens[] = {
		token_init(PP_LPAR, "(", (struct position){0}),
		token_init(PP_IDENT, "void", (struct position){0}),
		token_init(PP_PUNCT, "*", (struct position){0}),
		token_init(PP_RPAR, ")", (struct position){0}),
		token_init(PP_NUMBER, "0", (struct position){0})
	};

	for (unsigned int i = 0;
		 i < sizeof(tokens) / sizeof(*tokens); i++) {
		define_add_def(&def, tokens[i]);
	}
	define_map_add(def);
}

int main(int argc, char **argv) {
	struct arguments arguments = parse_arguments(argc, argv);

	init_source_character_set();

	symbols_init();
	add_implementation_defs();
	builtins_init();

	preprocessor_create(arguments.input);
	parse_into_ir();
	codegen(arguments.output);

	return 0;
}
