#ifndef COMPRESS_COMMON_H
#define COMPRESS_COMMON_H

#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>

#define COMPRESS_VERSION L"3.3.0"
#define MAGIC_BYTE_0     0x1F
#define MAGIC_BYTE_1     0x9D
#define BLOCK_MASK       0x80
#define BITS_MASK        0x1F
#define INIT_BITS        9
#define DEFAULT_BITS     16
#define CLEAR_CODE       256
#define FIRST_FREE_CODE  257
#define HASH_SIZE        69001
#define IO_BUFFER_SIZE   (64 * 1024)
#define STACK_SIZE       65536
#define CHECK_GAP        10000

#endif
