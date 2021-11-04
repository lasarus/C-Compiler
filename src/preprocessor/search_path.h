#ifndef SEARCH_PATH_H
#define SEARCH_PATH_H

#include <stdio.h>

struct file {
	char *name, *dir, *full;
	FILE *fp;
};

struct file search_include(const char *dir, const char *path);
int try_open_file(const char *path, struct file *file);

void file_free(struct file *file);
void add_include_path(const char *path);

#endif
