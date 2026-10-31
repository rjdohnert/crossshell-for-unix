#ifndef STRUTIL_H
#define STRUTIL_H

#include "common.h"

char* SafeStrDup(const char* s);
wchar_t* Utf8ToUtf16(const char* utf8);
char* Utf16ToUtf8(const wchar_t* utf16);
bool FormatIntegerBase(int64_t val, int base, char* out, size_t out_len);
bool ParseIntegerBase(const char* str, int64_t* out_val);

#endif
