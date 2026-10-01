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
 * SINGLE FILE INDEX: dmesg.cpp
 * ============================================================================
 * WinDmesg - Object-Oriented Windows Event Log & Kernel Diagnostic Viewer
 * Specification: C++17 / POSIX dmesg-style | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & ANSI COLOR CONSOLE] ........ AnsiColor, DmesgOptions classes
 * 2. [STRUCTURED OUTPUT REPORTER] .......... DmesgReporter class (Colored Text, JSON, CSV, Pipe)
 * 3. [EVENT LOG READER & PARSER ENGINE] .... XmlHelper, EventLogReader, DmesgEngine classes
 * 4. [APPLICATION CONTROLLER] .............. DmesgApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winevt.h>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <locale>
#include <cwctype>
#include <cstdio>
#include <sstream>
#include <memory>

#pragma comment(lib, "wevtapi.lib")

// ============================================================================
// 1. OPTIONS & ANSI COLOR CONSOLE
// ============================================================================

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

class DmesgOptions {
public:
    bool useColor{true};
    int maxEventsToShow{50};
    int maxEventsToScan{1000};
    bool includeKernel{true};
    bool includeDriver{true};
    bool includeScm{true};
    int minLevel{5}; // 1=Critical ... 5=Verbose
    std::wstring providerContains;
    int outputFormat{0};
    std::string pipeCommand;

    static void printHelp() {
           std::cout << R"HELP(dmesg(1)                 CrossShell for UNIX Reference Manual                   dmesg(1)

    NAME
        dmesg - read recent Windows system events

    SYNOPSIS
        dmesg [OPTIONS]

    DESCRIPTION
        Reads recent Windows Event Log records and presents kernel, driver, and
        Service Control Manager events in a dmesg-like view.

    OPTIONS
        -n, --max-events COUNT       Maximum matching events to print (default: 50).
        -m, --max-scan COUNT         Maximum records to scan (default: 1000).
        -k, --kernel / -K            Include or exclude kernel providers.
        -d, --driver / -D            Include or exclude driver providers.
        -s, --scm / -S               Include or exclude Service Control Manager events.
        -p, --provider-contains TEXT Filter by provider substring.
        -l, --min-level LEVEL        critical, error, warning, info, or verbose.
        -c, --color / -C             Enable or disable ANSI colors.
        --json, --csv, --table       Select structured output.
        --pipe COMMAND               Send output through COMMAND.
        -h, --help                   Display this comprehensive reference manual.
        -V, --version                Display version information and exit.

    EXAMPLES
        dmesg
        dmesg -n 100 --min-level warning
        dmesg --provider-contains Kernel --json
        dmesg -u --table

    EXIT STATUS
        0          Help, version, or successful event-log rendering.
        1          Invalid options, event-log failure, or pipe failure.

    CrossShell for UNIX                                                         dmesg(1)
    )HELP";
           return;

