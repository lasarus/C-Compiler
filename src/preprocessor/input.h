#ifndef INPUT_H
#define INPUT_H

#include "search_path.h"

#define N_BUFF 3
#define INT_BUFF 2

struct position {
	const char *path;
	int line, column;
};

#define PRINT_POS(POS) do { printf("%s:%d:%d", POS.path, POS.line, POS.column); } while(0)

struct input {
	struct file file;

	struct position pos[N_BUFF];
	char c[N_BUFF];

	struct position ipos[INT_BUFF];
	char ic[INT_BUFF];

	int iline, icol;
};

struct input input_create(struct file file);
void input_free(struct input *input);

void input_next(struct input *input);

#endif
