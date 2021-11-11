#include "string_view.h"

#include <string.h>
#include <stdlib.h>

int sv_string_cmp(struct string_view view, const char *string) {
	if ((unsigned)view.len != strlen(string))
		return 0;

	return memcmp(view.str, string, view.len) == 0;
}

int sv_cmp(struct string_view a, struct string_view b) {
	if (a.len != b.len)
		return 0;

	return memcmp(a.str, b.str, a.len) == 0;
}

struct string_view sv_from_str(char *string) {
	return (struct string_view) {
		.str = string,
		.len = strlen(string)
	};
}

char *sv_to_str(struct string_view sv) {
	if (sv.len == 0)
		return NULL;
	char *buffer = malloc(sv.len + 1);
	memcpy(buffer, sv.str, sv.len);
	buffer[sv.len] = '\0';
	return buffer;
}

// djb2 hash: http://www.cse.yorku.ca/~oz/hash.html
uint32_t sv_hash(struct string_view sv) {
	uint32_t hash = 5381;
	
	for (int i = 0; i < sv.len; i++)
		hash = ((hash << 5) + hash) + sv.str[i];

	return hash;
}

struct string_view sv_concat(struct string_view a, struct string_view b) {
	struct string_view ret = { .len = a.len + b.len };
	ret.str = malloc(ret.len);
	memcpy(ret.str, a.str, a.len);
	memcpy(ret.str + a.len, b.str, b.len);
	return ret;
}

void sv_tail(struct string_view *sv, int n) {
	sv->len -= n;
	sv->str += n;
}
