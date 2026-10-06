#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winternl.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>
#include <memory>

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Advapi32.lib")

extern "C" NTSTATUS NTAPI RtlGetVersion(PRTL_OSVERSIONINFOW lpVersionInformation);

#define KUSER_SHARED_DATA_VA ((const BYTE*)0x7FFE0000)
#define KUSD_MAJOR_VERSION   (*(const ULONG*)(KUSER_SHARED_DATA_VA + 0x0260))
#define KUSD_MINOR_VERSION   (*(const ULONG*)(KUSER_SHARED_DATA_VA + 0x0264))
#define KUSD_BUILD_NUMBER    (*(const ULONG*)(KUSER_SHARED_DATA_VA + 0x0268))
