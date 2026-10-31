#include "dictionary.h"

LzwDictionary* LzwDictionary_Create(uint32_t maxBits) {
    LzwDictionary* self = (LzwDictionary*)calloc(1, sizeof(LzwDictionary));
    if (!self) return NULL;
    self->maxMaxCode = 1U << maxBits;
    for (uint32_t i = 0; i < 256; ++i) {
        self->suffixTab[i] = (uint8_t)i;
    }
    return self;
}

void LzwDictionary_Destroy(LzwDictionary* self) {
    free(self);
}

void LzwDictionary_Reset(LzwDictionary* self, bool blockMode) {
    self->freeEnt = blockMode ? LZW_FIRST_CODE : 256;
    memset(self->prefixTab, 0, LZW_DICT_SIZE * sizeof(uint16_t));
    memset(self->suffixTab, 0, LZW_DICT_SIZE * sizeof(uint8_t));
    for (uint32_t i = 0; i < 256; ++i) {
        self->suffixTab[i] = (uint8_t)i;
    }
}
