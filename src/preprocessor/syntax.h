#ifndef SYNTAX_H
#define SYNTAX_H

int is_nondigit(char c);
int is_digit(char c);
int is_octal_digit(char c);
int is_hexadecimal_digit(char c);
int is_sign(char c);
int is_space(char c);
int is_identifier_nondigit(char c);
int is_identifier(char c, char nc, int initial);
int is_pp_number(char c, char nc, int initial);
int is_punctuator(char c, char nc, int initial);
int is_hchar(char c);
int is_qchar(char c);

#endif
