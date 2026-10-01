/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <memory>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// 1. CONSTANTS, ENUMS & RAII HANDLES
// ============================================================================

constexpr unsigned int MASK_ALL_BITS = 0777;
constexpr unsigned int DEFAULT_MASK  = 0022;

class ScopedRegistryKey {
public:
    explicit ScopedRegistryKey(HKEY key = NULL) : m_key(key) {}

    ~ScopedRegistryKey() {
        Close();
    }

    ScopedRegistryKey(const ScopedRegistryKey&) = delete;
    ScopedRegistryKey& operator=(const ScopedRegistryKey&) = delete;

    ScopedRegistryKey(ScopedRegistryKey&& other) noexcept : m_key(other.m_key) {
        other.m_key = NULL;
    }

    ScopedRegistryKey& operator=(ScopedRegistryKey&& other) noexcept {
        if (this != &other) {
            Close();
            m_key = other.m_key;
            other.m_key = NULL;
        }
        return *this;
    }

    HKEY Get() const { return m_key; }
    HKEY* Receive() { Close(); return &m_key; }
    bool IsValid() const { return m_key != NULL; }

    void Close() {
        if (m_key != NULL) {
            RegCloseKey(m_key);
            m_key = NULL;
        }
    }

private:
    HKEY m_key;
};

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedProcessHandle() {
        Close();
    }

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    HANDLE m_handle;
};

// ============================================================================
// 2. FORMATTING & PARSING HELPERS
// ============================================================================

class UmaskFormatter {
public:
    static std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size_needed <= 0) return "";
        std::string out(static_cast<size_t>(size_needed - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, out.data(), size_needed, nullptr, nullptr);
        return out;
    }

    static std::string FormatOctalMask(unsigned int mask) {
        std::stringstream ss;
        ss << std::setw(4) << std::setfill('0') << std::oct << (mask & MASK_ALL_BITS);
        return ss.str();
    }

    static std::string FormatSymbolicMask(unsigned int mask) {
        unsigned int perm = (~mask) & MASK_ALL_BITS;

        auto formatWho = [](unsigned int p) -> std::string {
            std::string s;
            if (p & 4) s += "r";
            if (p & 2) s += "w";
            if (p & 1) s += "x";
            return s;
        };

        std::string u = formatWho((perm >> 6) & 7);
        std::string g = formatWho((perm >> 3) & 7);
        std::string o = formatWho(perm & 7);

        return "u=" + u + ",g=" + g + ",o=" + o;
    }
};

class UmaskParser {
public:
    static unsigned int ParseSymbolicMask(unsigned int current_mask, const std::string& expr) {
        unsigned int current_perm = (~current_mask) & MASK_ALL_BITS;
        std::stringstream ss(expr);
        std::string clause;

        while (std::getline(ss, clause, ',')) {
            if (clause.empty()) continue;

            size_t pos = 0;
            unsigned int who_mask = 0;

            while (pos < clause.size() && (clause[pos] == 'u' || clause[pos] == 'g' || clause[pos] == 'o' || clause[pos] == 'a')) {
                if (clause[pos] == 'u') who_mask |= 0700;
                else if (clause[pos] == 'g') who_mask |= 0070;
                else if (clause[pos] == 'o') who_mask |= 0007;
                else if (clause[pos] == 'a') who_mask |= 0777;
                pos++;
            }
            if (who_mask == 0) who_mask = 0777;

            if (pos >= clause.size()) {
                throw std::invalid_argument("Invalid symbolic clause: '" + clause + "'");
            }
            char op = clause[pos++];
            if (op != '=' && op != '+' && op != '-') {
                throw std::invalid_argument("Invalid operator '" + std::string(1, op) + "' in clause: '" + clause + "'");
            }

            unsigned int perm_bits = 0;
            while (pos < clause.size()) {
                char c = clause[pos++];
                if (c == 'r') perm_bits |= 0444;
                else if (c == 'w') perm_bits |= 0222;
                else if (c == 'x') perm_bits |= 0111;
                else if (c == 'u') perm_bits |= ((current_perm >> 6) & 7) * 0111;
                else if (c == 'g') perm_bits |= ((current_perm >> 3) & 7) * 0111;
                else if (c == 'o') perm_bits |= (current_perm & 7) * 0111;
                else {
                    throw std::invalid_argument("Invalid permission character '" + std::string(1, c) + "'");
                }
            }

            unsigned int affected_bits = perm_bits & who_mask;
            if (op == '=') {
                current_perm = (current_perm & ~who_mask) | affected_bits;
            } else if (op == '+') {
                current_perm |= affected_bits;
            } else if (op == '-') {
                current_perm &= ~affected_bits;
            }
        }

        return (~current_perm) & MASK_ALL_BITS;
    }

