/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 * See the repository license file for the full license text.
 */

#include "dynamic_string.h"

#include <stdint.h>
#include <stdlib.h>

static bool DynamicString_reserve(DynamicString* string, size_t needed) {
    if (needed == SIZE_MAX) return false;
    if (needed + 1 <= string->capacity) return true;

    size_t new_capacity = string->capacity == 0 ? 128 : string->capacity;
    while (new_capacity <= needed) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = needed + 1;
            break;
        }
        new_capacity *= 2;
    }

    char* new_buffer = (char*)realloc(string->buffer, new_capacity);
    if (!new_buffer) return false;
    string->buffer = new_buffer;
    string->capacity = new_capacity;
    return true;
}

void DynamicString_init(DynamicString* string) {
    string->buffer = NULL;
    string->length = 0;
    string->capacity = 0;
}

void DynamicString_clear(DynamicString* string) {
    string->length = 0;
    if (string->buffer) string->buffer[0] = '\0';
}

bool DynamicString_append_char(DynamicString* string, char value) {
    if (string->length == SIZE_MAX ||
        !DynamicString_reserve(string, string->length + 1)) {
        return false;
    }
    string->buffer[string->length++] = value;
    string->buffer[string->length] = '\0';
    return true;
}

void DynamicString_destroy(DynamicString* string) {
    free(string->buffer);
    string->buffer = NULL;
    string->length = 0;
    string->capacity = 0;
}
