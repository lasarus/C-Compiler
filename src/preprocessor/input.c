#include "input.h"

#include <common.h>

#include <errno.h>
#include <assert.h>
#include <limits.h>

static size_t paths_size = 0, paths_cap;
static const char **paths = NULL;

// Used for #pragma once.
static struct string_set disabled_headers;

void input_reset(void) {
	paths_size = paths_cap = 0;
	free(paths);
	paths = 0;
	disabled_headers = (struct string_set) { 0 };
}

static struct input *input_create(const char *path, FILE *fp) {
	struct input input = {
		.path = path,
	};

	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	rewind(fp);

	input.contents = cc_malloc(size + 1);
	fread(input.contents, size, 1, fp);
	input.contents[size] = '\0';

	return ALLOC(input);
}

void input_add_include_path(const char *path) {
	ADD_ELEMENT(paths_size, paths_cap, paths) = path;
}

static int last_slash_pos(const char *str) {
	int slash_pos = 0;
	for (int i = 0; str[i]; i++) {
		if (str[i] == '/' && slash_pos != i)
			slash_pos = i + 1;
	}
	return slash_pos;
}

static FILE *try_open_file(const char *path) {
	FILE *fp = fopen(path, "r");
	if (!fp && errno != ENOENT) {
		char *str = strerror(errno);
		ICE("Error opening file %s, %s", path, str);
	}
	return fp;
}

#define BUFFER_SIZE 256

static FILE *try_open_local_path(const char *parent_path, const char *path, char *path_buffer) {
	int last_slash = last_slash_pos(parent_path);
	if (last_slash && path[0] != '/')
		assert(snprintf(path_buffer, BUFFER_SIZE, "%.*s/%s", last_slash, parent_path, path) < BUFFER_SIZE);
	else
		assert(snprintf(path_buffer, BUFFER_SIZE, "%s", path) < BUFFER_SIZE);
	return try_open_file(path_buffer);
}

struct input *input_open(const char *parent_path, const char *path, int system) {
	FILE *fp = NULL;

	char path_buffer[BUFFER_SIZE]; // TODO: Remove arbitrary limit.

	if (!system)
		fp = try_open_local_path(parent_path, path, path_buffer);

	for (unsigned i = 0; !fp && i < paths_size; i++) {
		assert(snprintf(path_buffer, BUFFER_SIZE, "%s/%s", paths[i], path) < BUFFER_SIZE);
		fp = try_open_file(path_buffer);
	}

	if (!fp && system)
		fp = try_open_local_path(parent_path, path, path_buffer);

	if (!fp)
		ICE("\"%s\" not found in search path, with origin %s", path, parent_path ? parent_path : (const char *)".");

	if (string_set_contains(disabled_headers, sv_from_str(path_buffer))) {
		fclose(fp);
		return NULL;
	}

	struct input *input = input_create(strdup(path_buffer), fp);

	fclose(fp);

	return input;
}

void input_disable_path(const char *path) {
	string_set_insert(&disabled_headers, strdup(path));
}

void input_free(struct input *input) {
	// TODO: Handle the freeing of tokens somehow.
	free(input);
}