    static unsigned int ParseMaskInput(unsigned int current_mask, const std::string& input) {
        bool is_octal = !input.empty() && std::all_of(input.begin(), input.end(), [](char c) {
            return c >= '0' && c <= '7';
        });

        if (is_octal) {
            unsigned int val = std::stoul(input, nullptr, 8);
            return val & MASK_ALL_BITS;
        }

        return ParseSymbolicMask(current_mask, input);
    }
};

// ============================================================================
// 3. SYSTEM UMASK MANAGER & SUBCOMMAND EXECUTOR
// ============================================================================

class SystemUmaskManager {
public:
    static unsigned int GetSystemUmask() {
        wchar_t envBuf[32];
        DWORD res = GetEnvironmentVariableW(L"UMASK", envBuf, 32);
        if (res > 0 && res < 32) {
            std::string str = UmaskFormatter::WideToUtf8(std::wstring(envBuf));
            try {
                return UmaskParser::ParseMaskInput(DEFAULT_MASK, str);
            } catch (...) {}
        }

        ScopedRegistryKey hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, hKey.Receive()) == ERROR_SUCCESS) {
            wchar_t regBuf[32];
            DWORD dwType = REG_SZ;
            DWORD dwSize = sizeof(regBuf);
            if (RegQueryValueExW(hKey.Get(), L"UMASK", NULL, &dwType, reinterpret_cast<LPBYTE>(regBuf), &dwSize) == ERROR_SUCCESS) {
                std::string str = UmaskFormatter::WideToUtf8(std::wstring(regBuf));
                try {
                    return UmaskParser::ParseMaskInput(DEFAULT_MASK, str);
                } catch (...) {}
            }
        }

        return DEFAULT_MASK;
    }

    static void SetSystemUmask(unsigned int mask) {
        mask &= MASK_ALL_BITS;

        _umask(static_cast<int>(mask));

        std::string octalStr = UmaskFormatter::FormatOctalMask(mask);
        std::wstring wOctalStr(octalStr.begin(), octalStr.end());
        SetEnvironmentVariableW(L"UMASK", wOctalStr.c_str());

        ScopedRegistryKey hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, hKey.Receive()) == ERROR_SUCCESS) {
            RegSetValueExW(hKey.Get(), L"UMASK", 0, REG_SZ, 
                           reinterpret_cast<const BYTE*>(wOctalStr.c_str()), 
                           static_cast<DWORD>((wOctalStr.length() + 1) * sizeof(wchar_t)));

            DWORD_PTR dwResult = 0;
            SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                                reinterpret_cast<LPARAM>(L"Environment"), SMTO_ABORTIFHUNG, 2000, &dwResult);
        }
    }

    static int ExecuteSubcommand(const std::wstring& cmdline) {
        STARTUPINFOW si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);

        std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
        cmdBuf.push_back(L'\0');

        if (!CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            std::cerr << "umask: failed to execute subcommand (Error " << GetLastError() << ")\n";
            return 1;
        }

        ScopedProcessHandle hProcess(pi.hProcess);
        ScopedProcessHandle hThread(pi.hThread);

        WaitForSingleObject(hProcess.Get(), INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(hProcess.Get(), &exitCode);

        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// 4. OPTIONS PARSER & APPLICATION CONTROLLER
// ============================================================================

class UmaskOptions {
public:
    bool flag_symbolic = false;
    bool flag_print_reusable = false;
    bool show_help = false;
    std::string new_mask_arg;
    std::wstring subcommand_str;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--") {
                std::wstringstream wss;
                for (int j = i + 1; j < argc; ++j) {
                    std::string a = argv[j];
                    wss << std::wstring(a.begin(), a.end()) << (j + 1 < argc ? L" " : L"");
                }
                subcommand_str = wss.str();
                break;
            }

            if (arg == "-S") {
                flag_symbolic = true;
            } else if (arg == "-p") {
                flag_print_reusable = true;
            } else if (arg == "-h" || arg == "--help" || arg == "/?") {
                show_help = true;
                return true;
            } else if (arg[0] != '-') {
                new_mask_arg = arg;
            } else {
                std::cerr << "umask: unknown option '" << arg << "'\n";
                std::cerr << "Try 'umask --help' for more information.\n";
                return false;
            }
        }
        return true;
    }

    void PrintHelp() const {
        std::cout << R"(umask(1)                CrossShell for UNIX Reference Manual                 umask(1)

    NAME
        umask - get or set file mode creation mask for Windows / CrossShell

    SYNOPSIS
        umask [-S] [-p] [MASK] [-- COMMAND [ARG]...]

    DESCRIPTION
        The umask utility sets the file mode creation mask of the current process
        or prints the current mask in octal or symbolic format.

    OPTIONS
        -S
            Produce symbolic output (e.g. u=rwx,g=rx,o=rx).

        -p
            Output in a form that can be reused as input (e.g., umask 0022).

        -h, --help
            Display this reference manual.

    EXAMPLES
        umask
            Print current mask in octal format.

        umask -S
            Print current mask in symbolic notation.

        umask 027
            Set file creation mask to 027.

    CrossShell for UNIX                                                  umask(1)
)";
    }
};

