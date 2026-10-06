#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sysinfoapi.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

#pragma comment(lib, "setupapi.lib")
