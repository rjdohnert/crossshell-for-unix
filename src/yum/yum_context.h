#ifndef YUM_CONTEXT_H
#define YUM_CONTEXT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wuapi.h>
#include <oleauto.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wchar.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uuid.lib")

#define YUM_VERSION L"5.0.0-NT"
#define MAX_CMDLINE 8192

typedef struct {
    bool assume_yes;
    bool verbose;
    bool quiet;
} YumContext;

/* --- Win32 Standard Command-Line Escaping --- */

#endif
