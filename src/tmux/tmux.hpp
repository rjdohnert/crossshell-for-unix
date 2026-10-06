#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <deque>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <cstring>

#define VT_ESC "\x1b"
#define VT_ALT_BUF_ON "\x1b[?1049h"
#define VT_ALT_BUF_OFF "\x1b[?1049l"
#define VT_HIDE_CURSOR "\x1b[?25l"
#define VT_SHOW_CURSOR "\x1b[?25h"
#define VT_MOUSE_ON "\x1b[?1000h\x1b[?1002h\x1b[?1006h"
#define VT_MOUSE_OFF "\x1b[?1000l\x1b[?1002l\x1b[?1006l"
#define VT_RESET "\x1b[0m"

#pragma comment(lib, "user32.lib")
