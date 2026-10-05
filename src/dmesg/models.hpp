/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DMESG_MODELS_HPP
#define DMESG_MODELS_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace dmesg {

class AnsiColor {
public:
    static inline const wchar_t* CLR_RESET   = L"\x1b[0m";
    static inline const wchar_t* CLR_DIM     = L"\x1b[2m";
    static inline const wchar_t* CLR_CYAN    = L"\x1b[36m";
    static inline const wchar_t* CLR_GREEN   = L"\x1b[32m";
    static inline const wchar_t* CLR_YELLOW  = L"\x1b[33m";
    static inline const wchar_t* CLR_RED     = L"\x1b[31m";
    static inline const wchar_t* CLR_MAGENTA = L"\x1b[35m";

    static bool enableVirtualTerminal() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE || hOut == NULL) return false;
        DWORD mode = 0;
        if (!GetConsoleMode(hOut, &mode)) return false;
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        return SetConsoleMode(hOut, mode) != 0;
    }

    static const wchar_t* levelColor(const std::wstring& level) {
        if (level == L"1" || level == L"2") return CLR_RED;
        if (level == L"3") return CLR_YELLOW;
        if (level == L"4") return CLR_GREEN;
        if (level == L"5") return CLR_CYAN;
        return CLR_MAGENTA;
    }

    static const wchar_t* providerColor(const std::wstring& provider) {
        if (provider.find(L"Microsoft-Windows-Kernel") != std::wstring::npos) return CLR_CYAN;
        if (provider.find(L"Microsoft-Windows-Driver") != std::wstring::npos) return CLR_MAGENTA;
        if (provider == L"Service Control Manager") return CLR_YELLOW;
        return CLR_GREEN;
    }
};

} // namespace dmesg

#endif // DMESG_MODELS_HPP