        std::cout
            << "NAME\n"
            << "    dmesg - read recent Windows System events in a dmesg-like format\n\n"
            << "SYNOPSIS\n"
            << "    dmesg [options]\n\n"
            << "OPTIONS\n"
            << "    -n, --max-events <count>   maximum matching events to print (default: 50)\n"
            << "    -m, --max-scan <count>     maximum log records to scan (default: 1000)\n"
            << "    -k, --kernel / -K          include / exclude Microsoft-Windows-Kernel* providers\n"
            << "    -d, --driver / -D          include / exclude Microsoft-Windows-Driver* providers\n"
            << "    -s, --scm / -S             include / exclude Service Control Manager events\n"
            << "    -p, --provider-contains <s> filter by provider substring (case-insensitive)\n"
            << "    -l, --min-level <level>    filter by minimum severity (critical, error, warning, info, verbose)\n"
            << "    -c, --color / -C           enable / disable ANSI color formatting\n"
            << "        --json, --csv, --table structured output format\n"
            << "        --pipe COMMAND         send output through COMMAND\n"
            << "    -h, --help                 show this help text and exit\n"
            << "    -V, --version              show tool version and exit\n";
    }

    static void printVersion() {
        std::cout << "dmesg 1.1.0\n";
    }

    static std::string toLowerAscii(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

    static std::wstring toWide(const std::string& s) {
        std::wstring out;
        out.reserve(s.size());
        for (unsigned char c : s) out.push_back(static_cast<wchar_t>(c));
        return out;
    }

    static bool tryParsePositiveInt(const std::string& text, int& valueOut) {
        if (text.empty()) return false;
        try {
            size_t idx = 0;
            int value = std::stoi(text, &idx, 10);
            if (idx != text.size() || value <= 0) return false;
            valueOut = value;
            return true;
        } catch (...) {
            return false;
        }
    }

    static bool tryParseLevel(const std::string& text, int& levelOut) {
        const std::string lower = toLowerAscii(text);
        if (lower == "1" || lower == "critical") { levelOut = 1; return true; }
        if (lower == "2" || lower == "error") { levelOut = 2; return true; }
        if (lower == "3" || lower == "warning" || lower == "warn") { levelOut = 3; return true; }
        if (lower == "4" || lower == "info" || lower == "information") { levelOut = 4; return true; }
        if (lower == "5" || lower == "verbose" || lower == "debug") { levelOut = 5; return true; }
        return false;
    }

    static bool parse(int argc, char* argv[], DmesgOptions& options) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp();
                std::exit(0);
            }
            if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            }
            if (arg == "--json") { options.outputFormat = 1; continue; }
            if (arg == "--csv") { options.outputFormat = 2; continue; }
            if (arg == "--table") { options.outputFormat = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { options.pipeCommand = argv[++i]; continue; }
            if (arg == "-c" || arg == "--color") { options.useColor = true; continue; }
            if (arg == "-C" || arg == "--no-color") { options.useColor = false; continue; }
            if (arg == "-k" || arg == "--kernel") { options.includeKernel = true; continue; }
            if (arg == "-K" || arg == "--no-kernel") { options.includeKernel = false; continue; }
            if (arg == "-d" || arg == "--driver") { options.includeDriver = true; continue; }
            if (arg == "-D" || arg == "--no-driver") { options.includeDriver = false; continue; }
            if (arg == "-s" || arg == "--scm") { options.includeScm = true; continue; }
            if (arg == "-S" || arg == "--no-scm") { options.includeScm = false; continue; }

            if (arg == "-n" || arg == "--max-events") {
                if (i + 1 >= argc || !tryParsePositiveInt(argv[++i], options.maxEventsToShow)) {
                    std::cerr << "Invalid max-events value\n";
                    return false;
                }
                continue;
            }

            if (arg == "-m" || arg == "--max-scan") {
                if (i + 1 >= argc || !tryParsePositiveInt(argv[++i], options.maxEventsToScan)) {
                    std::cerr << "Invalid max-scan value\n";
                    return false;
                }
                continue;
            }

            if (arg == "-p" || arg == "--provider-contains") {
                if (i + 1 >= argc) {
                    std::cerr << "Missing value for " << arg << "\n";
                    return false;
                }
                options.providerContains = toWide(toLowerAscii(argv[++i]));
                continue;
            }

            if (arg == "-l" || arg == "--min-level") {
                if (i + 1 >= argc || !tryParseLevel(argv[++i], options.minLevel)) {
                    std::cerr << "Invalid min-level value\n";
                    return false;
                }
                continue;
            }

            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }

        if (!options.includeKernel && !options.includeDriver && !options.includeScm) {
            std::cerr << "All provider groups disabled (--no-kernel --no-driver --no-scm). Nothing to display.\n";
            return false;
        }
        return true;
    }
};

// ============================================================================
// 2. EVENT LOG READER & XML PARSER ENGINE
// ============================================================================

class XmlHelper {
public:
    static std::wstring extractAttribute(const std::wstring& xml, const std::wstring& anchor, const std::wstring& attribute) {
        const size_t anchorPos = xml.find(anchor);
        if (anchorPos == std::wstring::npos) return L"";

        const std::wstring token = attribute + L"='";
        const size_t attrPos = xml.find(token, anchorPos);
        if (attrPos == std::wstring::npos) return L"";

        const size_t valueStart = attrPos + token.size();
        const size_t valueEnd = xml.find(L"'", valueStart);
        if (valueEnd == std::wstring::npos) return L"";

        return xml.substr(valueStart, valueEnd - valueStart);
    }

    static std::wstring extractTagValue(const std::wstring& xml, const std::wstring& tag) {
        const std::wstring openTag = L"<" + tag;
        const size_t openPos = xml.find(openTag);
        if (openPos == std::wstring::npos) return L"";

        const size_t contentStart = xml.find(L">", openPos);
        if (contentStart == std::wstring::npos) return L"";

        const std::wstring closeTag = L"</" + tag + L">";
        const size_t contentEnd = xml.find(closeTag, contentStart + 1);
        if (contentEnd == std::wstring::npos) return L"";

        return xml.substr(contentStart + 1, contentEnd - (contentStart + 1));
    }

