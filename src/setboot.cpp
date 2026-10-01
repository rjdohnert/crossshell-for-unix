/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * SINGLE FILE INDEX: setboot.cpp
 * ============================================================================
 * WinSetboot - Object-Oriented UEFI NVRAM Boot Variable Manager for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & DATA STRUCTURES] ........... BootEntry, BootEnvironment, SetbootOptions
 * 2. [PRIVILEGE GUARD & NVRAM CONTROLLER] .. TokenPrivilegeGuard, UefiNvramController
 * 3. [BOOT ENVIRONMENT REPORTER] .......... SetbootReporter class
 * 4. [APPLICATION CONTROLLER] .............. SetbootApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cwctype>
#include <memory>

#pragma comment(lib, "Advapi32.lib")

// ============================================================================
// 1. OPTIONS & DATA STRUCTURES
// ============================================================================

struct BootEntry {
    WORD id{0};
    std::wstring name;         // e.g. Boot0001
    std::wstring description;  // e.g. Windows Boot Manager
    std::wstring devicePath;   // e.g. \EFI\Microsoft\Boot\bootmgfw.efi
    DWORD attributes{0};
    bool isValid{false};
};

struct BootEnvironment {
    std::wstring firmwareType{L"Unknown"};
    bool isUefi{false};
    WORD currentBootId{0};
    bool hasCurrentBoot{false};
    WORD nextBootId{0};
    bool hasNextBoot{false};
    WORD timeoutSeconds{0};
    bool hasTimeout{false};
    bool bootOrderReadOk{false};
    DWORD bootOrderReadError{ERROR_SUCCESS};
    std::vector<WORD> bootOrder;
    std::vector<BootEntry> bootEntries;
};

class SetbootOptions {
public:
    bool verbose{false};
    std::wstring setPrimary;
    std::wstring setAlternate;
    std::wstring setAutoboot;
    int setTimeout{-1};
    bool isModify{false};

    static void printHelp(const wchar_t* progName) {
        std::wcout << L"Boot Environment Parameter v3.1.0\n"
                   << L"Usage: " << progName << L" [-p id] [-a id] [-b on|off] [-t seconds] [-v] [-h]\n\n"
                   << L"OPTIONS:\n"
                   << L"  -p id          Set Primary Boot Path target in NVRAM BootOrder (e.g., Boot0001 or 1).\n"
                   << L"  -a id          Set Alternate / Next Boot Path target (sets NVRAM BootNext).\n"
                   << L"  -b on|off      Enable or disable Autoboot delay (sets timeout to 5s or 0s).\n"
                   << L"  -t seconds     Set Autoboot delay timeout in seconds.\n"
                   << L"  -v, --verbose  Display extended NVRAM variable attributes and EFI paths.\n"
                   << L"  -h, /?         Display this comprehensive help section.\n\n"
                   << L"PRIVILEGES:\n"
                   << L"  Modifying NVRAM variables (-p, -a, -b, -t) requires Administrator rights\n"
                   << L"  and SeSystemEnvironmentPrivilege enabled.\n\n"
                   << L"EXAMPLES:\n"
                   << L"  " << progName << L"\n"
                   << L"  " << progName << L" -v\n"
                   << L"  " << progName << L" -p Boot0001\n"
                   << L"  " << progName << L" -a Boot0002\n"
                   << L"  " << progName << L" -t 10\n"
                   << L"  " << progName << L" -b off\n";
    }

    static std::wstring toUpper(std::wstring str) {
        std::transform(str.begin(), str.end(), str.begin(), ::towupper);
        return str;
    }

