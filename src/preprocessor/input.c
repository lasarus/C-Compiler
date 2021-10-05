#include "input.h"

#include <common.h>

static void input_internal_next(struct input *input);

struct input input_create(struct file file) {
	struct input input = {
		.file = file,
		.pos = {{0}},
		.iline = 1,
		.icol = 1,
		.c = {'\n', '\n', '\n'},
	};

	// Buffer should be initialized at the start.
	for (int i = 0; i < INT_BUFF; i++)
		input_internal_next(&input);

	for (int i = 0; i < N_BUFF - 1; i++)
		input_next(&input);

	return input;
}

void input_free(struct input *input) {
	file_free(&input->file);
}

static void input_internal_next(struct input *input) {
	for (int i = 0; i < INT_BUFF - 1; i++) {
		input->ic[i] = input->ic[i + 1];
		input->ipos[i] = input->ipos[i + 1];
	}

	char c = '\0';
	if (!feof(input->file.fp)) {
		int fc = fgetc(input->file.fp);

		if (fc == EOF)
			c = 0;
		else
			c = fc;
	}

	if (c == '\n') {
		input->iline++;
		input->icol = 1;
	} else {
		input->icol++;
	}

	input->ic[INT_BUFF - 1] = c;
	input->ipos[INT_BUFF - 1] = (struct position) {
		.path = input->file.full,
		.line = input->iline,
		input->icol
	};
}

static int flush_comment(struct input *input) {
	int did_something = 0;
	if (input->ic[0] == '/' &&
		input->ic[1] == '*') {
		did_something = 1;
		while (!(input->ic[0] == '*' &&
				 input->ic[1] == '/')) {
			input_internal_next(input);

			if (input->ic[0] == '\0')
				ERROR("Comment reached end of file");
		}
		input_internal_next(input);
		input_internal_next(input);
	} else if (input->ic[0] == '/' &&
			   input->ic[1] == '/') {
		did_something = 1;
		while (!(input->ic[0] == '\n')) {
			input_internal_next(input);

			if (input->ic[0] == '\0')
				ERROR("Comment reached end of file");
		}
		input_internal_next(input);
	}
	return did_something;
}

static int flush_backslash(struct input *input) {
	if (input->ic[0] == '\\' &&
		input->ic[1] == '\n') {

		input_internal_next(input);
		input_internal_next(input);

		return 1;
	}
	return 0;
}

void input_next(struct input *input) {
	for (int i = 0; i < N_BUFF - 1; i++) {
		input->c[i] = input->c[i + 1];
		input->pos[i] = input->pos[i + 1];
	}

	int removed_comment = 0;

	do {
		while (flush_comment(input)) {
			removed_comment = 1;
		}
	} while (flush_backslash(input));

	char nc;
	struct position npos;

	npos = input->ipos[0];

	if (removed_comment) {
		nc = ' ';
	} else {
		nc = input->ic[0];
		input_internal_next(input);
	}

	input->c[N_BUFF - 1] = nc;
	input->pos[N_BUFF - 1] = npos;
}
