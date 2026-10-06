#pragma once

#include "strace.hpp"

class SymbolResolver {
public:
    static void Initialize(HANDLE hProcess);
    static void Cleanup(HANDLE hProcess);
    static std::string Resolve(HANDLE hProcess, DWORD64 address);
};
