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
#include <ktmw32.h>
#include <iostream>
#include <cstdio>
#include <streambuf>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <map>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ktmw32.lib")
