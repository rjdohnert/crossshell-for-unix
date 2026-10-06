#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <virtdisk.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <limits>
#include <cstdio>
#include <streambuf>
#include <memory>

#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "advapi32.lib")


enum class OutputFormat { Human, Json, Csv, Table };
