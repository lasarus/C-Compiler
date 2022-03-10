#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stdint.h>

struct string_view {
	int len;
	char *str;
};

int sv_string_cmp(struct string_view view, const char *string);
int sv_cmp(struct string_view a, struct string_view b);
struct string_view sv_from_str(char *string);
struct string_view sv_slice_string(char *string, int start, int end);
char *sv_to_str(struct string_view sv); // This creates a copy.
struct string_view sv_concat(struct string_view a, struct string_view b);

void sv_tail(struct string_view *sv, int n);

uint32_t sv_hash(struct string_view sv);

#endif
