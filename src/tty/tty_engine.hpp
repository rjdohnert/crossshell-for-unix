#pragma once

#include "tty_options.hpp"
#include "tty.hpp"

class TtyEngine {
public:
    static bool isTerminal(HANDLE hInput, std::wstring& ttyName);

    static int execute(const TtyOptions& opts);
};
