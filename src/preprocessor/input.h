#ifndef INPUT_H
#define INPUT_H

#include <stdlib.h>

#define N_BUFF 3

struct position {
	const char *path;
	int line, column;
};

#define PRINT_POS(POS) do { printf("%s:%d:%d", POS.path, POS.line, POS.column); } while(0)

struct input {
	const char *filename;

	size_t c_ptr, contents_size, contents_cap;
	char *contents;
	struct input *next;

	struct position pos[N_BUFF];
	char c[N_BUFF];

	int iline, icol;
};

void input_next(struct input *input);

void input_add_include_path(const char *path);
void input_open(struct input **input, const char *path, int system);
void input_close(struct input **input);
void input_disable_path(struct input *input);

struct input input_open_string(char *str);

#endif
