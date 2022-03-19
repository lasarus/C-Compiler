#include "preprocessor/preprocessor.h"
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "common.h"
#include "preprocessor/macro_expander.h"
#include "assembler/assembler.h"
#include "parser/symbols.h"
#include "linker/elf.h"
#include "linker/coff.h"
#include "abi/abi.h"
#include "arguments.h"

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

struct string_view get_basename(const char *path) {
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

static enum {
	ABI_SYSV,
	ABI_MICROSOFT
} abi = ABI_SYSV;

static int mingw_workarounds = 0;

static void set_flags(struct arguments *arguments) {
	for (int i = 0; i < arguments->n_flag; i++) {
		const char *flag = arguments->flags[i];

		if (strcmp(flag, "cmodel=small") == 0) {
			codegen_flags.cmodel = CMODEL_SMALL;
		} else if (strcmp(flag, "cmodel=large") == 0) {
			codegen_flags.cmodel = CMODEL_LARGE;
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

const char *default_include[] = {
	NULL
};

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

	for (int i = 0; i < arguments->n_define; i++)
		add_definition(arguments->defines[i]);

	for (int i = 0; i < arguments->n_undefine; i++)
		NOTIMP();

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

	// TODO: The compiler currently relies too heavily on global state.
	preprocessor_reset();
	ir_reset();
	asm_reset();
	parser_reset();
}

int main(int argc, char **argv) {
	struct arguments arguments = arguments_parse(argc, argv);

	int will_link = !(arguments.flag_S || arguments.flag_c);

	if (will_link) {
		printf("Warning! Emitting executables is still work in progress.\n");
	}

	if (arguments.flag_g)
		printf("Warning: -g flag is ignored.");

	if (arguments.flag_E || arguments.flag_s)
		NOTIMP();

	if (arguments.n_operand != 1 && arguments.outfile &&
		(arguments.flag_S || arguments.flag_c)) {
		ARG_ERROR(0, "Can't have multiple input files with -o.");
	}

	init_source_character_set();
	set_flags(&arguments);

	for (int i = 0; i < arguments.n_operand; i++) {
		struct string_view basename = get_basename(arguments.operands[i]);
		if (is_ext_file(basename, 'c')) {
			compile_file(arguments.operands[i], &arguments);
		} else if (is_ext_file(basename, 'o')) {
			assert(!(arguments.flag_S || arguments.flag_c));
			struct object *object = elf_read_object(arguments.operands[i]);
			if (!object)
				NOTIMP();
			ADD_ELEMENT(object_size, object_cap, objects) = *object;
		} else {
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
