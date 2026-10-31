#ifndef TYPESET_COMMON_H
#define TYPESET_COMMON_H

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>

#define TYPESET_VERSION "2.0.0"
#define MAX_IDENT_LEN   256
#define TS_PATH_CAP     32768

#define ATTR_READONLY   (1U << 0)
#define ATTR_EXPORT     (1U << 1)
#define ATTR_INTEGER    (1U << 2)
#define ATTR_LOWER      (1U << 3)
#define ATTR_UPPER      (1U << 4)
#define ATTR_LJUST      (1U << 5)
#define ATTR_RJUST      (1U << 6)
#define ATTR_ZFILL      (1U << 7)
#define ATTR_TAGGED     (1U << 8)
#define ATTR_ARRAY      (1U << 9)
#define ATTR_FUNCTION   (1U << 10)

#endif
