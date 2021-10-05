#include "common.h"

int escaped_to_str(const char *str) {
	if (str[0] != '\\')
		return str[0];
	else {
		switch (str[1]) {
		case 'n':
			return '\n';
		case '\'':
			return '\'';
		case '\"':
			return '\"';
		case 't':
			return '\t';
		case '0':
			return '\0';
		case '\\':
			return '\\';
		default:
			ERROR("Invalid escape sequence %c%c", str[0], str[1]);
		}
	}
}
