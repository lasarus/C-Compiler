#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

// Write codepoint to output buffer, padding with zeroes.
void utf8_encode(uint32_t codepoint, char output[4]);

// Decode codepoint starting with *start, and then advance to next codepoint.
uint32_t utf8_decode(const char **start);

#endif