class UmaskApplication {
public:
    int Run(int argc, char* argv[]) const {
        UmaskOptions options;
        if (!options.Parse(argc, argv)) {
            return 1;
        }

        if (options.show_help) {
            options.PrintHelp();
            return 0;
        }

        unsigned int current_mask = SystemUmaskManager::GetSystemUmask();

        if (!options.new_mask_arg.empty()) {
            try {
                unsigned int new_mask = UmaskParser::ParseMaskInput(current_mask, options.new_mask_arg);
                SystemUmaskManager::SetSystemUmask(new_mask);
                current_mask = new_mask;

                if (options.subcommand_str.empty()) {
                    std::cout << "umask: mask set to " << UmaskFormatter::FormatOctalMask(current_mask) 
                              << " (" << UmaskFormatter::FormatSymbolicMask(current_mask) << ")\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "umask: " << e.what() << "\n";
                return 1;
            }
        }

        if (!options.subcommand_str.empty()) {
            std::cout << "umask: running subcommand with mask " << UmaskFormatter::FormatOctalMask(current_mask) << "...\n";
            return SystemUmaskManager::ExecuteSubcommand(options.subcommand_str);
        }

        if (options.new_mask_arg.empty() || options.flag_print_reusable) {
            if (options.flag_print_reusable) {
                if (options.flag_symbolic) {
                    std::cout << "umask -S " << UmaskFormatter::FormatSymbolicMask(current_mask) << "\n";
                } else {
                    std::cout << "umask " << UmaskFormatter::FormatOctalMask(current_mask) << "\n";
                }
            } else if (options.flag_symbolic) {
                std::cout << UmaskFormatter::FormatSymbolicMask(current_mask) << "\n";
            } else {
                std::cout << UmaskFormatter::FormatOctalMask(current_mask) << "\n";
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    UmaskApplication app;
    return app.Run(argc, argv);
}
