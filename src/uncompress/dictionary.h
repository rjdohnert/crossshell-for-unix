#ifndef DICTIONARY_H
#define DICTIONARY_H

#include "common.h"

typedef struct LzwDictionary {
    uint16_t prefixTab[LZW_DICT_SIZE];
    uint8_t  suffixTab[LZW_DICT_SIZE];
    uint8_t  stack[LZW_DICT_SIZE];
    uint32_t freeEnt;
    uint32_t maxMaxCode;
} LzwDictionary;

LzwDictionary* LzwDictionary_Create(uint32_t maxBits);
void LzwDictionary_Destroy(LzwDictionary* self);
void LzwDictionary_Reset(LzwDictionary* self, bool blockMode);

#endif
