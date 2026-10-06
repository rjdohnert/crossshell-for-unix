#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <cstdio>
#include <memory>


enum class BufferMode {
    Unbuffered,   // '0'
    LineBuffered, // 'L'
    BlockBuffered // 'SIZE' (e.g. 1024, 4096)
};
