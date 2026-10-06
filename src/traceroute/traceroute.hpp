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

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <algorithm>
#include <clocale>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
