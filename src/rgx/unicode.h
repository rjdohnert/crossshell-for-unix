#ifndef RGX_UNICODE_H
#define RGX_UNICODE_H

#include "rgx_common.h"

uint32_t utf8_rune(const char* text, size_t length, size_t* width);
WORD rune_type(uint32_t rune, DWORD type);
bool rune_word(uint32_t rune);
uint32_t rune_fold(uint32_t rune);
uint32_t previous_rune(const char* text, size_t position);
int unicode_property_id(const char* name);
bool rune_property(uint32_t rune, int property);
bool utf8_valid(const char* text, size_t length);

#endif
