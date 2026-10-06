#ifndef MAN_HPP
#define MAN_HPP

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <conio.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace Style {
    const std::string RESET        = "\033[0m";
    const std::string BOLD         = "\033[1m";
    const std::string DIM          = "\033[2m";
    const std::string ITALIC       = "\033[3m";
    const std::string UNDERLINE    = "\033[4m";
    const std::string REVERSE      = "\033[7m";

    const std::string FG_BLACK     = "\033[30m";
    const std::string FG_RED       = "\033[31m";
    const std::string FG_GREEN     = "\033[32m";
    const std::string FG_YELLOW    = "\033[33m";
    const std::string FG_BLUE      = "\033[34m";
    const std::string FG_MAGENTA   = "\033[35m";
    const std::string FG_CYAN      = "\033[36m";
    const std::string FG_WHITE     = "\033[37m";
    const std::string FG_GRAY      = "\033[90m";

    const std::string FG_B_CYAN    = "\033[96m";
    const std::string FG_B_YELLOW  = "\033[93m";
    const std::string FG_B_WHITE   = "\033[97m";

    const std::string BG_GRAY      = "\033[48;5;236m";
    const std::string BG_YELLOW    = "\033[43;30m";
}

std::string StripANSI(const std::string& input);
size_t VisibleLength(const std::string& input);
std::string ToLower(std::string s);
std::string ToUpper(std::string s);
bool FileExists(const std::string& path);
std::string ReadFileToString(const std::string& path);
std::vector<std::string> WrapLine(const std::string& line, size_t maxWidth);

#endif // MAN_HPP
