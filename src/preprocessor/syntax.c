#include "syntax.h"

int is_nondigit(char c) {
	return (c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		c == '_';
}

int is_digit(char c) {
	return c >= '0' && c <= '9';
}

int is_octal_digit(char c) {
	return c >= '0' && c <= '7';
}

int is_hexadecimal_digit(char c) {
	return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int is_sign(char c) {
	return c == '-' || c == '+';
}

// 6.4p3
int is_space(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f';
}

int is_identifier_nondigit(char c) {
	// TODO: implement universal character names.
	return is_nondigit(c) || c == '$'; /* || is_universal_character_name(c) || other implementation-defined characters */;
}

// Initial is first character of token
int is_identifier(char c, char nc, int initial) {
	(void)nc;
	if (initial)
		return is_identifier_nondigit(c) ? 1 : 0;
	else
		return is_identifier_nondigit(c) || is_digit(c) ? 1 : 0;
}

int is_pp_number(char c, char nc, int initial) {
	if (initial) {
		if (is_digit(c))
			return 1;
		if (c == '.' && is_digit(nc))
			return 2;
		// The standard seems redundant.
		// pp-number:
		//     digit
		//     . digit
		//     pp-number digit
		//     pp-number identifier-nondigit
		//     pp-number e sign
		//     pp-number E sign
		//     pp-number p sign
		//     pp-number P sign
		//     pp-number .
	} else {
		if (is_digit(c) ||
			is_identifier_nondigit(c) ||
			c == '.')
			return 1;
		if ((c == 'e' ||
			 c == 'E' ||
			 c == 'p' ||
			 c == 'P') && is_sign(nc))
			return 2;
	}
	return 0;
}

int is_punctuator(char c, char nc, int initial) {
	(void)c, (void)nc, (void)initial;
	const char *puncts = "[](){}.-+*&!~/%<>=^|:;,?";
	for (int i = 0; i < 24; i++) {
		if (puncts[i] == c)
			return 1;
	}
	return 0;
}

int is_hchar(char c) {
	return c != '>' && c != '\n';
}

int is_qchar(char c) {
	return c != '"' && c != '\n';
}
