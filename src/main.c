#include "preprocessor/preprocessor.h"
#include "parser/parser.h"
#include "parser/symbols.h"
#include "codegen/codegen.h"
#include "common.h"
#include "assembler/assembler.h"
#include "linker/elf.h"
#include "linker/coff.h"
#include "abi/abi.h"
#include "arguments.h"
#include "escape_sequence.h"

#ifdef CONFIG_PATH
#include CONFIG_PATH
#else
#include "../config.h"
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static void add_implementation_defs(void) {
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

static struct string_view get_basename(const char *path) {
	int last_slash = -1;
	for (int i = 0; path[i]; i++)
		if (path[i] == '/')
			last_slash = i;

	return sv_slice_string((char *)path, last_slash + 1, -1);
}

static int is_ext_file(struct string_view view, char ext) {
	return view.str[view.len - 2] == '.' &&
		view.str[view.len - 1] == ext;
}

static char *replace_object_file_suffix(const char* path) {
	int len = strlen(path);

    if (len <= 2 || strcmp(path + len - 2, ".o") != 0)
		return NULL;

	return allocate_printf("%.*s.d", len - 2, path);
}

static void add_definition(const char *str) {
	char *buffer = strdup(str);

	char *name = buffer, *value = NULL;

	for (int i = 0; str[i]; i++) {
		if (str[i] == '=') {
			buffer[i] = '\0';
			value = buffer + i + 1;
			break;
		}
	}

	define_string(name, value ? value : "1");

	free(buffer);
}

static void set_flags(struct arguments *arguments) {
	for (int i = 0; i < arguments->n_flag; i++) {
		const char *flag = arguments->flags[i];

		if (strcmp(flag, "cmodel=small") == 0) {
			codegen_flags.code_model = CODE_MODEL_SMALL;
		} else if (strcmp(flag, "cmodel=large") == 0) {
			codegen_flags.code_model = CODE_MODEL_LARGE;
		} else if (strncmp(flag, "debug-stack-size", 16) == 0) {
			codegen_flags.debug_stack_size = 1;
			if (flag[16] == '=') {
				codegen_flags.debug_stack_min = atoi(flag + 17);
				printf("DBG STACK MIN: %d\n", codegen_flags.debug_stack_min);
			}
		} else if (strcmp(flag, "abi=ms") == 0) {
			abi = ABI_MICROSOFT;
		} else if (strcmp(flag, "abi=sysv") == 0) {
			abi = ABI_SYSV;
		} else if (strcmp(flag, "mingw-workarounds") == 0) {
			mingw_workarounds = 1;
		}
	}
}

static size_t object_size, object_cap;
static struct object *objects;

static void compile_file(const char *path,
						 struct arguments *arguments) {
	struct string_view basename = get_basename(path);

	symbols_init();

	if (mingw_workarounds) {
		abi_init_mingw_workarounds();
	}

	switch (abi) {
	case ABI_SYSV: abi_init_sysv(); break;
	case ABI_MICROSOFT: abi_init_microsoft(); break;
	}

	add_implementation_defs();

	for (int i = 0; i < arguments->n_include; i++)
		input_add_include_path(arguments->includes[i]);

	for (unsigned i = 0; default_include[i]; i++)
		input_add_include_path(default_include[i]);

	for (int i = 0; default_defs[i]; i++)
		add_definition(default_defs[i]);

	for (int i = 0; i < arguments->n_define; i++)
		add_definition(arguments->defines[i]);

	for (int i = 0; i < arguments->n_undefine; i++)
		define_remove(arguments->undefines[i]);

	if (arguments->flag_MD)
		preprocessor_write_dependencies();

	preprocessor_init(path);
	parse_into_ir();

	struct object out_object = { 0 };

	const char *outfile = arguments->outfile;

	if (!outfile) {
		if (arguments->flag_S)
			outfile = allocate_printf("%.*s.s", basename.len - 2, basename.str);
		else if (arguments->flag_c)
			outfile = allocate_printf("%.*s.o", basename.len - 2, basename.str);
		else
			outfile = "a.out";
	}

	if (arguments->flag_S) {
		asm_init_text_out(outfile);
	} else {
		asm_init_object(&out_object);
	}

	codegen();

	if (arguments->flag_c) {
		switch (abi) {
		case ABI_SYSV: elf_write_object(outfile, &out_object); break;
		case ABI_MICROSOFT: coff_write_object(outfile, &out_object); break;
		}
	} else if (arguments->flag_S) {
	} else {
		ADD_ELEMENT(object_size, object_cap, objects) = out_object;
	}

	if (arguments->flag_MD) {
		const char *mt_path = NULL;
		if (arguments->mt_path)
			mt_path = arguments->mt_path;
		else
			mt_path = outfile;

		const char *mf_path = NULL;
		if (arguments->mf_path) {
			mf_path = arguments->mf_path;
		} else {
			mf_path = replace_object_file_suffix(outfile);
		}

		preprocessor_finish_writing_dependencies(mt_path, mf_path);
	}

	// TODO: The compiler currently relies too heavily on global state.
	preprocessor_reset();
	ir_reset();
	asm_reset();
	parser_reset();
}

// This function is only called when -E flag is passed.
// That is: preprocess, but don't compile.
static void preprocess_file(const char *path, struct arguments *arguments) {
	symbols_init();

	if (mingw_workarounds) {
		abi_init_mingw_workarounds();
	}

	switch (abi) {
	case ABI_SYSV: abi_init_sysv(); break;
	case ABI_MICROSOFT: abi_init_microsoft(); break;
	}

	add_implementation_defs();

	for (int i = 0; i < arguments->n_include; i++)
		input_add_include_path(arguments->includes[i]);

	for (unsigned i = 0; default_include[i]; i++)
		input_add_include_path(default_include[i]);

	for (int i = 0; default_defs[i]; i++)
		add_definition(default_defs[i]);

	for (int i = 0; i < arguments->n_define; i++)
		add_definition(arguments->defines[i]);

	for (int i = 0; i < arguments->n_undefine; i++)
		NOTIMP();

	if (arguments->flag_MD)
		preprocessor_write_dependencies();

	preprocessor_init(path);

	while (T0->type != T_EOI) {
		if (T0->first_of_line)
			printf("\n");
		else if (T0->whitespace)
			printf(" ");

		printf("%s", dbg_token(T0));

		if (T0->whitespace_after)
			printf(" ");

		t_next();
	}

	preprocessor_reset();
	ir_reset();
	asm_reset();
	parser_reset();
}

int main(int argc, char **argv) {
	struct arguments arguments = arguments_parse(argc, argv);

	int will_link = !(arguments.flag_S || arguments.flag_c || arguments.flag_E);

	if (will_link) {
		printf("Warning! Emitting executables is still work in progress.\n");
	}

	if (arguments.flag_g)
		printf("Warning: -g flag is ignored.\n");

	if (arguments.flag_s)
		NOTIMP();

	if (arguments.n_operand != 1 && arguments.outfile &&
		(arguments.flag_S || arguments.flag_c)) {
		ERROR_NO_POS("Can't have multiple input files with -o.");
	}

	set_flags(&arguments);

	for (int i = 0; i < arguments.n_operand; i++) {
		struct string_view basename = get_basename(arguments.operands[i]);

		if (arguments.flag_E) {
			if (is_ext_file(basename, 'c')) {
				preprocess_file(arguments.operands[i], &arguments);
			} else {
				ERROR_NO_POS("Can't preprocess %s.", arguments.operands[i]);
			}
			continue;
		}

		if (is_ext_file(basename, 'c')) {
			compile_file(arguments.operands[i], &arguments);
		} else if (is_ext_file(basename, 'o')) {
			assert(!(arguments.flag_S || arguments.flag_c));
			struct object *object = elf_read_object(arguments.operands[i]);
			if (!object)
				NOTIMP();
			ADD_ELEMENT(object_size, object_cap, objects) = *object;
		} else {
			printf("Not implemented filename: %.*s", basename.len, basename.str);
			NOTIMP();
		}
	}

	if (will_link) {
		struct executable *executable = linker_link(object_size, objects);
		elf_write_executable(arguments.outfile ? arguments.outfile : "a.out", executable);
	}

	arguments_free(&arguments);

	return 0;
}
