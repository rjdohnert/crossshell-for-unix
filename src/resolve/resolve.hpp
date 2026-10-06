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
#include <windns.h>
#include <iphlpapi.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <algorithm>
#include <memory>
#include <clocale>

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