    static bool parse(int argc, wchar_t* argv[], SetbootOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-p" && i + 1 < argc) {
                opts.setPrimary = argv[++i];
                opts.isModify = true;
            } else if (arg == L"-a" && i + 1 < argc) {
                opts.setAlternate = argv[++i];
                opts.isModify = true;
            } else if (arg == L"-b" && i + 1 < argc) {
                opts.setAutoboot = toUpper(argv[++i]);
                opts.isModify = true;
            } else if (arg == L"-t" && i + 1 < argc) {
                opts.setTimeout = _wtoi(argv[++i]);
                opts.isModify = true;
            } else {
                std::wcerr << L"Unknown option: " << arg << L"\nUse -h for help.\n";
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. PRIVILEGE GUARD & NVRAM CONTROLLER
// ============================================================================

class TokenPrivilegeGuard {
public:
    static bool enablePrivilege(const wchar_t* privName) {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }

        TOKEN_PRIVILEGES tp = { 0 };
        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, privName, &luid)) {
            CloseHandle(hToken);
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
        DWORD err = GetLastError();
        CloseHandle(hToken);

        return (result && err != ERROR_NOT_ALL_ASSIGNED);
    }
};

class UefiNvramController {
private:
    static inline const wchar_t* EFI_GLOBAL_GUID = L"{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}";

public:
    static BootEntry readBootOption(WORD id) {
        BootEntry entry;
        entry.id = id;

        wchar_t varName[16];
        swprintf_s(varName, 16, L"Boot%04X", id);
        entry.name = varName;

        BYTE buffer[2048] = { 0 };
        DWORD readBytes = GetFirmwareEnvironmentVariableW(
            varName,
            EFI_GLOBAL_GUID,
            buffer,
            sizeof(buffer)
        );

        if (readBytes < 6) {
            return entry;
        }

        entry.attributes = *reinterpret_cast<DWORD*>(buffer);
        const wchar_t* descPtr = reinterpret_cast<const wchar_t*>(buffer + 6);

        size_t maxDescChars = (readBytes - 6) / sizeof(wchar_t);
        size_t descLen = wcsnlen(descPtr, maxDescChars);
        entry.description = std::wstring(descPtr, descLen);
        entry.isValid = true;

        size_t descByteSize = (descLen + 1) * sizeof(wchar_t);
        size_t pathOffset = 6 + descByteSize;
        if (pathOffset < readBytes) {
            const BYTE* pathBytes = buffer + pathOffset;
            size_t pathSize = readBytes - pathOffset;

            std::wstring rawPath;
            for (size_t i = 0; i + 1 < pathSize; i += 2) {
                wchar_t ch = *reinterpret_cast<const wchar_t*>(pathBytes + i);
                if ((ch >= 32 && ch <= 126) || ch == L'\\' || ch == L'/') {
                    rawPath += ch;
                }
            }
            entry.devicePath = rawPath.empty() ? L"EFI Device Path" : rawPath;
        } else {
            entry.devicePath = L"EFI Device Path";
        }

        return entry;
    }

    static bool readWord(const wchar_t* varName, WORD& outVal) {
        WORD val = 0;
        DWORD bytes = GetFirmwareEnvironmentVariableW(
            varName,
            EFI_GLOBAL_GUID,
            &val,
            sizeof(WORD)
        );
        if (bytes == sizeof(WORD)) {
            outVal = val;
            return true;
        }
        return false;
    }

    static std::vector<WORD> readBootOrder(bool& readOk, DWORD& readError) {
        std::vector<WORD> order;
        readOk = false;
        readError = ERROR_SUCCESS;

        BYTE buffer[512] = { 0 };
        DWORD bytes = GetFirmwareEnvironmentVariableW(
            L"BootOrder",
            EFI_GLOBAL_GUID,
            buffer,
            sizeof(buffer)
        );

        if (bytes == 0) {
            readError = GetLastError();
            return order;
        }

        readOk = true;
        size_t count = bytes / sizeof(WORD);
        const WORD* pOrder = reinterpret_cast<const WORD*>(buffer);
        for (size_t i = 0; i < count; ++i) {
            order.push_back(pOrder[i]);
        }
        return order;
    }

