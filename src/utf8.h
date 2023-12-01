#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

// Write code point to output buffer, padding with zeroes.
void utf8_encode(uint32_t codepoint, char output[4]);

// Decode code point starting with *start, and then advance to next code point.
uint32_t utf8_decode(const char **start);

// Get length of code point starting at s.
int utf8_code_point_length(unsigned char s);

#endif
