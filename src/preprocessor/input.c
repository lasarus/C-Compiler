#include "input.h"

#include <common.h>

#include <assert.h>
#include <errno.h>

static size_t paths_size = 0, paths_cap;
static const char **paths = NULL;

// Used for #pragma once.
static struct string_set disabled_headers;

static struct input input_create(const char *path, FILE *fp) {
	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	rewind(fp);

	char *contents = cc_malloc(size + 1);
	fread(contents, size, 1, fp);
	contents[size] = '\0';

	return (struct input) { .path = path, .contents = contents };
}

static int length_of_path_without_filename(const char *str) {
	int slash_pos = 0;
	for (int i = 0; str[i]; i++) {
		if (str[i] == '/')
			slash_pos = i + 1;
	}
	return slash_pos;
}

static FILE *try_open_file(const char *path, const char *modes) {
	FILE *fp = fopen(path, modes);
	if (!fp && errno != ENOENT) {
		char *str = strerror(errno);
		ICE("Error opening file %s, %s", path, str);
	}
	return fp;
}

void input_reset(void) {
	paths_size = paths_cap = 0;
	free(paths);
	paths = NULL;
	disabled_headers = (struct string_set) { 0 };
}

void input_add_include_path(const char *path) {
	ADD_ELEMENT(paths_size, paths_cap, paths) = path;
}

void input_disable_path(const char *path) {
	string_set_insert(&disabled_headers, strdup(path));
}

FILE *input_search_path(const char *parent_path, const char *path, int system, int is_embed, char **opened_path) {
	FILE *fp = NULL;

	static char *path_buffer = NULL;
	static size_t path_capacity = 0;

	if (!system) {
		int length = length_of_path_without_filename(parent_path);

		expand_printf(&path_buffer, &path_capacity, "%.*s%s", length, parent_path, path);
		fp = try_open_file(path_buffer, is_embed ? "rb" : "r");
	}

	for (unsigned i = 0; !fp && i < paths_size; i++) {
		expand_printf(&path_buffer, &path_capacity, "%s/%s", paths[i], path);
		fp = try_open_file(path_buffer, is_embed ? "rb" : "r");
	}

	if (!fp)
		ICE("\"%s\" not found in search path, with origin %s", path, parent_path);

	*opened_path = path_buffer;

	return fp;
}

struct input input_open(const char *parent_path, const char *path, int system) {
	char *opened_path = NULL;
	FILE *fp = input_search_path(parent_path, path, system, 0, &opened_path);

	struct input input = { 0 };

	if (!string_set_contains(disabled_headers, sv_from_str(opened_path)))
		input = input_create(strdup(opened_path), fp);

	fclose(fp);

	return input;
}
