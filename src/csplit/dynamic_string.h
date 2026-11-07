#ifndef CSPLIT_DYNAMIC_STRING_H
#define CSPLIT_DYNAMIC_STRING_H

#include <stdbool.h>
#include <stddef.h>

typedef struct DynamicString {
    char* buffer;
    size_t length;
    size_t capacity;
} DynamicString;

/* Initialize an empty growable byte string. */
void DynamicString_init(DynamicString* string);

/* Reset the string while retaining its allocated buffer. */
void DynamicString_clear(DynamicString* string);

/* Append one byte, growing storage safely; false indicates allocation failure. */
bool DynamicString_append_char(DynamicString* string, char value);

/* Release the string's buffer and reset its state. */
void DynamicString_destroy(DynamicString* string);

#endif
