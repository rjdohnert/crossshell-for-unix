#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wtsapi32.h>
#include <sysinfoapi.h>

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <memory>

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Advapi32.lib")


