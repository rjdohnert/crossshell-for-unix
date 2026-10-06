#pragma once

#include <windows.h>
#include <tlhelp32.h>
#pragma comment(lib, "advapi32.lib")

#include <iostream>
#include <string>
#include <vector>
#include <cwctype>
#include <cstdio>

using NtSuspendProcessFn = LONG (NTAPI*)(HANDLE);
using NtResumeProcessFn = LONG (NTAPI*)(HANDLE);
