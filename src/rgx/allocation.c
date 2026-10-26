#include "allocation.h"

void* rgx_calloc(size_t count, size_t size) {
    if (size != 0 && count > SIZE_MAX / size) {
        fprintf(stderr, "rgx: allocation size overflow\n");
        exit(2);
    }
    void* allocation = calloc(count, size);
    if (!allocation) {
        fprintf(stderr, "rgx: out of memory\n");
        exit(2);
    }
    return allocation;
}

void* rgx_malloc(size_t size) {
    void* allocation = malloc(size ? size : 1);
    if (!allocation) {
        fprintf(stderr, "rgx: out of memory\n");
        exit(2);
    }
    return allocation;
}

char* rgx_strdup(const char* text) {
    size_t length = strlen(text);
    char* copy = (char*)rgx_malloc(length + 1);
    memcpy(copy, text, length + 1);
    return copy;
}

void* rgx_realloc_array(void* pointer, size_t count, size_t size) {
    if (size && count > SIZE_MAX / size) {
        fprintf(stderr, "rgx: allocation size overflow\n");
        exit(2);
    }
    void* allocation = realloc(pointer, count && size ? count * size : 1);
    if (!allocation) {
        fprintf(stderr, "rgx: out of memory\n");
        exit(2);
    }
    return allocation;
}

/* ========================================================================= */
/*                          1. CONFIGURATION & OPTIONS                       */
/* ========================================================================= */
