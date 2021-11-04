#include "input.h"

#include <common.h>

#include <errno.h>

void read_contents(struct input *input, FILE *fp) {
	// TODO: pipes are probably good to support.
	fseek(fp, 0, SEEK_END);
	input->contents_cap = input->contents_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	input->contents = malloc(input->contents_size + 1);
	fread(input->contents, 1, input->contents_size, fp);
}

struct input input_create(const char *filename, FILE *fp) {
	struct input input = {
		.filename = filename,
		.iline = 1, .icol = 1,
	};

	read_contents(&input, fp);

	// Buffer should be initialized at the start.
	for (int i = 0; i < N_BUFF - 1; i++)
		input_next(&input);
	input.c[0] = '\n'; // Needs to start with newline.

	return input;
}

static char next_char(struct input *input) {
	while (input->c_ptr < input->contents_size - 1 &&
		input->contents[input->c_ptr] == '\\' &&
		input->contents[input->c_ptr + 1] == '\n') {
		input->c_ptr += 2;
		input->iline++;
		input->icol = 0;
	}

	if (input->c_ptr >= input->contents_size)
		return '\0';

	if (input->contents[input->c_ptr] == '\n') {
		input->iline++;
		input->icol = 0;
	}

	input->icol++;

	return input->contents[input->c_ptr++];
}

void input_next(struct input *input) {
	for (int i = 0; i < N_BUFF - 1; i++) {
		input->c[i] = input->c[i + 1];
		input->pos[i] = input->pos[i + 1];
	}

	input->c[N_BUFF - 1] = next_char(input);
	input->pos[N_BUFF - 1] = (struct position) {
		input->filename, input->iline, input->icol
	};
}

static size_t paths_size = 0, paths_cap;
static const char **paths = NULL;

// Used for #pragma once.
static struct string_set disabled_headers;

void input_add_include_path(const char *path) {
	ADD_ELEMENT(paths_size, paths_cap, paths) = path;
}

static int last_slash_pos(const char *str) {
	int slash_pos = 0;
	for (int i = 0; str[i]; i++) {
		if (str[i] == '/')
			slash_pos = i + 1;
	}
	return slash_pos;
}

static FILE *try_open_file(const char *path) {
	FILE *fp = fopen(path, "r");
	if (!fp && errno != ENOENT) {
		char *str = strerror(errno);
		ERROR("Error opening file %s, %s", path, str);
	}
	return fp;
}

void input_open(struct input **input, const char *path, int system) {
	FILE *fp = NULL;

	char path_buffer[256]; // TODO: Remove arbitrary limit.

	(void)system; // <- TODO.
	if (*input) {
		int last_slash = last_slash_pos((*input)->filename);
		if (last_slash)
			sprintf(path_buffer, "%.*s/%s", last_slash, (*input)->filename, path);
		else
			sprintf(path_buffer, "%s", path);
		fp = try_open_file(path_buffer);

		for (unsigned i = 0; !fp && i < paths_size; i++) {
			sprintf(path_buffer, "%s/%s", paths[i], path);
			fp = try_open_file(path_buffer);
		}
	} else {
		sprintf(path_buffer, "%s", path);
		fp = try_open_file(path_buffer);
	}


	if (!fp) {
		ERROR("\"%s\" not found in search path", path);
	}

	if (string_set_contains(disabled_headers, path_buffer)) {
		fclose(fp);
		return;
	}

	struct input *n_top = malloc(sizeof *n_top);
	*n_top = input_create(strdup(path_buffer), fp);
	n_top->next = *input;
	*input = n_top;

	fclose(fp);
}

void input_close(struct input **input) {
	struct input *prev = *input;
	*input = prev->next;
	free(prev->contents);
	free(prev);
}

void input_disable_path(struct input *input) {
	string_set_insert(&disabled_headers, strdup(input->filename));
}