    static BootEnvironment queryEnvironment() {
        BootEnvironment env;

        FIRMWARE_TYPE fwType;
        if (GetFirmwareType(&fwType)) {
            if (fwType == FirmwareTypeUefi) {
                env.firmwareType = L"UEFI (Unified Extensible Firmware Interface)";
                env.isUefi = true;
            } else if (fwType == FirmwareTypeBios) {
                env.firmwareType = L"Legacy BIOS";
                env.isUefi = false;
            }
        }

        if (!env.isUefi) return env;

        env.hasCurrentBoot = readWord(L"BootCurrent", env.currentBootId);
        env.hasNextBoot = readWord(L"BootNext", env.nextBootId);
        env.hasTimeout = readWord(L"Timeout", env.timeoutSeconds);
        env.bootOrder = readBootOrder(env.bootOrderReadOk, env.bootOrderReadError);

        for (WORD id : env.bootOrder) {
            BootEntry entry = readBootOption(id);
            if (entry.isValid) {
                env.bootEntries.push_back(entry);
            }
        }

        return env;
    }

    static bool writeBootOrder(const std::vector<WORD>& order) {
        DWORD byteSize = static_cast<DWORD>(order.size() * sizeof(WORD));
        return SetFirmwareEnvironmentVariableW(
            L"BootOrder",
            EFI_GLOBAL_GUID,
            const_cast<WORD*>(order.data()),
            byteSize
        ) != 0;
    }

    static bool writeBootNext(WORD bootId) {
        return SetFirmwareEnvironmentVariableW(
            L"BootNext",
            EFI_GLOBAL_GUID,
            &bootId,
            sizeof(WORD)
        ) != 0;
    }

    static bool writeTimeout(WORD seconds) {
        return SetFirmwareEnvironmentVariableW(
            L"Timeout",
            EFI_GLOBAL_GUID,
            &seconds,
            sizeof(WORD)
        ) != 0;
    }

    static bool parseBootId(const std::wstring& input, WORD& outId) {
        std::wstring str = SetbootOptions::toUpper(input);
        if (str.rfind(L"BOOT", 0) == 0) {
            str = str.substr(4);
        }
        try {
            outId = static_cast<WORD>(wcstoul(str.c_str(), nullptr, 16));
            return true;
        } catch (...) {
            return false;
        }
    }
};

// ============================================================================
// 3. BOOT ENVIRONMENT REPORTER
// ============================================================================