    static std::wstring extractFirstDataLine(const std::wstring& xml) {
        std::vector<std::wstring> parts;
        size_t cursor = 0;

        while (parts.size() < 3) {
            const size_t dataStart = xml.find(L"<Data", cursor);
            if (dataStart == std::wstring::npos) break;

            const size_t namePos = xml.find(L"Name='", dataStart);
            if (namePos == std::wstring::npos) break;
            const size_t nameStart = namePos + 6;
            const size_t nameEnd = xml.find(L"'", nameStart);
            if (nameEnd == std::wstring::npos) break;

            const size_t valueStart = xml.find(L">", nameEnd);
            if (valueStart == std::wstring::npos) break;
            const size_t valueEnd = xml.find(L"</Data>", valueStart + 1);
            if (valueEnd == std::wstring::npos) break;

            std::wstring key = xml.substr(nameStart, nameEnd - nameStart);
            std::wstring value = xml.substr(valueStart + 1, valueEnd - (valueStart + 1));
            if (!value.empty()) {
                parts.push_back(key + L"=" + value);
            }

            cursor = valueEnd + 7;
        }

        if (parts.empty()) return L"";

        std::wstring joined;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) joined += L" | ";
            joined += parts[i];
        }
        return joined;
    }

    static std::wstring formatSystemTime(std::wstring systemTime) {
        std::replace(systemTime.begin(), systemTime.end(), L'T', L' ');
        if (!systemTime.empty() && systemTime.back() == L'Z') {
            systemTime.pop_back();
        }
        return systemTime;
    }

    static std::wstring levelToText(const std::wstring& level) {
        if (level == L"1") return L"Critical";
        if (level == L"2") return L"Error";
        if (level == L"3") return L"Warning";
        if (level == L"4") return L"Info";
        if (level == L"5") return L"Verbose";
        return L"Unknown";
    }
};

