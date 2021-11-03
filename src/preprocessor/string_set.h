#ifndef STRING_SET_H
#define STRING_SET_H

// I would like to make this immutable,
// and so that it is hash-consed in some
// way.

struct string_set {
	int size, cap;
	char **strings;
};

struct string_set string_set_intersection(struct string_set a, struct string_set b);
struct string_set string_set_union(struct string_set a, struct string_set b);
struct string_set string_set_dup(struct string_set a);
void string_set_free(struct string_set a);
void string_set_insert(struct string_set *a, char *str);
int string_set_contains(struct string_set a, char *str);

#endif
