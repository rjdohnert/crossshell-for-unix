#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <io.h>
#include <memory>


enum class SuffixType { Alpha, Numeric, Hex };
enum class SplitMode { Lines, Bytes, Chunks };
