#ifndef ARGUMENTS_H
#define ARGUMENTS_H

struct arguments {
	int flag_c, flag_g, flag_s, flag_E, flag_S;
	int optlevel;

	const char *outfile;

	int n_operand;
	const char **operands;

	int n_define;
	const char **defines;

	int n_undefine;
	const char **undefines;

	int n_include;
	const char **includes;

	int n_library;
	const char **libraries;

	int n_library_dir;
	const char **library_dirs;

	int n_flag;
	const char **flags;
};

struct arguments arguments_parse(int argc, char **argv);
void arguments_free(struct arguments *arguments);

#endif
