#ifndef SETBOOT_OPTIONS_HPP
#define SETBOOT_OPTIONS_HPP

#include "setboot.hpp"

class SetbootOptions {
public:
    bool verbose{false};
    std::wstring setPrimary;
    std::wstring setAlternate;
    std::wstring setAutoboot;
    int setTimeout{-1};
    bool isModify{false};

    static void printHelp(const wchar_t* progName);
    static std::wstring toUpper(std::wstring str);
    static bool parse(int argc, wchar_t* argv[], SetbootOptions& opts);
};

#endif // SETBOOT_OPTIONS_HPP
