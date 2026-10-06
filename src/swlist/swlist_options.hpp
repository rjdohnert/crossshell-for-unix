#pragma once

#include "swlist.hpp"

struct SwlistOptions {
    wstring level = L"PRODUCT";
    wstring attribute = L"ALL";
    bool verbose = false;
    bool showSystemComponents = false;
    bool showHelp = false;
    vector<wstring> searchPatterns;
    int output_format = 0;
    wstring pipe_command;
};
