#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <cwchar>
#include <cstdio>
#include <memory>

enum class ReadOutputFormat { Human, Json, Csv, Table };
