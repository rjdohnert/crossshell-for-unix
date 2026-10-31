#ifndef UNCOMPRESS_COMMON_H
#define UNCOMPRESS_COMMON_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>

#define UNCOMPRESS_VERSION       L"2.0.0"
#define LZW_MAGIC_1              0x1F
#define LZW_MAGIC_2              0x9D
#define LZW_BIT_MASK             0x1F
#define LZW_BLOCK_MODE           0x80
#define LZW_RESERVED_MASK        0x60
#define LZW_INIT_BITS            9
#define LZW_MAX_BITS             16
#define LZW_FIRST_CODE           257
#define LZW_CLEAR_CODE           256
#define LZW_DICT_SIZE            (1U << LZW_MAX_BITS)
#define IO_BUFFER_SIZE           65536
#define BUFFER_PADDING           64
#define PATH_BUFFER_CAPACITY     32768

extern volatile bool g_Interrupted;

// Prefix a path with \\?\ so Win32 APIs accept paths longer than MAX_PATH.
const wchar_t* LongPath(const wchar_t* path);

#endif
