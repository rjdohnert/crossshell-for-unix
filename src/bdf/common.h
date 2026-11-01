#ifndef BDF_COMMON_H
#define BDF_COMMON_H

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define BDF_VERSION "3.0.0-production"
#define HPUX_NAME_MAX_COL 20
#define DEFAULT_NUM_WIDTH 8
#define BDF_PATH_CAP 32768

const wchar_t* LongPath(const wchar_t* path);
char* SafeStrDup(const char* s);
wchar_t* Utf8ToUtf16(const char* utf8);
char* Utf16ToUtf8(const wchar_t* utf16);
size_t CountDigits(uint64_t n);

#endif
