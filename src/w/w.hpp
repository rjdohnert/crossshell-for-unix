#pragma once

#ifndef UNICODE
#ifndef UNICODE
#define UNICODE
#endif
#endif
#ifndef _UNICODE
#ifndef _UNICODE
#define _UNICODE
#endif
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wtsapi32.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <sstream>
#include <clocale>
#include <cwctype>
#include <cstdio>
#include <streambuf>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "ws2_32.lib")
