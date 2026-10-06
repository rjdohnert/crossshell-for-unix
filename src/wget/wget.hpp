#pragma once

#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cctype>
#include <streambuf>

#pragma comment(lib, "wininet.lib")

enum class OutputFormat { Human, Json, Csv, Table };