class EventLogReader {
public:
    static void printKernelEvents(const DmesgOptions& options, bool colorsEnabled) {
        LPCWSTR channelPath = L"System";
        LPCWSTR xpathQuery = L"*";

        EVT_HANDLE hResults = EvtQuery(
            NULL,
            channelPath,
            xpathQuery,
            EvtQueryChannelPath | EvtQueryReverseDirection
        );

        if (NULL == hResults) {
            std::cerr << "EvtQuery failed with error: " << GetLastError() << std::endl;
            return;
        }

        EVT_HANDLE hEvent = NULL;
        DWORD dwReturned = 0;
        const DWORD maxEventsToShow = static_cast<DWORD>(options.maxEventsToShow);
        const DWORD maxEventsToScan = static_cast<DWORD>(options.maxEventsToScan);
        DWORD scannedCount = 0;
        DWORD shownCount = 0;

        auto color = [colorsEnabled](const wchar_t* c) -> const wchar_t* {
            return colorsEnabled ? c : L"";
        };

        std::wcout << color(AnsiColor::CLR_CYAN) << L"--- dmesg (Kernel/Driver Logs) ---" << color(AnsiColor::CLR_RESET) << L"\n\n";

        while (shownCount < maxEventsToShow && scannedCount < maxEventsToScan &&
               EvtNext(hResults, 1, &hEvent, INFINITE, 0, &dwReturned)) {
            DWORD dwBufferSize = 0;
            DWORD dwBufferUsed = 0;
            DWORD dwPropertyCount = 0;

            EvtRender(NULL, hEvent, EvtRenderEventXml, 0, NULL, &dwBufferUsed, &dwPropertyCount);

            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                dwBufferSize = dwBufferUsed;
                std::vector<wchar_t> buffer(dwBufferSize / sizeof(wchar_t));

                if (EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferSize, buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
                    std::wstring xml = buffer.data();
                    const bool isKernel = xml.find(L"Provider Name='Microsoft-Windows-Kernel") != std::wstring::npos;
                    const bool isDriver = xml.find(L"Provider Name='Microsoft-Windows-Driver") != std::wstring::npos;
                    const bool isScm = xml.find(L"Provider Name='Service Control Manager'") != std::wstring::npos;
                    const bool providerGroupMatch =
                        (options.includeKernel && isKernel) ||
                        (options.includeDriver && isDriver) ||
                        (options.includeScm && isScm);

                    if (providerGroupMatch) {
                        const std::wstring provider = XmlHelper::extractAttribute(xml, L"<Provider", L"Name");
                        const std::wstring eventId = XmlHelper::extractTagValue(xml, L"EventID");
                        const std::wstring level = XmlHelper::extractTagValue(xml, L"Level");
                        const std::wstring created = XmlHelper::extractAttribute(xml, L"<TimeCreated", L"SystemTime");
                        const std::wstring details = XmlHelper::extractFirstDataLine(xml);

                        int eventLevel = 5;
                        try {
                            if (!level.empty()) {
                                eventLevel = std::stoi(level);
                            }
                        } catch (...) {
                            eventLevel = 5;
                        }
                        if (eventLevel > options.minLevel) {
                            EvtClose(hEvent);
                            scannedCount++;
                            hEvent = NULL;
                            continue;
                        }

                        if (!options.providerContains.empty()) {
                            std::wstring providerLower = provider;
                            std::transform(providerLower.begin(), providerLower.end(), providerLower.begin(),
                                [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
                            if (providerLower.find(options.providerContains) == std::wstring::npos) {
                                EvtClose(hEvent);
                                scannedCount++;
                                hEvent = NULL;
                                continue;
                            }
                        }

                        const wchar_t* levelColor = AnsiColor::levelColor(level);
                        const wchar_t* providerColor = AnsiColor::providerColor(provider);

                        std::wcout << color(AnsiColor::CLR_DIM) << L"[" << XmlHelper::formatSystemTime(created) << L"] " << color(AnsiColor::CLR_RESET)
                                   << color(levelColor) << L"[" << XmlHelper::levelToText(level) << L"] " << color(AnsiColor::CLR_RESET)
                                   << color(providerColor) << provider << color(AnsiColor::CLR_RESET)
                                   << color(AnsiColor::CLR_DIM) << L" (EventID " << eventId << L")" << color(AnsiColor::CLR_RESET) << L"\n";
                        if (!details.empty()) {
                            std::wcout << color(AnsiColor::CLR_DIM) << L"  " << details << color(AnsiColor::CLR_RESET) << L"\n";
                        }
                        std::wcout << color(AnsiColor::CLR_DIM) << L"----------------------------------------" << color(AnsiColor::CLR_RESET) << L"\n";
                        shownCount++;
                    }
                }
            }

            EvtClose(hEvent);
            scannedCount++;
            hEvent = NULL;
        }

        if (shownCount == 0) {
            std::cout << "No kernel/driver events matched recent System log entries." << std::endl;
        }

        EvtClose(hResults);
    }
};

// ============================================================================
// 3. STRUCTURED OUTPUT DISPATCHER & ENGINE
// ============================================================================

class DmesgEngine {
private:
    DmesgOptions options;

    static std::string utf8FromWide(const std::wstring& value) {
        int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

public:
    explicit DmesgEngine(DmesgOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::wcout.imbue(std::locale(""));
        bool colorsEnabled = options.useColor && AnsiColor::enableVirtualTerminal();

        std::wostringstream captured;
        std::wstreambuf* oldOutput = nullptr;
        if (options.outputFormat || !options.pipeCommand.empty()) {
            oldOutput = std::wcout.rdbuf(captured.rdbuf());
        }

        EventLogReader::printKernelEvents(options, colorsEnabled);

        if (oldOutput) {
            std::wcout.rdbuf(oldOutput);
            std::wstring data = captured.str();
            std::wstring text = options.outputFormat == 1 ? L"{\"output\":\"" + data + L"\"}\n"
                              : options.outputFormat == 2 ? L"\"output\"\n\"" + data + L"\"\n"
                              : L"OUTPUT\n------\n" + data;

            if (!options.pipeCommand.empty()) {
                FILE* pipe = _popen(options.pipeCommand.c_str(), "w");
                if (!pipe) return 1;
                std::string narrow = utf8FromWide(text);
                std::fwrite(narrow.data(), 1, narrow.size(), pipe);
                _pclose(pipe);
            } else {
                std::wcout << text;
            }
        }
        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class DmesgApp {
public:
    static int run(int argc, char* argv[]) {
        DmesgOptions options;
        if (!DmesgOptions::parse(argc, argv, options)) {
            std::cerr << "Use --help to see available options.\n";
            return 1;
        }
        DmesgEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return DmesgApp::run(argc, argv);
}