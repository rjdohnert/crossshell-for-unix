#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <locationapi.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <optional>

#pragma comment(lib, "Locationapi.lib")
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Ole32.lib")
