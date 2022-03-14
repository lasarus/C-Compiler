#include "arguments.h"

#include "common.h"

#include <stdlib.h>

struct arguments arguments_parse(int argc, char **argv) {
	size_t operand_size = 0, operand_cap = 0;
	const char **operands = NULL;

	size_t define_size = 0, define_cap = 0;
	const char **defines = NULL;

	size_t undefine_size = 0, undefine_cap = 0;
	const char **undefines = NULL;

	size_t include_size = 0, include_cap = 0;
	const char **includes = NULL;

	size_t library_size = 0, library_cap = 0;
	const char **libraries = NULL;

	size_t flag_size = 0, flag_cap = 0;
	const char **flags = NULL;

	struct arguments ret = { 0 };

	enum state {
		S_OPERAND,
		S_DEFINE,
		S_UNDEFINE,
		S_INCLUDE,
		S_LIBRARY,
		S_FLAG,
		S_OUTFILE,
		S_OPTLEVEL
	} state = S_OPERAND;

	int i = 1;
	const char *next_arg = NULL;
	for (;;) {
		const char *arg = NULL;
		if (next_arg) {
			arg = next_arg;
			next_arg = NULL;
		} else if (i < argc) {
			arg = argv[i++];
		} else {
			break;
		}

		enum state next_state = S_OPERAND;

		if (state == S_OPERAND) {
			if (*arg == '-') {
				arg++;

				int cont = 1, ignore = 0;
				switch (*arg) {
				case 'c': ret.flag_c = 1; cont = 0; break;
				case 'g': ret.flag_g = 1; cont = 0; break;
				case 's': ret.flag_s = 1; cont = 0; break;
				case 'E': ret.flag_E = 1; cont = 0; break;
				case 'S': ret.flag_S = 1; cont = 0; break;

				case 'W': ignore = 1; cont = 0; break;

				case 'o': next_state = S_OUTFILE; break;
				case 'O': next_state = S_OPTLEVEL; break;
				case 'D': next_state = S_DEFINE; break;
				case 'U': next_state = S_UNDEFINE; break;
				case 'I': next_state = S_INCLUDE; break;
				case 'L': next_state = S_LIBRARY; break;
				case 'f': next_state = S_FLAG; break;
				}

				arg++;

				if (*arg) {
					if (cont) {
						next_arg = arg;
					} else if (!ignore) {
						ARG_ERROR(i, "Unrecognized flag: \"%s\"", argv[i]);
					}
				}
			} else {
				ADD_ELEMENT(operand_size, operand_cap, operands) = arg;
			}
		} else if (state == S_DEFINE) {
			ADD_ELEMENT(define_size, define_cap, defines) = arg;
		} else if (state == S_UNDEFINE) {
			ADD_ELEMENT(undefine_size, undefine_cap, undefines) = arg;
		} else if (state == S_INCLUDE) {
			ADD_ELEMENT(include_size, include_cap, includes) = arg;
		} else if (state == S_LIBRARY) {
			ADD_ELEMENT(library_size, library_cap, libraries) = arg;
		} else if (state == S_FLAG) {
			ADD_ELEMENT(flag_size, flag_cap, flags) = arg;
		} else if (state == S_OUTFILE) {
			ret.outfile = arg;
		} else if (state == S_OPTLEVEL) {
			ret.optlevel = atoi(arg);
		}

		state = next_state;
	}

	ret.n_operand = operand_size;
	ret.operands = operands;

	ret.n_define = define_size;
	ret.defines = defines;

	ret.n_undefine = undefine_size;
	ret.undefines = undefines;

	ret.n_include = include_size;
	ret.includes = includes;

	ret.n_library = library_size;
	ret.libraries = libraries;

	ret.n_flag = flag_size;
	ret.flags = flags;

	return ret;
}

void arguments_free(struct arguments *arguments) {
	free(arguments->operands);
	free(arguments->defines);
	free(arguments->undefines);
	free(arguments->includes);
	free(arguments->libraries);
}
