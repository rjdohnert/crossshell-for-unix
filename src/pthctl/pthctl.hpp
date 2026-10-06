#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <shlwapi.h>
#include <cstdio>
#include <streambuf>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shlwapi.lib")

constexpr const wchar_t* PROGRAM_NAME = L"pthctl";
constexpr const wchar_t* VERSION      = L"1.0.0";
constexpr const wchar_t* COPYRIGHT    = L"Copyright (C) 2026, Roberto J Dohnert";
constexpr size_t PATH_HARD_LIMIT_CHARS = 32767;
constexpr size_t PATH_WARN_LIMIT_CHARS = 8191;

constexpr const wchar_t* USER_ENV_KEY   = L"Environment";
constexpr const wchar_t* SYSTEM_ENV_KEY = L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";

enum class OutputFormat { Human, Json, Csv, Table };
enum class Scope { User, System };
