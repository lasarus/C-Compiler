#ifndef ESCAPE_SEQUENCE_H
#define ESCAPE_SEQUENCE_H

#include <stdint.h>

int escape_sequence_read(uint32_t *out, const char **start, int n);
void character_to_escape_sequence(char character, char output[static 5], int allow_compilcated_escape);

#endif