class SetbootReporter {
public:
    static void display(const BootEnvironment& env, bool verbose) {
        std::wcout << L"\nSystem Boot Parameters\n\n";
        std::wcout << L"Firmware Mode:          " << env.firmwareType << L"\n";

        if (!env.bootOrderReadOk) {
            std::wcerr << L"Warning: Unable to read UEFI BootOrder variable (Error: " << env.bootOrderReadError << L").\n";
            if (env.bootOrderReadError == ERROR_PRIVILEGE_NOT_HELD || env.bootOrderReadError == ERROR_ACCESS_DENIED) {
                std::wcerr << L"         Run an elevated Administrator terminal to query firmware NVRAM entries.\n";
            }
        }

        // Primary Boot Path
        if (!env.bootEntries.empty()) {
            const auto& primary = env.bootEntries[0];
            std::wcout << L"Primary Boot Path:      " << primary.name << L" [" << primary.description << L"]\n";
            if (verbose) {
                std::wcout << L"                        " << primary.devicePath << L"\n";
            }
        } else {
            std::wcout << L"Primary Boot Path:      None\n";
        }

        // Alternate Boot Path
        if (env.bootEntries.size() > 1) {
            const auto& alt = env.bootEntries[1];
            std::wcout << L"Alternate Boot Path:    " << alt.name << L" [" << alt.description << L"]\n";
            if (verbose) {
                std::wcout << L"                        " << alt.devicePath << L"\n";
            }
        } else {
            std::wcout << L"Alternate Boot Path:    None\n";
        }

        // Boot Next Target
        if (env.hasNextBoot) {
            std::wcout << L"Boot Next Target:       Boot" << std::setfill(L'0') << std::setw(4) << std::hex << env.nextBootId << std::dec << L"\n";
        } else {
            std::wcout << L"Boot Next Target:       None (Default Sequence)\n";
        }

        // Autoboot / Timeout Status
        std::wcout << L"Autoboot Status:        " << ((env.timeoutSeconds > 0) ? L"ENABLED" : L"DISABLED") << L"\n";
        std::wcout << L"Autoboot Timeout:       " << env.timeoutSeconds << L" seconds\n\n";

        // Boot Order Table
        std::wcout << L"Boot Order Sequence:\n";
        std::wcout << L"--------------------------------------------------------------------------------\n";

        for (size_t i = 0; i < env.bootEntries.size(); ++i) {
            const auto& entry = env.bootEntries[i];
            bool isCurrent = (env.hasCurrentBoot && entry.id == env.currentBootId);

            std::wcout << L"  " << std::setw(2) << (i + 1) << L". "
                      << entry.name << L" : "
                      << entry.description
                      << (isCurrent ? L" [Active Boot]" : L"") << L"\n";

            if (verbose) {
                std::wcout << L"      Path: " << entry.devicePath << L"\n"
                          << L"      Attr: 0x" << std::hex << entry.attributes << std::dec << L"\n";
            }
        }
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class SetbootApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        _setmode(_fileno(stdout), _O_U16TEXT);
        _setmode(_fileno(stderr), _O_U16TEXT);

        bool hasEnvPrivilege = TokenPrivilegeGuard::enablePrivilege(L"SeSystemEnvironmentPrivilege");

        SetbootOptions opts;
        if (!SetbootOptions::parse(argc, argv, opts)) {
            return 1;
        }

        BootEnvironment env = UefiNvramController::queryEnvironment();

        if (!env.isUefi) {
            std::wcerr << L"Error: setboot requires a UEFI system. Current system mode: " << env.firmwareType << L"\n";
            return 1;
        }

        if (opts.isModify) {
            if (!hasEnvPrivilege) {
                std::wcerr << L"Error: Administrator privileges required to modify UEFI NVRAM parameters.\n";
                return 1;
            }

            if (!opts.setPrimary.empty()) {
                WORD targetId = 0;
                if (UefiNvramController::parseBootId(opts.setPrimary, targetId)) {
                    std::vector<WORD> order = env.bootOrder;
                    order.erase(std::remove(order.begin(), order.end(), targetId), order.end());
                    order.insert(order.begin(), targetId);

                    if (UefiNvramController::writeBootOrder(order)) {
                        std::wcout << L"Successfully set Primary Boot Path to Boot"
                                  << std::setfill(L'0') << std::setw(4) << std::hex << targetId << std::dec << L"\n";
                    } else {
                        std::wcerr << L"Failed to update BootOrder in NVRAM. Error: " << GetLastError() << L"\n";
                    }
                }
            }

            if (!opts.setAlternate.empty()) {
                WORD targetId = 0;
                if (UefiNvramController::parseBootId(opts.setAlternate, targetId)) {
                    if (UefiNvramController::writeBootNext(targetId)) {
                        std::wcout << L"Successfully set Boot Next Target to Boot"
                                  << std::setfill(L'0') << std::setw(4) << std::hex << targetId << std::dec << L"\n";
                    } else {
                        std::wcerr << L"Failed to set BootNext in NVRAM. Error: " << GetLastError() << L"\n";
                    }
                }
            }

            if (opts.setTimeout >= 0) {
                if (UefiNvramController::writeTimeout(static_cast<WORD>(opts.setTimeout))) {
                    std::wcout << L"Successfully set Autoboot Timeout to " << opts.setTimeout << L" seconds.\n";
                } else {
                    std::wcerr << L"Failed to set Timeout in NVRAM. Error: " << GetLastError() << L"\n";
                }
            }

            if (!opts.setAutoboot.empty()) {
                WORD timeoutVal = (opts.setAutoboot == L"ON" || opts.setAutoboot == L"ENABLE") ? 5 : 0;
                if (UefiNvramController::writeTimeout(timeoutVal)) {
                    std::wcout << L"Successfully set Autoboot to " << opts.setAutoboot << L"\n";
                } else {
                    std::wcerr << L"Failed to update Autoboot status in NVRAM. Error: " << GetLastError() << L"\n";
                }
            }

            env = UefiNvramController::queryEnvironment();
            std::wcout << L"\n";
        }

        SetbootReporter::display(env, opts.verbose);
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return SetbootApp::run(argc, argv);
}