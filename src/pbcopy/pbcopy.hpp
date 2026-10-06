#ifndef PBCOPY_HPP
#define PBCOPY_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>

#pragma comment(lib, "user32.lib")

class EncodingConverter {
public:
    static std::wstring ConvertToWString(const std::vector<uint8_t>& raw_bytes);
};

class StreamReader {
public:
    static std::vector<uint8_t> ReadAllStdin();
};

#endif // PBCOPY_HPP
