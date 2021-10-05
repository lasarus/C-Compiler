#include "search_path.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <common.h>

const char **include_paths = NULL;
int n_include_paths = 0;

// *dir is NULL if path contains no slash.
void split_path(char *path, const char **name,
				const char **dir) {
	char *last_slash = NULL;
	for (char *it = path; *it; it++)
		if (*it == '/')
			last_slash = it;

	if (last_slash) {
		*last_slash = '\0';

		*dir = path;
		*name = ++last_slash;
	} else {
		*dir = NULL;
		*name = path;
	}
}

int try_open_file(const char *path, struct file *file) {
	char mutable_path[strlen(path) + 1];
	strcpy(mutable_path, path);

	FILE *fp = fopen(mutable_path, "r");
	if (fp) {
		file->fp = fp;
		file->full = strdup(mutable_path);

		const char *name, *dir;
		split_path(mutable_path, &name,
				   &dir);

		file->name = strdup(name);
		file->dir = dir ? strdup(dir) : NULL;

		return 1;
	} else if (errno != ENOENT) {
		char *str = strerror(errno);
		ERROR("Error opening file %s, %s", mutable_path, str);
	}
	return 0;
}

int check_against_path(const char *dir, const char *name,
					   struct file *file) {
	char whole_path[strlen(dir) + 1 + strlen(name) + 1];
	sprintf(whole_path, "%s/%s", dir, name);

	return try_open_file(whole_path, file);
}

struct file search_include(struct file *current_file,
						   const char *path) {
	struct file out;
	if (current_file) {
		if (check_against_path(current_file->dir,
							   path, &out))
			return out;
	}

	for (int i = 0; i < n_include_paths; i++) {
		if (check_against_path(include_paths[i],
							   path, &out))
			return out;
	}

	ERROR("Could not find %s in search path", path);
}

void add_include_path(const char *path) {
	n_include_paths++;
	include_paths = realloc(include_paths, sizeof *include_paths * n_include_paths);

	include_paths[n_include_paths - 1] = path;
}

void file_free(struct file *file) {
	free(file->dir);
	free(file->name);
	fclose(file->fp);
}
